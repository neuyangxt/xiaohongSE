#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"

#include "es8311.h"
#include "bsp_board_extra.h"
#include "ohos_board_i2c.h"
#include "ohos_audio_es8311_port.h"
#include "ohos_liteos_media_task.h"

#define OHOS_AUDIO_I2C_NUM              (0)
#define OHOS_AUDIO_I2C_SCL_IO           (GPIO_NUM_8)
#define OHOS_AUDIO_I2C_SDA_IO           (GPIO_NUM_7)
#define OHOS_AUDIO_PA_GPIO              (GPIO_NUM_53)

#define OHOS_AUDIO_I2S_NUM              (0)
#define OHOS_AUDIO_I2S_MCK_IO           (GPIO_NUM_13)
#define OHOS_AUDIO_I2S_BCK_IO           (GPIO_NUM_12)
#define OHOS_AUDIO_I2S_WS_IO            (GPIO_NUM_10)
#define OHOS_AUDIO_I2S_DO_IO            (GPIO_NUM_9)
#define OHOS_AUDIO_I2S_DI_IO            (GPIO_NUM_11)

#define OHOS_AUDIO_SAMPLE_RATE          (16000)
#define OHOS_AUDIO_MCLK_MULTIPLE        (384)
#define OHOS_AUDIO_MCLK_FREQ_HZ         (OHOS_AUDIO_SAMPLE_RATE * OHOS_AUDIO_MCLK_MULTIPLE)
#define OHOS_AUDIO_RECV_BUF_SIZE        (2400)
#define OHOS_AUDIO_PCM_MONO_CHUNK       (3840)
#define OHOS_AUDIO_VOICE_VOLUME         (60)
#define OHOS_AUDIO_TASK_PRIO            25
#define OHOS_AUDIO_TASK_STACK           0x2000

static const char *TAG = "ohos_audio_es8311";

static i2s_chan_handle_t g_audio_tx_handle = NULL;
static i2s_chan_handle_t g_audio_rx_handle = NULL;
static es8311_handle_t g_es8311_handle = NULL;
static i2c_master_bus_handle_t g_audio_i2c_bus_handle = NULL;

static volatile uint32_t g_es8311_play_stop_req = 0;
static volatile uint32_t g_audio_hw_started = 0;
static volatile uint32_t g_audio_record_task_started = 0;
static volatile uint32_t g_audio_combined_task_started = 0;
static volatile uint32_t g_audio_tx_enabled = 0;
static volatile uint32_t g_audio_pcm_play_count = 0;
static volatile uint32_t g_audio_bsp_dialog_ready = 0;
static volatile uint32_t g_audio_bsp_dialog_write_ok = 0;
static volatile uint32_t g_audio_bsp_dialog_write_fail = 0;
static volatile uint32_t g_audio_codec_recover_count = 0;
static int16_t g_audio_pcm_stereo_chunk[OHOS_AUDIO_PCM_MONO_CHUNK * 2U];

extern const uint8_t audio_canon_pcm_start[] asm("_binary_canon_pcm_start");
extern const uint8_t audio_canon_pcm_end[]   asm("_binary_canon_pcm_end");

static uint32_t OhosAudioPaEnable(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << OHOS_AUDIO_PA_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PA gpio_config failed: %s", esp_err_to_name(ret));
        return 1;
    }

    gpio_set_level(OHOS_AUDIO_PA_GPIO, 1);
    ESP_LOGI(TAG, "PA enabled gpio=%d", (int)OHOS_AUDIO_PA_GPIO);
    return 0;
}

static uint32_t OhosAudioI2cInit(void)
{
    if (g_audio_i2c_bus_handle != NULL) {
        ESP_LOGI(TAG, "I2C master bus already initialized");
        return 0;
    }
    esp_err_t ret = OhosBoardI2cGetSharedBus(&g_audio_i2c_bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "get shared I2C bus failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C shared bus ready port=%d scl=%d sda=%d",
             OHOS_AUDIO_I2C_NUM,
             (int)OHOS_AUDIO_I2C_SCL_IO,
             (int)OHOS_AUDIO_I2C_SDA_IO);
    return 0;
}

static uint32_t OhosAudioI2sInit(void)
{
    if (g_audio_tx_handle != NULL || g_audio_rx_handle != NULL) {
        ESP_LOGI(TAG, "I2S already initialized");
        return 0;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(OHOS_AUDIO_I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    esp_err_t ret = i2s_new_channel(&chan_cfg, &g_audio_tx_handle, &g_audio_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s_new_channel failed: %s", esp_err_to_name(ret));
        return 1;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(OHOS_AUDIO_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = OHOS_AUDIO_I2S_MCK_IO,
            .bclk = OHOS_AUDIO_I2S_BCK_IO,
            .ws = OHOS_AUDIO_I2S_WS_IO,
            .dout = OHOS_AUDIO_I2S_DO_IO,
            .din = OHOS_AUDIO_I2S_DI_IO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    std_cfg.clk_cfg.mclk_multiple = OHOS_AUDIO_MCLK_MULTIPLE;

    ret = i2s_channel_init_std_mode(g_audio_tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s tx init std failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = i2s_channel_init_std_mode(g_audio_rx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s rx init std failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = i2s_channel_enable(g_audio_tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s tx enable failed: %s", esp_err_to_name(ret));
        return 1;
    }
    g_audio_tx_enabled = 1;

    ret = i2s_channel_enable(g_audio_rx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s rx enable failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ESP_LOGI(TAG, "I2S ready mclk=%d bclk=%d ws=%d dout=%d din=%d",
             (int)OHOS_AUDIO_I2S_MCK_IO,
             (int)OHOS_AUDIO_I2S_BCK_IO,
             (int)OHOS_AUDIO_I2S_WS_IO,
             (int)OHOS_AUDIO_I2S_DO_IO,
             (int)OHOS_AUDIO_I2S_DI_IO);
    return 0;
}

static uint32_t OhosAudioCodecInit(void)
{
    if (g_es8311_handle != NULL) {
        ESP_LOGI(TAG, "ES8311 already initialized");
        return 0;
    }

    g_es8311_handle = es8311_create(g_audio_i2c_bus_handle, ES8311_ADDRRES_0);
    if (g_es8311_handle == NULL) {
        ESP_LOGE(TAG, "es8311_create failed");
        return 1;
    }

    const es8311_clock_config_t es_clk = {
        .mclk_inverted = false,
        .sclk_inverted = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency = OHOS_AUDIO_MCLK_FREQ_HZ,
        .sample_frequency = OHOS_AUDIO_SAMPLE_RATE,
    };

    esp_err_t ret = es8311_init(g_es8311_handle, &es_clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "es8311_init failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = es8311_sample_frequency_config(g_es8311_handle,
                                         OHOS_AUDIO_SAMPLE_RATE * OHOS_AUDIO_MCLK_MULTIPLE,
                                         OHOS_AUDIO_SAMPLE_RATE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "es8311_sample_frequency_config failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = es8311_voice_volume_set(g_es8311_handle, OHOS_AUDIO_VOICE_VOLUME, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "es8311_voice_volume_set failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = es8311_microphone_config(g_es8311_handle, false);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "es8311_microphone_config failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = es8311_microphone_gain_set(g_es8311_handle, 3);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "es8311_microphone_gain_set warn: %s", esp_err_to_name(ret));
    }

    ESP_LOGI(TAG, "ES8311 codec ready volume=%d sample_rate=%d", OHOS_AUDIO_VOICE_VOLUME, OHOS_AUDIO_SAMPLE_RATE);
    return 0;
}

static uint32_t OhosAudioCodecPrepareOutput(void)
{
    esp_err_t ret;

    if (g_es8311_handle == NULL) {
        return OhosAudioCodecInit();
    }

    ret = es8311_sample_frequency_config(g_es8311_handle,
                                         OHOS_AUDIO_SAMPLE_RATE * OHOS_AUDIO_MCLK_MULTIPLE,
                                         OHOS_AUDIO_SAMPLE_RATE);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ES8311 sample freq recover failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = es8311_voice_volume_set(g_es8311_handle, OHOS_AUDIO_VOICE_VOLUME, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ES8311 volume recover failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = es8311_voice_mute(g_es8311_handle, false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ES8311 unmute recover failed: %s", esp_err_to_name(ret));
        return 1;
    }

    g_audio_codec_recover_count++;
    if (g_audio_codec_recover_count <= 5U ||
        (g_audio_codec_recover_count % 50U) == 0U) {
        ESP_LOGI(TAG, "ES8311 output prepared count=%u txEnabled=%u",
                 (unsigned)g_audio_codec_recover_count,
                 (unsigned)g_audio_tx_enabled);
    }
    return 0;
}

uint32_t OhosAudioEs8311PortStartRealHw(void)
{
    if (g_audio_hw_started) {
        return 0;
    }

    ESP_LOGI(TAG, "start ES8311 real audio hw");

    if (OhosAudioPaEnable() != 0) {
        return 1;
    }

    if (OhosAudioI2sInit() != 0) {
        return 1;
    }

    if (OhosAudioI2cInit() != 0) {
        return 1;
    }

    if (OhosAudioCodecInit() != 0) {
        return 1;
    }

    if (OhosAudioCodecPrepareOutput() != 0) {
        return 1;
    }

    g_audio_hw_started = 1;
    ESP_LOGI(TAG, "ES8311 real audio hw start done");
    return 0;
}

uint32_t OhosAudioEs8311PortPrepareForDownlink(uint32_t settleMs)
{
    if (OhosAudioEs8311PortStartRealHw() != 0) {
        return 1;
    }

    if (OhosAudioCodecPrepareOutput() != 0) {
        return 1;
    }

    if (g_audio_tx_handle != NULL && !g_audio_tx_enabled) {
        esp_err_t ret = i2s_channel_enable(g_audio_tx_handle);
        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
            g_audio_tx_enabled = 1;
        } else {
            ESP_LOGE(TAG, "prepare downlink tx enable failed: %s", esp_err_to_name(ret));
            return 1;
        }
    }

    if (settleMs > 0U) {
        OhosLiteosDelayMs(settleMs);
    }

    ESP_LOGI(TAG, "prepare downlink output ok settle=%u txEnabled=%u",
             (unsigned)settleMs,
             (unsigned)g_audio_tx_enabled);
    return 0;
}

uint32_t OhosAudioEs8311PortPrepareForDialog(uint32_t settleMs)
{
    esp_err_t ret = bsp_extra_codec_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "dialog bsp codec init failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = bsp_extra_codec_set_fs(OHOS_AUDIO_SAMPLE_RATE,
                                 16,
                                 I2S_SLOT_MODE_MONO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "dialog bsp codec set fs failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = bsp_extra_codec_volume_set(OHOS_AUDIO_VOICE_VOLUME, NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "dialog bsp volume set failed: %s", esp_err_to_name(ret));
        return 1;
    }

    ret = bsp_extra_codec_mute_set(false);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "dialog bsp unmute failed: %s", esp_err_to_name(ret));
        return 1;
    }

    if (g_audio_tx_handle != NULL && !g_audio_tx_enabled) {
        ret = i2s_channel_enable(g_audio_tx_handle);
        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
            g_audio_tx_enabled = 1;
        } else {
            ESP_LOGW(TAG, "dialog tx enable recover failed: %s", esp_err_to_name(ret));
        }
    }

    g_es8311_play_stop_req = 0;
    g_audio_bsp_dialog_ready = 1;
    if (settleMs > 0U) {
        OhosLiteosDelayMs(settleMs);
    }

    ESP_LOGI(TAG, "dialog bsp output prepared settle=%u volume=%u",
             (unsigned)settleMs,
             (unsigned)OHOS_AUDIO_VOICE_VOLUME);
    return 0;
}

uint32_t OhosAudioEs8311PortPlayPcm16MonoDialog(const int16_t *pcm, uint32_t samples)
{
    uint32_t offset = 0;
    size_t total_written = 0;

    if (pcm == NULL || samples == 0U) {
        ESP_LOGW(TAG, "dialog pcm16 mono invalid pcm=%p samples=%u", pcm, (unsigned)samples);
        return 1;
    }

    if (!g_audio_bsp_dialog_ready) {
        if (OhosAudioEs8311PortPrepareForDialog(0) != 0U) {
            return OhosAudioEs8311PortPlayPcm16Mono(pcm, samples);
        }
    }

    while (offset < samples && !g_es8311_play_stop_req) {
        uint32_t chunk_samples = samples - offset;
        size_t bytes_written = 0;
        esp_err_t ret;

        if (chunk_samples > OHOS_AUDIO_PCM_MONO_CHUNK) {
            chunk_samples = OHOS_AUDIO_PCM_MONO_CHUNK;
        }

        uint32_t start_ms = (uint32_t)esp_log_timestamp();
        ret = bsp_extra_i2s_write((void *)&pcm[offset],
                                  chunk_samples * sizeof(int16_t),
                                  &bytes_written,
                                  1000);
        uint32_t cost_ms = (uint32_t)esp_log_timestamp() - start_ms;
        if (ret != ESP_OK) {
            if (g_es8311_play_stop_req) {
                ESP_LOGI(TAG, "dialog bsp write interrupted by stop ret=%s", esp_err_to_name(ret));
                break;
            }
            g_audio_bsp_dialog_write_fail++;
            ESP_LOGE(TAG,
                     "dialog bsp write failed ret=%s samples=%u offset=%u ok=%u fail=%u, fallback direct",
                     esp_err_to_name(ret),
                     (unsigned)samples,
                     (unsigned)offset,
                     (unsigned)g_audio_bsp_dialog_write_ok,
                     (unsigned)g_audio_bsp_dialog_write_fail);
            return OhosAudioEs8311PortPlayPcm16Mono(&pcm[offset], samples - offset);
        }

        total_written += bytes_written;
        offset += chunk_samples;

        if (bytes_written == 0U) {
            OhosLiteosDelayMs(1);
        }

        if (cost_ms > 350U ||
            g_audio_bsp_dialog_write_ok < 3U ||
            ((g_audio_bsp_dialog_write_ok + 1U) % 50U) == 0U) {
            ESP_LOGI(TAG,
                     "dialog bsp mono write samples=%u bytes=%u cost=%u expected=%u",
                     (unsigned)chunk_samples,
                     (unsigned)bytes_written,
                     (unsigned)cost_ms,
                     (unsigned)((chunk_samples * 1000U) / OHOS_AUDIO_SAMPLE_RATE));
        }
    }

    if (g_es8311_play_stop_req) {
        ESP_LOGI(TAG, "dialog bsp pcm16 play stopped samples=%u bytes=%u",
                 (unsigned)samples,
                 (unsigned)total_written);
        g_es8311_play_stop_req = 0;
        return 0;
    }

    g_audio_bsp_dialog_write_ok++;
    if (g_audio_bsp_dialog_write_ok <= 3U ||
        (g_audio_bsp_dialog_write_ok % 100U) == 0U) {
        ESP_LOGI(TAG,
                 "dialog bsp pcm16 play done count=%u samples=%u bytes=%u",
                 (unsigned)g_audio_bsp_dialog_write_ok,
                 (unsigned)samples,
                 (unsigned)total_written);
    }
    return 0;
}

uint32_t OhosAudioEs8311PortPlayPcm16Mono(const int16_t *pcm, uint32_t samples)
{
    uint32_t offset = 0;
    size_t total_written = 0;

    if (pcm == NULL || samples == 0U) {
        ESP_LOGW(TAG, "pcm16 mono play invalid pcm=%p samples=%u", pcm, (unsigned)samples);
        return 1;
    }

    if (OhosAudioEs8311PortStartRealHw() != 0) {
        return 1;
    }

    g_es8311_play_stop_req = 0;

    esp_err_t ret = ESP_OK;
    if (!g_audio_tx_enabled) {
        ret = i2s_channel_enable(g_audio_tx_handle);
        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
            g_audio_tx_enabled = 1;
            if (ret == ESP_ERR_INVALID_STATE &&
                (g_audio_pcm_play_count < 5U || ((g_audio_pcm_play_count + 1U) % 20U) == 0U)) {
                ESP_LOGW(TAG, "pcm16 mono tx state repaired as enabled count=%u",
                         (unsigned)(g_audio_pcm_play_count + 1U));
            }
        } else {
            ESP_LOGE(TAG, "pcm16 mono tx enable failed: %s", esp_err_to_name(ret));
            return 1;
        }
    }

    while (offset < samples && !g_es8311_play_stop_req) {
        uint32_t chunk_samples = samples - offset;
        size_t bytes_write = 0;

        if (chunk_samples > OHOS_AUDIO_PCM_MONO_CHUNK) {
            chunk_samples = OHOS_AUDIO_PCM_MONO_CHUNK;
        }

        for (uint32_t i = 0; i < chunk_samples; ++i) {
            int16_t v = pcm[offset + i];
            g_audio_pcm_stereo_chunk[(i * 2U)] = v;
            g_audio_pcm_stereo_chunk[(i * 2U) + 1U] = v;
        }

        ret = i2s_channel_write(g_audio_tx_handle,
                                g_audio_pcm_stereo_chunk,
                                chunk_samples * 2U * sizeof(int16_t),
                                &bytes_write,
                                100);
        if (ret != ESP_OK) {
            if (g_es8311_play_stop_req) {
                break;
            }
            ESP_LOGE(TAG, "pcm16 mono i2s write failed: %s", esp_err_to_name(ret));
            return 1;
        }

        total_written += bytes_write;
        offset += chunk_samples;

        if (bytes_write == 0U) {
            OhosLiteosDelayMs(1);
        }
    }

    if (g_es8311_play_stop_req) {
        ESP_LOGI(TAG, "pcm16 mono play stopped samples=%u bytes=%u",
                 (unsigned)samples,
                 (unsigned)total_written);
        g_es8311_play_stop_req = 0;
        return 0;
    }

    g_audio_pcm_play_count++;
    if (g_audio_pcm_play_count <= 3U || (g_audio_pcm_play_count % 100U) == 0U) {
        ESP_LOGI(TAG, "pcm16 mono play done count=%u samples=%u bytes=%u",
                 (unsigned)g_audio_pcm_play_count,
                 (unsigned)samples,
                 (unsigned)total_written);
    }
    return 0;
}

uint32_t OhosAudioEs8311PortPlayOnce(void)
{
    if (OhosAudioEs8311PortStartRealHw() != 0) {
        return 1;
    }

    g_es8311_play_stop_req = 0;

    size_t bytes_write = 0;
    uint8_t *data_ptr = (uint8_t *)audio_canon_pcm_start;
    size_t total_size = (size_t)(audio_canon_pcm_end - audio_canon_pcm_start);

    ESP_LOGI(TAG, "speaker output test begin pcm_size=%u", (unsigned)total_size);

    esp_err_t ret = i2s_channel_disable(g_audio_tx_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "tx disable before preload warn: %s", esp_err_to_name(ret));
    }
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
        g_audio_tx_enabled = 0;
    }

    ret = i2s_channel_preload_data(g_audio_tx_handle, data_ptr, total_size, &bytes_write);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2s preload failed: %s", esp_err_to_name(ret));
        return 1;
    }

    data_ptr += bytes_write;

    if (g_es8311_play_stop_req) {
        ESP_LOGI(TAG, "S70C speaker stop after preload");
        (void)i2s_channel_disable(g_audio_tx_handle);
        g_audio_tx_enabled = 0;
        return 0;
    }

    ret = i2s_channel_enable(g_audio_tx_handle);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "tx enable after preload failed: %s", esp_err_to_name(ret));
        return 1;
    }
    g_audio_tx_enabled = 1;

    size_t total_written = bytes_write;
    size_t remain = (size_t)(audio_canon_pcm_end - data_ptr);
    while (remain > 0 && !g_es8311_play_stop_req) {
        size_t chunk = remain;
        if (chunk > 1024) {
            chunk = 1024;
        }

        bytes_write = 0;
        ret = i2s_channel_write(g_audio_tx_handle, data_ptr, chunk, &bytes_write, 1000);
        if (ret != ESP_OK) {
            if (g_es8311_play_stop_req) {
                ESP_LOGI(TAG, "S70C speaker write interrupted by stop ret=%s", esp_err_to_name(ret));
                break;
            }
            ESP_LOGE(TAG, "i2s music write failed: %s", esp_err_to_name(ret));
            return 1;
        }

        data_ptr += bytes_write;
        remain -= bytes_write;
        total_written += bytes_write;

        if (bytes_write == 0) {
            OhosLiteosDelayMs(10);
        }
    }

    if (g_es8311_play_stop_req) {
        ESP_LOGI(TAG, "S70C speaker output stopped written=%u", (unsigned)total_written);
        (void)i2s_channel_disable(g_audio_tx_handle);
        g_audio_tx_enabled = 0;
        g_es8311_play_stop_req = 0;
        return 0;
    }

    ESP_LOGI(TAG, "music write done bytes=%u", (unsigned)total_written);

    OhosLiteosDelayMs(100);

    /*
     * Keep TX enabled for RX clock.
     */
    ESP_LOGI(TAG, "speaker output test done, keep TX enabled for RX clock");
    return 0;
}


uint32_t OhosAudioEs8311PortStopPlay(void)
{
    g_es8311_play_stop_req = 1;

    ESP_LOGI(TAG, "S70C speaker stop requested");

    if (g_audio_tx_handle != NULL) {
        esp_err_t ret = i2s_channel_disable(g_audio_tx_handle);
        if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
            g_audio_tx_enabled = 0;
        }
        ESP_LOGI(TAG, "S70C speaker tx disable ret=%s", esp_err_to_name(ret));
    }

    return 0;
}

static void OhosAudioRecordStatsTask(void *args)
{
    (void)args;

    int16_t *mic_data = malloc(OHOS_AUDIO_RECV_BUF_SIZE);
    if (mic_data == NULL) {
        ESP_LOGE(TAG, "record stats no memory");
        g_audio_record_task_started = 0;
        return;
    }

    ESP_LOGI(TAG, "stereo record stats task start");

    /*
     * Re-enable ES8311 microphone path before recording.
     * In the all-media build, playback is executed before record stats.
     * Re-applying MIC/ADC configuration here helps avoid silent RX samples.
     */
    esp_err_t mic_ret = es8311_microphone_config(g_es8311_handle, false);
    ESP_LOGI(TAG, "mic config before record ret=%d", (int)mic_ret);

    mic_ret = es8311_microphone_gain_set(g_es8311_handle, 3);
    ESP_LOGI(TAG, "mic gain before record ret=%d", (int)mic_ret);

    /*
     * Restart RX before recording.
     * In the all-media build, TX playback finishes before RX statistics.
     * Re-enabling RX here helps the I2S RX side resync with ES8311 ADC output.
     */
    esp_err_t rx_ret = i2s_channel_disable(g_audio_rx_handle);
    ESP_LOGI(TAG, "rx disable before record ret=%d", (int)rx_ret);
    OhosLiteosDelayMs(20);

    rx_ret = i2s_channel_enable(g_audio_rx_handle);
    ESP_LOGI(TAG, "rx enable before record ret=%d", (int)rx_ret);
    OhosLiteosDelayMs(20);

    uint32_t seq = 0;

    while (1) {
        size_t bytes_read = 0;
        memset(mic_data, 0, OHOS_AUDIO_RECV_BUF_SIZE);

        esp_err_t ret = i2s_channel_read(g_audio_rx_handle,
                                         mic_data,
                                         OHOS_AUDIO_RECV_BUF_SIZE,
                                         &bytes_read,
                                         1000);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "i2s read failed: %s, continue", esp_err_to_name(ret));
            OhosLiteosDelayMs(20);
            continue;
        }

        size_t sample_count = bytes_read / sizeof(int16_t);

        if (seq == 0U && sample_count >= 8) {
            ESP_LOGI(TAG,
                     "record raw first samples: %d %d %d %d %d %d %d %d",
                     mic_data[0],
                     mic_data[1],
                     mic_data[2],
                     mic_data[3],
                     mic_data[4],
                     mic_data[5],
                     mic_data[6],
                     mic_data[7]);
        }

        uint64_t sum_l = 0;
        uint64_t sum_r = 0;
        int32_t peak_l = 0;
        int32_t peak_r = 0;
        size_t cnt_l = 0;
        size_t cnt_r = 0;

        for (size_t i = 0; i + 1 < sample_count; i += 2) {
            int32_t vl = mic_data[i];
            int32_t vr = mic_data[i + 1];

            int32_t al = (vl < 0) ? -vl : vl;
            int32_t ar = (vr < 0) ? -vr : vr;

            sum_l += (uint32_t)al;
            sum_r += (uint32_t)ar;

            if (al > peak_l) {
                peak_l = al;
            }
            if (ar > peak_r) {
                peak_r = ar;
            }

            cnt_l++;
            cnt_r++;
        }

        uint32_t mean_l = cnt_l ? (uint32_t)(sum_l / cnt_l) : 0;
        uint32_t mean_r = cnt_r ? (uint32_t)(sum_r / cnt_r) : 0;

        if ((seq++ % 10U) == 0U) {
            ESP_LOGI(TAG,
                     "record stats bytes=%u samples=%u L_mean=%u L_peak=%ld R_mean=%u R_peak=%ld",
                     (unsigned)bytes_read,
                     (unsigned)sample_count,
                     (unsigned)mean_l,
                     (long)peak_l,
                     (unsigned)mean_r,
                     (long)peak_r);
        }

        OhosLiteosDelayMs(20);
    }
}

uint32_t OhosAudioEs8311PortStartRecordStatsTask(void)
{
    if (OhosAudioEs8311PortStartRealHw() != 0) {
        return 1;
    }

    if (g_audio_record_task_started) {
        ESP_LOGI(TAG, "record stats task already started");
        return 0;
    }

    UINT32 ret = OhosLiteosCreateTask("ohos_audio_record",
                                      OhosAudioRecordStatsTask,
                                      NULL,
                                      OHOS_AUDIO_TASK_PRIO,
                                      OHOS_AUDIO_TASK_STACK,
                                      NULL);

    ESP_LOGI(TAG, "record stats LiteOS task create ret=%u", (unsigned)ret);

    if (ret != LOS_OK) {
        return 1;
    }

    g_audio_record_task_started = 1;
    return 0;
}

static void OhosAudioCombinedTask(void *args)
{
    (void)args;

    ESP_LOGI(TAG, "combined audio validation task start");

    if (OhosAudioEs8311PortStartRealHw() != 0) {
        ESP_LOGE(TAG, "combined start hw failed");
        g_audio_combined_task_started = 0;
        return;
    }

    if (OhosAudioEs8311PortPlayOnce() != 0) {
        ESP_LOGE(TAG, "combined play failed");
        g_audio_combined_task_started = 0;
        return;
    }

    ESP_LOGI(TAG, "combined switch to record stats");
    OhosAudioRecordStatsTask(NULL);
}

uint32_t OhosAudioEs8311PortCombinedSelfTest(void)
{
    if (g_audio_combined_task_started) {
        ESP_LOGI(TAG, "combined audio task already started");
        return 0;
    }

    UINT32 ret = OhosLiteosCreateTask("ohos_audio_combined",
                                      OhosAudioCombinedTask,
                                      NULL,
                                      OHOS_AUDIO_TASK_PRIO,
                                      OHOS_AUDIO_TASK_STACK,
                                      NULL);

    ESP_LOGI(TAG, "combined audio LiteOS task create ret=%u", (unsigned)ret);

    if (ret != LOS_OK) {
        return 1;
    }

    g_audio_combined_task_started = 1;
    return 0;
}
