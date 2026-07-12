#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/i2s_std.h"
#include "los_task.h"

#include "bsp_board_extra.h"
#include "ohos_liteos_media_task.h"
#include "ohos_audio_es7210_port.h"

#ifndef LOS_OK
#define LOS_OK 0U
#endif

extern UINT32 LOS_IntLock(void);
extern void LOS_IntRestore(UINT32 intSave);

#define OHOS_ES7210_TASK_PRIO   25
#define OHOS_ES7210_TASK_STACK  0x2000
#define OHOS_ES7210_MONO_FRAME_SAMPLES 640U
#define OHOS_ES7210_MONO_FRAME_BYTES (OHOS_ES7210_MONO_FRAME_SAMPLES * sizeof(int16_t))
#define OHOS_ES7210_STEREO_FRAME_BYTES (OHOS_ES7210_MONO_FRAME_SAMPLES * 2U * sizeof(int16_t))
#define OHOS_ES7210_ACCUM_READ_SLICE_MS 20U
#define OHOS_ES7210_UPLINK_VOICE_GAIN 6
#define OHOS_ES7210_COMMAND_VOICE_GAIN 1
#define OHOS_ES7210_UPLINK_NOISE_GATE 8
#define OHOS_ES7210_UPLINK_WARMUP_DROP_FRAMES 2U
#define OHOS_ES7210_UPLINK_CAPTURE_ENABLE 1
#define OHOS_ES7210_UPLINK_CAPTURE_TASK_PRIO 19
#define OHOS_ES7210_UPLINK_CAPTURE_TASK_STACK 0x5000
#define OHOS_ES7210_UPLINK_CAPTURE_RING_SAMPLES 32000U
#define OHOS_ES7210_UPLINK_CAPTURE_FALLBACK_SAMPLES 4096U
#define OHOS_ES7210_UPLINK_CAPTURE_CHUNK_MS 20U
#define OHOS_ES7210_UPLINK_CAPTURE_CHUNK_SAMPLES \
    ((16000U * OHOS_ES7210_UPLINK_CAPTURE_CHUNK_MS) / 1000U)
#define OHOS_ES7210_UPLINK_CAPTURE_STEREO_BYTES \
    (OHOS_ES7210_UPLINK_CAPTURE_CHUNK_SAMPLES * 2U * sizeof(int16_t))
#define OHOS_ES7210_UPLINK_CAPTURE_START_LEVEL 1280U
#define OHOS_ES7210_UPLINK_CAPTURE_HIGH_WATER 24000U
#define OHOS_ES7210_UPLINK_CAPTURE_WAIT_STEP_MS 5U

static const char *TAG = "ohos_audio_es7210";
static volatile uint32_t g_es7210_record_task_started = 0;
static volatile uint32_t g_es7210_record_stop_req = 0;
static volatile uint32_t g_es7210_record_seq = 0;
static volatile uint32_t g_es7210_record_bytes_read = 0;
static volatile uint32_t g_es7210_record_samples = 0;
static volatile uint32_t g_es7210_record_peak = 0;
static volatile uint32_t g_es7210_record_nonzero = 0;
static volatile uint32_t g_es7210_record_last_ret = 0xffffffffU;
static volatile int32_t g_es7210_record_first0 = 0;
static volatile int32_t g_es7210_record_first1 = 0;
static volatile int32_t g_es7210_record_first2 = 0;
static volatile int32_t g_es7210_record_first3 = 0;
static uint8_t g_es7210_pcm_raw[OHOS_ES7210_STEREO_FRAME_BYTES];
static volatile uint32_t g_es7210_pcm_init_ok = 0;
static volatile uint32_t g_es7210_pcm_read_ok = 0;
static volatile uint32_t g_es7210_pcm_read_fail = 0;
static volatile uint32_t g_es7210_pcm_bytes_read = 0;
static volatile uint32_t g_es7210_pcm_samples = 0;
static volatile uint32_t g_es7210_pcm_peak = 0;
static volatile uint32_t g_es7210_pcm_nonzero = 0;
static volatile uint32_t g_es7210_pcm_use_right = 0;
static volatile uint32_t g_es7210_pcm_last_ret = 0xffffffffU;
static volatile uint32_t g_es7210_pcm_partial_read = 0;
static volatile uint32_t g_es7210_pcm_accum_timeout = 0;
static volatile uint32_t g_es7210_pcm_need_resume = 0;
static volatile uint32_t g_es7210_pcm_stop_for_downlink = 0;
static volatile uint32_t g_es7210_pcm_warmup_left = 0;
static volatile uint32_t g_es7210_pcm_warmup_drop = 0;
static uint8_t g_es7210_uplink_raw[OHOS_ES7210_UPLINK_CAPTURE_STEREO_BYTES];
static int16_t *g_es7210_uplink_ring = NULL;
static int16_t g_es7210_uplink_ring_fallback[OHOS_ES7210_UPLINK_CAPTURE_FALLBACK_SAMPLES];
static volatile uint32_t g_es7210_uplink_ring_capacity = 0;
static volatile uint32_t g_es7210_uplink_ring_read_idx = 0;
static volatile uint32_t g_es7210_uplink_ring_write_idx = 0;
static volatile uint32_t g_es7210_uplink_ring_count = 0;
static volatile uint32_t g_es7210_uplink_ring_min_level = 0xffffffffU;
static volatile uint32_t g_es7210_uplink_ring_max_level = 0;
static volatile uint32_t g_es7210_uplink_task_started = 0;
static volatile uint32_t g_es7210_uplink_running = 0;
static volatile uint32_t g_es7210_uplink_stop_req = 0;
static volatile uint32_t g_es7210_uplink_stopping = 0;
static volatile uint32_t g_es7210_uplink_produced_samples = 0;
static volatile uint32_t g_es7210_uplink_consumed_samples = 0;
static volatile uint32_t g_es7210_uplink_read_ok = 0;
static volatile uint32_t g_es7210_uplink_read_fail = 0;
static volatile uint32_t g_es7210_uplink_overflow = 0;
static volatile uint32_t g_es7210_uplink_underflow = 0;
static volatile uint32_t g_es7210_uplink_peak = 0;
static volatile uint32_t g_es7210_uplink_nonzero = 0;
static volatile uint32_t g_es7210_uplink_use_right = 0;
static volatile uint32_t g_es7210_uplink_last_ret = 0xffffffffU;
static volatile uint32_t g_es7210_uplink_last_bytes = 0;
static volatile uint32_t g_es7210_uplink_start_ms = 0;
static volatile uint32_t g_es7210_uplink_last_stat_ms = 0;
static volatile uint32_t g_es7210_uplink_last_stat_samples = 0;
static volatile uint32_t g_es7210_uplink_warmup_left = 0;
static volatile uint32_t g_es7210_uplink_warmup_drop = 0;
static volatile uint32_t g_es7210_uplink_preferred_ready = 0;
static volatile uint32_t g_es7210_uplink_preferred_right = 0;
static OhosAudioEs7210PcmTap g_es7210_pcm_tap = NULL;
static void *g_es7210_pcm_tap_user_data = NULL;

static esp_err_t OhosAudioEs7210PrepareUplinkStereo(void)
{
    esp_err_t ret = bsp_extra_codec_set_fs(16000, 16, I2S_SLOT_MODE_STEREO);
    ESP_LOGI(TAG, "S80D uplink force fs 16k/16bit/stereo ret=%s",
             esp_err_to_name(ret));
    return ret;
}

static int16_t OhosAudioClamp16(int32_t v)
{
    if (v > 32767) {
        return 32767;
    }
    if (v < -32768) {
        return -32768;
    }
    return (int16_t)v;
}

static uint32_t OhosAudioEs7210UplinkRingInit(void)
{
    if (g_es7210_uplink_ring != NULL && g_es7210_uplink_ring_capacity > 0U) {
        return 0;
    }

    g_es7210_uplink_ring =
        (int16_t *)heap_caps_calloc(OHOS_ES7210_UPLINK_CAPTURE_RING_SAMPLES,
                                    sizeof(int16_t),
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (g_es7210_uplink_ring != NULL) {
        g_es7210_uplink_ring_capacity = OHOS_ES7210_UPLINK_CAPTURE_RING_SAMPLES;
        ESP_LOGI(TAG,
                 "P4-UP-CAP ring psram ok cap=%u bytes=%u ptr=%p",
                 (unsigned)g_es7210_uplink_ring_capacity,
                 (unsigned)(OHOS_ES7210_UPLINK_CAPTURE_RING_SAMPLES * sizeof(int16_t)),
                 g_es7210_uplink_ring);
        return 0;
    }

    memset(g_es7210_uplink_ring_fallback, 0, sizeof(g_es7210_uplink_ring_fallback));
    g_es7210_uplink_ring = g_es7210_uplink_ring_fallback;
    g_es7210_uplink_ring_capacity = OHOS_ES7210_UPLINK_CAPTURE_FALLBACK_SAMPLES;
    ESP_LOGW(TAG,
             "P4-UP-CAP ring psram fail fallback cap=%u bytes=%u ptr=%p",
             (unsigned)g_es7210_uplink_ring_capacity,
             (unsigned)sizeof(g_es7210_uplink_ring_fallback),
             g_es7210_uplink_ring);
    return 0;
}

static void OhosAudioEs7210UplinkRingReset(void)
{
    uint32_t intSave;

    (void)OhosAudioEs7210UplinkRingInit();

    intSave = LOS_IntLock();
    g_es7210_uplink_ring_read_idx = 0;
    g_es7210_uplink_ring_write_idx = 0;
    g_es7210_uplink_ring_count = 0;
    LOS_IntRestore(intSave);

    g_es7210_uplink_ring_min_level = 0xffffffffU;
    g_es7210_uplink_ring_max_level = 0;
    g_es7210_uplink_produced_samples = 0;
    g_es7210_uplink_consumed_samples = 0;
    g_es7210_uplink_read_ok = 0;
    g_es7210_uplink_read_fail = 0;
    g_es7210_uplink_overflow = 0;
    g_es7210_uplink_underflow = 0;
    g_es7210_uplink_peak = 0;
    g_es7210_uplink_nonzero = 0;
    g_es7210_uplink_use_right = 0;
    g_es7210_uplink_last_ret = 0xffffffffU;
    g_es7210_uplink_last_bytes = 0;
    g_es7210_uplink_last_stat_samples = 0;
    g_es7210_uplink_warmup_left = OHOS_ES7210_UPLINK_WARMUP_DROP_FRAMES;
    g_es7210_uplink_warmup_drop = 0;
    g_es7210_uplink_preferred_ready = 0;
    g_es7210_uplink_preferred_right = 0;
}

static uint32_t OhosAudioEs7210UplinkRingWrite(const int16_t *pcm, uint32_t samples)
{
    uint32_t written = 0;
    uint32_t intSave;

    if (pcm == NULL || samples == 0U ||
        g_es7210_uplink_ring == NULL || g_es7210_uplink_ring_capacity == 0U) {
        return 0;
    }

    intSave = LOS_IntLock();
    while (written < samples) {
        if (g_es7210_uplink_ring_count >= g_es7210_uplink_ring_capacity) {
            g_es7210_uplink_ring_read_idx =
                (g_es7210_uplink_ring_read_idx + 1U) % g_es7210_uplink_ring_capacity;
            g_es7210_uplink_ring_count--;
            g_es7210_uplink_overflow++;
        }
        g_es7210_uplink_ring[g_es7210_uplink_ring_write_idx] = pcm[written];
        g_es7210_uplink_ring_write_idx =
            (g_es7210_uplink_ring_write_idx + 1U) % g_es7210_uplink_ring_capacity;
        g_es7210_uplink_ring_count++;
        written++;
    }

    if (g_es7210_uplink_ring_count > g_es7210_uplink_ring_max_level) {
        g_es7210_uplink_ring_max_level = g_es7210_uplink_ring_count;
    }
    if (g_es7210_uplink_ring_count < g_es7210_uplink_ring_min_level) {
        g_es7210_uplink_ring_min_level = g_es7210_uplink_ring_count;
    }
    LOS_IntRestore(intSave);

    if (written > 0U) {
        g_es7210_uplink_produced_samples += written;
    }
    return written;
}

static uint32_t OhosAudioEs7210UplinkRingRead(int16_t *out, uint32_t samples)
{
    uint32_t read = 0;
    uint32_t intSave;

    if (out == NULL || samples == 0U ||
        g_es7210_uplink_ring == NULL || g_es7210_uplink_ring_capacity == 0U) {
        return 0;
    }

    intSave = LOS_IntLock();
    while (read < samples && g_es7210_uplink_ring_count > 0U) {
        out[read] = g_es7210_uplink_ring[g_es7210_uplink_ring_read_idx];
        g_es7210_uplink_ring_read_idx =
            (g_es7210_uplink_ring_read_idx + 1U) % g_es7210_uplink_ring_capacity;
        g_es7210_uplink_ring_count--;
        read++;
    }
    if (g_es7210_uplink_ring_count < g_es7210_uplink_ring_min_level) {
        g_es7210_uplink_ring_min_level = g_es7210_uplink_ring_count;
    }
    LOS_IntRestore(intSave);

    if (read > 0U) {
        g_es7210_uplink_consumed_samples += read;
    }
    return read;
}

static void OhosAudioEs7210UplinkConvertAndPush(const int16_t *chunk,
                                                uint32_t pairs)
{
    int16_t mono[OHOS_ES7210_UPLINK_CAPTURE_CHUNK_SAMPLES];
    int16_t commandMono[OHOS_ES7210_UPLINK_CAPTURE_CHUNK_SAMPLES];
    int32_t peak_l = 0;
    int32_t peak_r = 0;
    int32_t rawPeak = 0;
    int32_t outPeak = 0;
    uint32_t nonzero_l = 0;
    uint32_t nonzero_r = 0;
    uint32_t outNonzero = 0;
    uint32_t clip = 0;
    uint32_t useRight;
    uint32_t count = pairs;

    if (chunk == NULL || pairs == 0U) {
        return;
    }
    if (count > OHOS_ES7210_UPLINK_CAPTURE_CHUNK_SAMPLES) {
        count = OHOS_ES7210_UPLINK_CAPTURE_CHUNK_SAMPLES;
    }

    for (uint32_t i = 0; i < count; ++i) {
        int32_t l0 = chunk[i * 2U];
        int32_t r0 = chunk[i * 2U + 1U];
        int32_t al = l0 < 0 ? -l0 : l0;
        int32_t ar = r0 < 0 ? -r0 : r0;

        if (al > peak_l) {
            peak_l = al;
        }
        if (ar > peak_r) {
            peak_r = ar;
        }
        if (l0 != 0) {
            nonzero_l++;
        }
        if (r0 != 0) {
            nonzero_r++;
        }
    }

    useRight = ((uint32_t)peak_r > ((uint32_t)peak_l * 2U)) ||
               (nonzero_r > (nonzero_l * 2U));
    if (!g_es7210_uplink_preferred_ready) {
        g_es7210_uplink_preferred_right = useRight;
        g_es7210_uplink_preferred_ready = 1;
    } else {
        uint32_t chosenPeak = g_es7210_uplink_preferred_right ?
                              (uint32_t)peak_r : (uint32_t)peak_l;
        uint32_t otherPeak = g_es7210_uplink_preferred_right ?
                             (uint32_t)peak_l : (uint32_t)peak_r;
        uint32_t chosenNonzero = g_es7210_uplink_preferred_right ?
                                 nonzero_r : nonzero_l;
        uint32_t otherNonzero = g_es7210_uplink_preferred_right ?
                                nonzero_l : nonzero_r;
        if (otherPeak > chosenPeak * 4U && otherNonzero > chosenNonzero * 2U) {
            g_es7210_uplink_preferred_right = !g_es7210_uplink_preferred_right;
        }
    }
    useRight = g_es7210_uplink_preferred_right;

    for (uint32_t i = 0; i < count; ++i) {
        int32_t v = chunk[i * 2U + useRight];
        int32_t av = v < 0 ? -v : v;
        if (av > rawPeak) {
            rawPeak = av;
        }
        commandMono[i] = OhosAudioClamp16(v * OHOS_ES7210_COMMAND_VOICE_GAIN);
        if (v > -OHOS_ES7210_UPLINK_NOISE_GATE && v < OHOS_ES7210_UPLINK_NOISE_GATE) {
            v = 0;
        }
        int32_t amplified = v * OHOS_ES7210_UPLINK_VOICE_GAIN;
        if (amplified > 32767 || amplified < -32768) {
            clip++;
        }
        int16_t y = OhosAudioClamp16(amplified);
        int32_t ay = y < 0 ? -(int32_t)y : (int32_t)y;
        mono[i] = y;
        if (ay > outPeak) {
            outPeak = ay;
        }
        if (y != 0) {
            outNonzero++;
        }
    }

    g_es7210_uplink_peak = (uint32_t)outPeak;
    g_es7210_uplink_nonzero = outNonzero;
    g_es7210_uplink_use_right = useRight;
    g_es7210_pcm_peak = (uint32_t)outPeak;
    g_es7210_pcm_nonzero = outNonzero;
    g_es7210_pcm_use_right = useRight;

    if (g_es7210_uplink_warmup_left > 0U) {
        g_es7210_uplink_warmup_left--;
        g_es7210_uplink_warmup_drop++;
        if (g_es7210_uplink_warmup_drop <= 4U) {
            ESP_LOGI(TAG,
                     "P4-UP-CAP warmup drop left=%u drop=%u use=%s rawPeak=%ld outPeak=%ld nonzero=%u clip=%u L=%ld R=%ld",
                     (unsigned)g_es7210_uplink_warmup_left,
                     (unsigned)g_es7210_uplink_warmup_drop,
                     useRight ? "R" : "L",
                     (long)rawPeak,
                     (long)outPeak,
                     (unsigned)outNonzero,
                     (unsigned)clip,
                     (long)peak_l,
                     (long)peak_r);
        }
        return;
    }

    OhosAudioEs7210PcmTap tap = g_es7210_pcm_tap;
    if (tap != NULL) {
        tap(commandMono, count, g_es7210_pcm_tap_user_data);
    }
    (void)OhosAudioEs7210UplinkRingWrite(mono, count);
}

void OhosAudioEs7210SetPcmTap(OhosAudioEs7210PcmTap tap, void *userData)
{
    uint32_t intSave = LOS_IntLock();
    g_es7210_pcm_tap = tap;
    g_es7210_pcm_tap_user_data = userData;
    LOS_IntRestore(intSave);
}

static void OhosAudioEs7210UplinkCaptureTask(void *arg)
{
    (void)arg;
    esp_err_t ret;

    ESP_LOGI(TAG,
             "P4-UP-CAP task start prio=%u stack=0x%x fs=16000 bits=16 src=stereo chunkMs=%u ringCap=%u",
             (unsigned)OHOS_ES7210_UPLINK_CAPTURE_TASK_PRIO,
             (unsigned)OHOS_ES7210_UPLINK_CAPTURE_TASK_STACK,
             (unsigned)OHOS_ES7210_UPLINK_CAPTURE_CHUNK_MS,
             (unsigned)g_es7210_uplink_ring_capacity);

    if (g_es7210_pcm_need_resume) {
        ret = bsp_extra_codec_dev_resume();
        ESP_LOGI(TAG, "P4-UP-CAP codec resume ret=%s", esp_err_to_name(ret));
        if (ret != ESP_OK) {
            ret = bsp_extra_codec_init();
            ESP_LOGI(TAG, "P4-UP-CAP resume fallback init ret=%s", esp_err_to_name(ret));
        }
    } else if (!g_es7210_pcm_init_ok) {
        ret = bsp_extra_codec_init();
        ESP_LOGI(TAG, "P4-UP-CAP codec init ret=%s", esp_err_to_name(ret));
    } else {
        ret = ESP_OK;
    }

    if (ret == ESP_OK) {
        ret = OhosAudioEs7210PrepareUplinkStereo();
    }

    if (ret != ESP_OK) {
        g_es7210_uplink_last_ret = (uint32_t)ret;
        g_es7210_uplink_read_fail++;
        g_es7210_uplink_running = 0;
        g_es7210_uplink_stop_req = 0;
        g_es7210_uplink_task_started = 0;
        ESP_LOGE(TAG, "P4-UP-CAP prepare fail ret=%s", esp_err_to_name(ret));
        return;
    }

    g_es7210_pcm_init_ok = 1;
    g_es7210_pcm_need_resume = 0;
    g_es7210_uplink_last_stat_ms = (uint32_t)esp_log_timestamp();
    g_es7210_uplink_start_ms = g_es7210_uplink_last_stat_ms;

    while (g_es7210_uplink_running &&
           !g_es7210_uplink_stop_req &&
           !g_es7210_pcm_stop_for_downlink) {
        size_t bytesRead = 0;

        while (g_es7210_uplink_running &&
               !g_es7210_uplink_stop_req &&
               !g_es7210_pcm_stop_for_downlink &&
               g_es7210_uplink_ring_count >= OHOS_ES7210_UPLINK_CAPTURE_HIGH_WATER) {
            LOS_TaskDelay(1);
        }

        if (!g_es7210_uplink_running ||
            g_es7210_uplink_stop_req ||
            g_es7210_pcm_stop_for_downlink) {
            break;
        }

        memset(g_es7210_uplink_raw, 0, sizeof(g_es7210_uplink_raw));

        ret = bsp_extra_i2s_read(g_es7210_uplink_raw,
                                 sizeof(g_es7210_uplink_raw),
                                 &bytesRead,
                                 80);
        g_es7210_uplink_last_ret = (uint32_t)ret;
        g_es7210_uplink_last_bytes = (uint32_t)bytesRead;
        g_es7210_pcm_last_ret = (uint32_t)ret;
        g_es7210_pcm_bytes_read = (uint32_t)bytesRead;

        if (ret != ESP_OK || bytesRead < sizeof(g_es7210_uplink_raw)) {
            g_es7210_uplink_read_fail++;
            g_es7210_pcm_read_fail++;
            if (g_es7210_uplink_read_fail <= 5U ||
                (g_es7210_uplink_read_fail % 50U) == 0U) {
                ESP_LOGW(TAG,
                         "P4-UP-CAP read fail ret=%s bytes=%u need=%u fail=%u stop=%u",
                         esp_err_to_name(ret),
                         (unsigned)bytesRead,
                         (unsigned)sizeof(g_es7210_uplink_raw),
                         (unsigned)g_es7210_uplink_read_fail,
                         (unsigned)g_es7210_pcm_stop_for_downlink);
            }
            LOS_TaskDelay(1);
            continue;
        }

        g_es7210_uplink_read_ok++;
        g_es7210_pcm_read_ok++;
        g_es7210_pcm_samples = OHOS_ES7210_UPLINK_CAPTURE_CHUNK_SAMPLES;
        OhosAudioEs7210UplinkConvertAndPush((const int16_t *)g_es7210_uplink_raw,
                                            OHOS_ES7210_UPLINK_CAPTURE_CHUNK_SAMPLES);

        uint32_t nowMs = (uint32_t)esp_log_timestamp();
        if ((uint32_t)(nowMs - g_es7210_uplink_last_stat_ms) >= 1000U) {
            uint32_t producedDelta =
                g_es7210_uplink_produced_samples - g_es7210_uplink_last_stat_samples;
            uint32_t minLevel = (g_es7210_uplink_ring_min_level == 0xffffffffU) ?
                                0U : g_es7210_uplink_ring_min_level;
            ESP_LOGI(TAG,
                     "P4-UP-CAP stat sec=%u produced=%u total=%u consumed=%u ring=%u min=%u max=%u readOk=%u readFail=%u overflow=%u underflow=%u peak=%u nonzero=%u use=%s bytes=%u",
                     (unsigned)((nowMs - g_es7210_uplink_start_ms) / 1000U),
                     (unsigned)producedDelta,
                     (unsigned)g_es7210_uplink_produced_samples,
                     (unsigned)g_es7210_uplink_consumed_samples,
                     (unsigned)g_es7210_uplink_ring_count,
                     (unsigned)minLevel,
                     (unsigned)g_es7210_uplink_ring_max_level,
                     (unsigned)g_es7210_uplink_read_ok,
                     (unsigned)g_es7210_uplink_read_fail,
                     (unsigned)g_es7210_uplink_overflow,
                     (unsigned)g_es7210_uplink_underflow,
                     (unsigned)g_es7210_uplink_peak,
                     (unsigned)g_es7210_uplink_nonzero,
                     g_es7210_uplink_use_right ? "R" : "L",
                     (unsigned)g_es7210_uplink_last_bytes);
            g_es7210_uplink_last_stat_ms = nowMs;
            g_es7210_uplink_last_stat_samples = g_es7210_uplink_produced_samples;
        }

        LOS_TaskYield();
    }

    ESP_LOGI(TAG,
             "P4-UP-CAP task stop run=%u stop=%u downStop=%u produced=%u consumed=%u ring=%u readOk=%u readFail=%u overflow=%u underflow=%u",
             (unsigned)g_es7210_uplink_running,
             (unsigned)g_es7210_uplink_stop_req,
             (unsigned)g_es7210_pcm_stop_for_downlink,
             (unsigned)g_es7210_uplink_produced_samples,
             (unsigned)g_es7210_uplink_consumed_samples,
             (unsigned)g_es7210_uplink_ring_count,
             (unsigned)g_es7210_uplink_read_ok,
             (unsigned)g_es7210_uplink_read_fail,
             (unsigned)g_es7210_uplink_overflow,
             (unsigned)g_es7210_uplink_underflow);

    g_es7210_uplink_running = 0;
    g_es7210_uplink_stop_req = 0;
    g_es7210_uplink_task_started = 0;
    g_es7210_uplink_stopping = 0;
}

uint32_t OhosAudioEs7210StartUplinkCapture(void)
{
#if OHOS_ES7210_UPLINK_CAPTURE_ENABLE
    UINT32 ret;

    if (g_es7210_pcm_stop_for_downlink) {
        g_es7210_uplink_last_ret = 0xfffffffcU;
        ESP_LOGW(TAG, "P4-UP-CAP start rejected: stop_for_downlink=1");
        return 1;
    }

    (void)OhosAudioEs7210UplinkRingInit();

    if (g_es7210_uplink_stopping) {
        uint32_t waitMs = 0;
        while (g_es7210_uplink_stopping && waitMs < 150U) {
            OhosLiteosDelayMs(5);
            waitMs += 5U;
        }
        if (g_es7210_uplink_stopping) {
            g_es7210_uplink_last_ret = 0xfffffffbU;
            ESP_LOGW(TAG,
                     "P4-UP-CAP start rejected: stopping wait=%u task=%u run=%u stop=%u",
                     (unsigned)waitMs,
                     (unsigned)g_es7210_uplink_task_started,
                     (unsigned)g_es7210_uplink_running,
                     (unsigned)g_es7210_uplink_stop_req);
            return 3;
        }
        ESP_LOGI(TAG, "P4-UP-CAP start waited old stop wait=%u", (unsigned)waitMs);
    }

    OhosAudioEs7210UplinkRingReset();
    g_es7210_uplink_stop_req = 0;
    g_es7210_uplink_running = 1;

    if (g_es7210_uplink_task_started) {
        ESP_LOGI(TAG,
                 "P4-UP-CAP start reuse running task ring=%u cap=%u",
                 (unsigned)g_es7210_uplink_ring_count,
                 (unsigned)g_es7210_uplink_ring_capacity);
        return 0;
    }

    g_es7210_uplink_stopping = 0;
    g_es7210_uplink_task_started = 1;
    ret = OhosLiteosCreateTask("ohos_up_cap",
                               OhosAudioEs7210UplinkCaptureTask,
                               NULL,
                               OHOS_ES7210_UPLINK_CAPTURE_TASK_PRIO,
                               OHOS_ES7210_UPLINK_CAPTURE_TASK_STACK,
                               NULL);
    if (ret != LOS_OK) {
        g_es7210_uplink_running = 0;
        g_es7210_uplink_stop_req = 0;
        g_es7210_uplink_task_started = 0;
        g_es7210_uplink_last_ret = ret;
        ESP_LOGE(TAG, "P4-UP-CAP task create fail ret=%u", (unsigned)ret);
        return 2;
    }

    ESP_LOGI(TAG,
             "P4-UP-CAP start ok ret=%u startLevel=%u chunkMs=%u gain=%u gate=%u",
             (unsigned)ret,
             (unsigned)OHOS_ES7210_UPLINK_CAPTURE_START_LEVEL,
             (unsigned)OHOS_ES7210_UPLINK_CAPTURE_CHUNK_MS,
             (unsigned)OHOS_ES7210_UPLINK_VOICE_GAIN,
             (unsigned)OHOS_ES7210_UPLINK_NOISE_GATE);
    return 0;
#else
    return 1;
#endif
}

void OhosAudioEs7210StopUplinkCapture(void)
{
    uint32_t waitMs = 0;

    g_es7210_uplink_stopping = 1;
    g_es7210_uplink_running = 0;
    g_es7210_uplink_stop_req = 1;

    while (g_es7210_uplink_task_started && waitMs < 120U) {
        OhosLiteosDelayMs(5);
        waitMs += 5U;
    }

    ESP_LOGI(TAG,
             "P4-UP-CAP stop requested wait=%u task=%u produced=%u consumed=%u ring=%u overflow=%u underflow=%u",
             (unsigned)waitMs,
             (unsigned)g_es7210_uplink_task_started,
             (unsigned)g_es7210_uplink_produced_samples,
             (unsigned)g_es7210_uplink_consumed_samples,
             (unsigned)g_es7210_uplink_ring_count,
             (unsigned)g_es7210_uplink_overflow,
             (unsigned)g_es7210_uplink_underflow);

    if (!g_es7210_uplink_task_started) {
        g_es7210_uplink_stopping = 0;
    }
}

uint32_t OhosAudioEs7210IsUplinkCaptureStopping(void)
{
    return g_es7210_uplink_stopping;
}

uint32_t OhosAudioEs7210GetUplinkCaptureLevel(void)
{
    return g_es7210_uplink_ring_count;
}

uint32_t OhosAudioEs7210ReadUplinkCaptureFrame(int16_t *out,
                                               uint32_t samples,
                                               uint32_t waitMs)
{
    uint32_t waited = 0;

    if (out == NULL || samples != OHOS_ES7210_MONO_FRAME_SAMPLES) {
        g_es7210_uplink_underflow++;
        return 1;
    }

    while (g_es7210_uplink_ring_count < samples && waited < waitMs &&
           g_es7210_uplink_running && !g_es7210_pcm_stop_for_downlink) {
        uint32_t step = waitMs - waited;
        if (step > OHOS_ES7210_UPLINK_CAPTURE_WAIT_STEP_MS) {
            step = OHOS_ES7210_UPLINK_CAPTURE_WAIT_STEP_MS;
        }
        OhosLiteosDelayMs(step);
        waited += step;
    }

    if (g_es7210_uplink_ring_count < samples) {
        g_es7210_uplink_underflow++;
        if (g_es7210_uplink_underflow <= 5U ||
            (g_es7210_uplink_underflow % 50U) == 0U) {
            ESP_LOGW(TAG,
                     "P4-UP-CAP underflow need=%u level=%u wait=%u running=%u downStop=%u under=%u produced=%u consumed=%u",
                     (unsigned)samples,
                     (unsigned)g_es7210_uplink_ring_count,
                     (unsigned)waited,
                     (unsigned)g_es7210_uplink_running,
                     (unsigned)g_es7210_pcm_stop_for_downlink,
                     (unsigned)g_es7210_uplink_underflow,
                     (unsigned)g_es7210_uplink_produced_samples,
                     (unsigned)g_es7210_uplink_consumed_samples);
        }
        return 2;
    }

    if (OhosAudioEs7210UplinkRingRead(out, samples) != samples) {
        g_es7210_uplink_underflow++;
        return 3;
    }

    return 0;
}

void OhosAudioEs7210GetUplinkCaptureSnapshot(OhosAudioEs7210UplinkCaptureSnapshot *snap)
{
    if (snap == NULL) {
        return;
    }

    snap->task_started = g_es7210_uplink_task_started;
    snap->running = g_es7210_uplink_running;
    snap->stop_req = g_es7210_uplink_stop_req;
    snap->ring_level = g_es7210_uplink_ring_count;
    snap->ring_capacity = g_es7210_uplink_ring_capacity;
    snap->produced_samples = g_es7210_uplink_produced_samples;
    snap->consumed_samples = g_es7210_uplink_consumed_samples;
    snap->read_ok = g_es7210_uplink_read_ok;
    snap->read_fail = g_es7210_uplink_read_fail;
    snap->overflow = g_es7210_uplink_overflow;
    snap->underflow = g_es7210_uplink_underflow;
    snap->peak = g_es7210_uplink_peak;
    snap->nonzero = g_es7210_uplink_nonzero;
    snap->use_right = g_es7210_uplink_use_right;
    snap->last_ret = g_es7210_uplink_last_ret;
    snap->last_bytes = g_es7210_uplink_last_bytes;
    snap->min_level = (g_es7210_uplink_ring_min_level == 0xffffffffU) ?
                      0U : g_es7210_uplink_ring_min_level;
    snap->max_level = g_es7210_uplink_ring_max_level;
}

uint32_t OhosAudioEs7210ReadPcm16MonoFrame(int16_t *out,
                                           uint32_t samples,
                                           uint32_t timeoutMs)
{
    size_t bytesRead = 0;
    size_t accumLen = 0;
    uint32_t elapsedMs = 0;
    esp_err_t ret;

    if (out == NULL || samples != OHOS_ES7210_MONO_FRAME_SAMPLES) {
        g_es7210_pcm_read_fail++;
        g_es7210_pcm_last_ret = 0xfffffffeU;
        return 1;
    }

    if (!g_es7210_pcm_init_ok) {
        if (g_es7210_pcm_need_resume) {
            ret = bsp_extra_codec_dev_resume();
            ESP_LOGI(TAG, "S80A uplink bsp_extra_codec_dev_resume ret=%s",
                     esp_err_to_name(ret));
            if (ret != ESP_OK) {
                ret = bsp_extra_codec_init();
                ESP_LOGI(TAG, "S80A uplink resume fallback init ret=%s",
                         esp_err_to_name(ret));
            }
        } else {
            ret = bsp_extra_codec_init();
            ESP_LOGI(TAG, "S80A uplink bsp_extra_codec_init ret=%s", esp_err_to_name(ret));
        }
        if (ret != ESP_OK) {
            g_es7210_pcm_read_fail++;
            g_es7210_pcm_last_ret = (uint32_t)ret;
            return 2;
        }
        ret = OhosAudioEs7210PrepareUplinkStereo();
        if (ret != ESP_OK) {
            g_es7210_pcm_read_fail++;
            g_es7210_pcm_last_ret = (uint32_t)ret;
            return 2;
        }
        g_es7210_pcm_init_ok = 1;
        g_es7210_pcm_need_resume = 0;
        g_es7210_pcm_warmup_left = OHOS_ES7210_UPLINK_WARMUP_DROP_FRAMES;
    }

    if (g_es7210_pcm_stop_for_downlink) {
        g_es7210_pcm_read_fail++;
        g_es7210_pcm_last_ret = 0xfffffffcU;
        return 4;
    }

    memset(g_es7210_pcm_raw, 0, sizeof(g_es7210_pcm_raw));
    while (accumLen < sizeof(g_es7210_pcm_raw) && elapsedMs < timeoutMs) {
        size_t sliceRead = 0;
        uint32_t remainMs = timeoutMs - elapsedMs;
        uint32_t sliceMs = (remainMs > OHOS_ES7210_ACCUM_READ_SLICE_MS) ?
                           OHOS_ES7210_ACCUM_READ_SLICE_MS : remainMs;

        if (g_es7210_pcm_stop_for_downlink) {
            g_es7210_pcm_last_ret = 0xfffffffcU;
            break;
        }
        if (sliceMs == 0U) {
            sliceMs = 1U;
        }

        ret = bsp_extra_i2s_read(&g_es7210_pcm_raw[accumLen],
                                 sizeof(g_es7210_pcm_raw) - accumLen,
                                 &sliceRead,
                                 sliceMs);
        g_es7210_pcm_last_ret = (uint32_t)ret;

        if (ret != ESP_OK) {
            break;
        }

        if (sliceRead > 0U) {
            accumLen += sliceRead;
            bytesRead += sliceRead;
            if (accumLen < sizeof(g_es7210_pcm_raw)) {
                g_es7210_pcm_partial_read++;
            }
        }

        elapsedMs += sliceMs;
        if (sliceRead == 0U) {
            OhosLiteosDelayMs(1);
        }
    }

    g_es7210_pcm_bytes_read = (uint32_t)bytesRead;

    if (g_es7210_pcm_stop_for_downlink) {
        g_es7210_pcm_read_fail++;
        ESP_LOGI(TAG,
                 "S80A uplink pcm read stopped for downlink bytes=%u partial=%u",
                 (unsigned)bytesRead,
                 (unsigned)g_es7210_pcm_partial_read);
        return 4;
    }

    if (g_es7210_pcm_last_ret != (uint32_t)ESP_OK || accumLen < sizeof(g_es7210_pcm_raw)) {
        g_es7210_pcm_read_fail++;
        if (accumLen < sizeof(g_es7210_pcm_raw)) {
            g_es7210_pcm_accum_timeout++;
        }
        ESP_LOGE(TAG,
                 "S80A uplink i2s accum fail ret=%s bytes=%u need=%u partial=%u timeout=%u",
                 esp_err_to_name((esp_err_t)g_es7210_pcm_last_ret),
                 (unsigned)bytesRead,
                 (unsigned)sizeof(g_es7210_pcm_raw),
                 (unsigned)g_es7210_pcm_partial_read,
                 (unsigned)g_es7210_pcm_accum_timeout);
        return 3;
    }

    int16_t *chunk = (int16_t *)g_es7210_pcm_raw;
    uint32_t samples16 = (uint32_t)(bytesRead / sizeof(int16_t));
    if (samples16 < samples * 2U) {
        g_es7210_pcm_read_fail++;
        g_es7210_pcm_last_ret = 0xfffffffbU;
        ESP_LOGE(TAG,
                 "S80D uplink stereo sample short bytes=%u samples16=%u need=%u",
                 (unsigned)bytesRead,
                 (unsigned)samples16,
                 (unsigned)(samples * 2U));
        return 3;
    }

    const int32_t voiceGain = OHOS_ES7210_UPLINK_VOICE_GAIN;
    int32_t peak_l = 0;
    int32_t peak_r = 0;
    uint32_t nonzero_l = 0;
    uint32_t nonzero_r = 0;
    uint64_t abs_sum_l = 0;
    uint64_t abs_sum_r = 0;
    int32_t rawPeak = 0;
    uint32_t rawNonzero = 0;
    uint64_t rawAbsSum = 0;
    int32_t voicePeak = 0;
    uint32_t voiceNonzero = 0;
    uint64_t voiceAbsSum = 0;
    uint32_t voiceClipped = 0;
    int32_t firstRaw[4] = {0};
    int32_t firstOut[4] = {0};

    for (uint32_t i = 0; i + 1U < samples16; i += 2U) {
        int32_t l0 = chunk[i];
        int32_t r0 = chunk[i + 1U];
        int32_t al = l0 < 0 ? -l0 : l0;
        int32_t ar = r0 < 0 ? -r0 : r0;

        if (al > peak_l) {
            peak_l = al;
        }
        if (ar > peak_r) {
            peak_r = ar;
        }
        if (l0 != 0) {
            nonzero_l++;
        }
        if (r0 != 0) {
            nonzero_r++;
        }
        abs_sum_l += (uint32_t)al;
        abs_sum_r += (uint32_t)ar;
    }

    uint32_t useRight = ((uint32_t)peak_r > ((uint32_t)peak_l * 2U)) ||
                        (nonzero_r > (nonzero_l * 2U));

    for (uint32_t i = 0; i < samples; ++i) {
        uint32_t src = (i * 2U) + useRight;
        int32_t v = chunk[src];
        int32_t raw = v;
        int32_t ar = raw < 0 ? -raw : raw;

        if (ar > rawPeak) {
            rawPeak = ar;
        }
        if (raw != 0) {
            rawNonzero++;
        }
        rawAbsSum += (uint32_t)ar;

        if (v > -OHOS_ES7210_UPLINK_NOISE_GATE && v < OHOS_ES7210_UPLINK_NOISE_GATE) {
            v = 0;
        }

        int32_t amplified = v * voiceGain;
        if (amplified > 32767 || amplified < -32768) {
            voiceClipped++;
        }
        int16_t y = OhosAudioClamp16(amplified);
        int32_t ay = y < 0 ? -(int32_t)y : (int32_t)y;
        out[i] = y;

        if (i < 4U) {
            firstRaw[i] = raw;
            firstOut[i] = y;
        }
        if (ay > voicePeak) {
            voicePeak = ay;
        }
        if (y != 0) {
            voiceNonzero++;
        }
        voiceAbsSum += (uint32_t)ay;
    }

    g_es7210_pcm_read_ok++;
    g_es7210_pcm_samples = samples;
    g_es7210_pcm_peak = (uint32_t)voicePeak;
    g_es7210_pcm_nonzero = voiceNonzero;
    g_es7210_pcm_use_right = useRight;

    if (g_es7210_pcm_read_ok <= 4U || (g_es7210_pcm_read_ok % 100U) == 0U) {
        uint32_t stereoSamples = samples16 / 2U;
        uint32_t avg_l = stereoSamples ? (uint32_t)(abs_sum_l / stereoSamples) : 0U;
        uint32_t avg_r = stereoSamples ? (uint32_t)(abs_sum_r / stereoSamples) : 0U;
        uint32_t rawAvg = samples ? (uint32_t)(rawAbsSum / samples) : 0U;
        uint32_t voiceAvg = samples ? (uint32_t)(voiceAbsSum / samples) : 0U;
        ESP_LOGI(TAG,
                 "S80D uplink stereo-to-mono diag seq=%u bytes=%u samples=%u use=%s gain=%ld gate=%u L_peak=%ld R_peak=%ld L_avg=%u R_avg=%u rawPeak=%ld rawAvg=%u rawNonzero=%u outPeak=%ld outAvg=%u outNonzero=%u clip=%u warm=%u raw4=[%ld,%ld,%ld,%ld] out4=[%ld,%ld,%ld,%ld]",
                 (unsigned)g_es7210_pcm_read_ok,
                 (unsigned)bytesRead,
                 (unsigned)samples,
                 useRight ? "R" : "L",
                 (long)voiceGain,
                 (unsigned)OHOS_ES7210_UPLINK_NOISE_GATE,
                 (long)peak_l,
                 (long)peak_r,
                 (unsigned)avg_l,
                 (unsigned)avg_r,
                 (long)rawPeak,
                 (unsigned)rawAvg,
                 (unsigned)rawNonzero,
                 (long)voicePeak,
                 (unsigned)voiceAvg,
                 (unsigned)voiceNonzero,
                 (unsigned)voiceClipped,
                 (unsigned)g_es7210_pcm_warmup_left,
                 (long)firstRaw[0],
                 (long)firstRaw[1],
                 (long)firstRaw[2],
                 (long)firstRaw[3],
                 (long)firstOut[0],
                 (long)firstOut[1],
                 (long)firstOut[2],
                 (long)firstOut[3]);
    }

    if (g_es7210_pcm_warmup_left > 0U) {
        g_es7210_pcm_warmup_left--;
        g_es7210_pcm_warmup_drop++;
        g_es7210_pcm_read_fail++;
        g_es7210_pcm_last_ret = 0xfffffff9U;
        if (g_es7210_pcm_warmup_drop <= 6U) {
            ESP_LOGI(TAG,
                     "S80D uplink drop warmup frame left=%u drop=%u peak=%ld use=%s",
                     (unsigned)g_es7210_pcm_warmup_left,
                     (unsigned)g_es7210_pcm_warmup_drop,
                     (long)voicePeak,
                     useRight ? "R" : "L");
        }
        return 5;
    }

    return 0;
}

void OhosAudioEs7210RequestPcmStopForDownlink(void)
{
    g_es7210_pcm_stop_for_downlink = 1;
    g_es7210_uplink_running = 0;
    g_es7210_uplink_stop_req = 1;
}

void OhosAudioEs7210ClearPcmStopForDownlink(void)
{
    g_es7210_pcm_stop_for_downlink = 0;
}

static void OhosAudioEs7210RecordStatsDelayStopAware(uint32_t total_ms)
{
    uint32_t elapsed = 0;

    while (elapsed < total_ms && !g_es7210_record_stop_req) {
        OhosLiteosDelayMs(50);
        elapsed += 50;
    }
}


static void OhosAudioEs7210RecordStatsTask(void *arg)
{
    ESP_LOGI(TAG, "ES7210 record stats task start");
    g_es7210_record_stop_req = 0;

    esp_err_t ret = bsp_extra_codec_init();
    ESP_LOGI(TAG, "bsp_extra_codec_init ret=%s", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        g_es7210_record_task_started = 0;
        return;
    }

    const size_t buf_size = 2400;
    int32_t *buf = (int32_t *)malloc(buf_size);
    if (buf == NULL) {
        ESP_LOGE(TAG, "malloc failed");
        g_es7210_record_task_started = 0;
        return;
    }

    uint32_t s71b_seq = 0;

    while (!g_es7210_record_stop_req) {
        size_t bytes_read = 0;
        memset(buf, 0, buf_size);

        ret = bsp_extra_i2s_read(buf, buf_size, &bytes_read, 1000);
        if (ret != ESP_OK) {
            g_es7210_record_last_ret = (uint32_t)ret;
            ESP_LOGE(TAG, "bsp_extra_i2s_read failed ret=%s", esp_err_to_name(ret));
            OhosAudioEs7210RecordStatsDelayStopAware(500);
            continue;
        }

        /*
         * S73B Mic Stats:
         * Use the same idea as S72C/S72D loopback.
         * ES7210 gives stereo-like 16-bit samples here. Human voice is mainly
         * on one channel, so compare L/R first and then show the stronger channel.
         */
        int samples16 = bytes_read / sizeof(int16_t);
        int pairs = samples16 / 2;
        int16_t *chunk16 = (int16_t *)buf;

        int32_t peak_l = 0;
        int32_t peak_r = 0;
        int nonzero_l = 0;
        int nonzero_r = 0;

        for (int i = 0; i + 1 < samples16; i += 2) {
            int32_t l0 = chunk16[i];
            int32_t r0 = chunk16[i + 1];

            int32_t al = l0 < 0 ? -l0 : l0;
            int32_t ar = r0 < 0 ? -r0 : r0;

            if (al > peak_l) {
                peak_l = al;
            }
            if (ar > peak_r) {
                peak_r = ar;
            }
            if (l0 != 0) {
                nonzero_l++;
            }
            if (r0 != 0) {
                nonzero_r++;
            }
        }

        int use_right = (peak_r > peak_l * 2) || (nonzero_r > nonzero_l * 2);
        const int32_t mic_stats_gain = 24;

        int32_t voice_peak16 = 0;
        int voice_nonzero = 0;
        int16_t first_voice0 = 0;
        int16_t first_voice1 = 0;
        int16_t first_voice2 = 0;
        int16_t first_voice3 = 0;
        int first_count = 0;

        for (int i = 0; i + 1 < samples16; i += 2) {
            int32_t v = use_right ? chunk16[i + 1] : chunk16[i];

            /*
             * Keep the same low gate as S72C/S72D. Do not use a high threshold,
             * otherwise normal speech may be filtered out and only wind noise remains.
             */
            if (v > -8 && v < 8) {
                v = 0;
            }

            int32_t out = v * mic_stats_gain;
            int16_t y;
            if (out > 32767) {
                y = 32767;
            } else if (out < -32768) {
                y = -32768;
            } else {
                y = (int16_t)out;
            }

            int32_t av = y < 0 ? -(int32_t)y : (int32_t)y;
            if (av > voice_peak16) {
                voice_peak16 = av;
            }
            if (y != 0) {
                voice_nonzero++;
            }

            if (first_count == 0) {
                first_voice0 = y;
            } else if (first_count == 1) {
                first_voice1 = y;
            } else if (first_count == 2) {
                first_voice2 = y;
            } else if (first_count == 3) {
                first_voice3 = y;
            }
            first_count++;
        }

        g_es7210_record_seq = ++s71b_seq;
        g_es7210_record_bytes_read = (uint32_t)bytes_read;
        g_es7210_record_samples = (uint32_t)pairs;
        g_es7210_record_peak = (uint32_t)voice_peak16;
        g_es7210_record_nonzero = (uint32_t)voice_nonzero;
        g_es7210_record_last_ret = (uint32_t)ESP_OK;
        g_es7210_record_first0 = first_voice0;
        g_es7210_record_first1 = first_voice1;
        g_es7210_record_first2 = first_voice2;
        g_es7210_record_first3 = first_voice3;

        ESP_LOGI(TAG,
                 "S73B mic stats bytes=%u pairs=%d L_peak=%ld L_nonzero=%d R_peak=%ld R_nonzero=%d use=%s voice_peak16=%ld nonzero=%d first=%d,%d,%d,%d",
                 (unsigned)bytes_read,
                 pairs,
                 (long)peak_l,
                 nonzero_l,
                 (long)peak_r,
                 nonzero_r,
                 use_right ? "R" : "L",
                 (long)voice_peak16,
                 voice_nonzero,
                 first_voice0,
                 first_voice1,
                 first_voice2,
                 first_voice3);

        OhosAudioEs7210RecordStatsDelayStopAware(500);
    }

    ESP_LOGI(TAG, "S71E ES7210 record stats task stop requested");

    free(buf);
    buf = NULL;

    g_es7210_record_task_started = 0;
    g_es7210_record_stop_req = 0;
    return;
}

uint32_t OhosAudioEs7210StartRecordStatsTest(void)
{
    if (g_es7210_record_task_started) {
        ESP_LOGI(TAG, "ES7210 record stats task already started");
        return 0;
    }

    g_es7210_record_stop_req = 0;
    UINT32 ret = OhosLiteosCreateTask("ohos_es7210_record",
                                      OhosAudioEs7210RecordStatsTask,
                                      NULL,
                                      OHOS_ES7210_TASK_PRIO,
                                      OHOS_ES7210_TASK_STACK,
                                      NULL);
    ESP_LOGI(TAG, "ES7210 record stats LiteOS task create ret=%u", (unsigned)ret);

    if (ret != LOS_OK) {
        g_es7210_record_task_started = 0;
        return 1;
    }

    g_es7210_record_task_started = 1;
    return 0;
}


static volatile uint32_t g_es7210_loopback_task_started = 0;
static volatile uint32_t g_es7210_loopback_stop_req = 0;


static void OhosAudioEs7210LoopbackDelayStopAware(uint32_t total_ms)
{
    uint32_t elapsed = 0;

    while (elapsed < total_ms && !g_es7210_loopback_stop_req) {
        OhosLiteosDelayMs(50);
        elapsed += 50;
    }
}

static void OhosAudioEs7210LoopbackTask(void *arg)
{
    ESP_LOGI(TAG, "ES7210->ES8311 half-duplex record-play task start");
    g_es7210_loopback_stop_req = 0; /* S72A loopback stop flag clear */

    esp_err_t ret = bsp_extra_codec_init();
        ESP_LOGI(TAG, "half-duplex bsp_extra_codec_init ret=%s", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        g_es7210_loopback_task_started = 0;
        return;
    }

    /*
     * S54B half-duplex:
     * Phase A: record microphone for about 1.2s, speaker does not output.
     * Phase B: play recorded voice through speaker, microphone data is not fed back in real time.
     */
    (void)bsp_extra_codec_volume_set(80, NULL);

    const size_t chunk_size = 2400;
    const uint32_t record_chunks = 32; /* 32 * 2400 bytes ~= 1.2s at 16kHz stereo 16-bit */
    const size_t record_bytes = chunk_size * record_chunks;

    uint8_t *audio_buf_u8 = (uint8_t *)malloc(record_bytes);
    if (audio_buf_u8 == NULL) {
        ESP_LOGE(TAG, "half-duplex malloc failed, bytes=%u", (unsigned)record_bytes);
        g_es7210_loopback_task_started = 0;
        return;
    }

    int16_t *audio_buf = (int16_t *)audio_buf_u8;
    const int32_t voice_gain = 24;
    uint32_t cycle = 0;

    while (!g_es7210_loopback_stop_req) {
        size_t offset = 0;
        int32_t record_peak = 0;
        int record_nonzero = 0;

        ESP_LOGI(TAG, "S72D half-duplex cycle=%u WAIT voice trigger", (unsigned)cycle);

        /*
         * S72D VAD:
         * - continuously read short chunks
         * - do not save/play until voice peak crosses start threshold
         * - once voice starts, keep recording until several silent chunks
         */
        const int32_t vad_start_peak = 700;
        const int32_t vad_keep_peak = 300;
        const uint32_t vad_silence_stop_chunks = 10; /* about 375ms with 2400-byte chunks */
        const uint32_t vad_wait_chunks = 160;        /* about 6s max wait before refreshing loop */

        uint32_t vad_silence_chunks = 0;
        uint32_t vad_chunks = 0;
        int vad_active = 0;
        int last_use_right = 0;

        for (uint32_t n = 0;
             n < vad_wait_chunks && !g_es7210_loopback_stop_req;
             n++) {
            size_t bytes_read = 0;

            /*
             * Before VAD triggers, reuse buffer start as a temporary monitor area.
             * After VAD triggers, append chunks at current offset.
             */
            uint8_t *dst = vad_active ? (audio_buf_u8 + offset) : audio_buf_u8;

            ret = bsp_extra_i2s_read(dst, chunk_size, &bytes_read, 1000);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "half-duplex read failed ret=%s", esp_err_to_name(ret));
                OhosAudioEs7210LoopbackDelayStopAware(100);
                continue;
            }

            int samples16 = bytes_read / sizeof(int16_t);
            int16_t *chunk = (int16_t *)dst;

            int32_t peak_l = 0;
            int32_t peak_r = 0;
            int nonzero_l = 0;
            int nonzero_r = 0;

            for (int i = 0; i + 1 < samples16; i += 2) {
                int32_t l0 = chunk[i];
                int32_t r0 = chunk[i + 1];

                int32_t al = l0 < 0 ? -l0 : l0;
                int32_t ar = r0 < 0 ? -r0 : r0;

                if (al > peak_l) {
                    peak_l = al;
                }
                if (ar > peak_r) {
                    peak_r = ar;
                }
                if (l0 != 0) {
                    nonzero_l++;
                }
                if (r0 != 0) {
                    nonzero_r++;
                }
            }

            int use_right = (peak_r > peak_l * 2) || (nonzero_r > nonzero_l * 2);
            last_use_right = use_right;

            int32_t chunk_peak = 0;
            int chunk_nonzero = 0;

            for (int i = 0; i + 1 < samples16; i += 2) {
                int32_t v = use_right ? chunk[i + 1] : chunk[i];

                if (v > -8 && v < 8) {
                    v = 0;
                }

                int32_t out = v * voice_gain;
                int16_t y = OhosAudioClamp16(out);

                chunk[i] = y;
                chunk[i + 1] = y;

                int32_t av = y < 0 ? -(int32_t)y : (int32_t)y;
                if (av > chunk_peak) {
                    chunk_peak = av;
                }
                if (y != 0) {
                    chunk_nonzero++;
                }
            }

            if (!vad_active) {
                if (chunk_peak >= vad_start_peak && chunk_nonzero > 20) {
                    vad_active = 1;
                    vad_silence_chunks = 0;
                    offset = bytes_read;
                    record_peak = chunk_peak;
                    record_nonzero = chunk_nonzero;
                    vad_chunks = 1;

                    ESP_LOGI(TAG,
                             "S72D VAD start cycle=%u chunk=%u peak=%ld nonzero=%d use=%s L_peak=%ld R_peak=%ld",
                             (unsigned)cycle,
                             (unsigned)n,
                             (long)chunk_peak,
                             chunk_nonzero,
                             use_right ? "R" : "L",
                             (long)peak_l,
                             (long)peak_r);
                } else if ((n % 20) == 0) {
                    ESP_LOGI(TAG,
                             "S72D VAD waiting cycle=%u chunk=%u peak=%ld nonzero=%d use=%s L_peak=%ld R_peak=%ld",
                             (unsigned)cycle,
                             (unsigned)n,
                             (long)chunk_peak,
                             chunk_nonzero,
                             use_right ? "R" : "L",
                             (long)peak_l,
                             (long)peak_r);
                }

                continue;
            }

            vad_chunks++;

            if (chunk_peak > record_peak) {
                record_peak = chunk_peak;
            }
            record_nonzero += chunk_nonzero;

            if (chunk_peak < vad_keep_peak) {
                vad_silence_chunks++;
            } else {
                vad_silence_chunks = 0;
            }

            offset += bytes_read;

            if (offset + chunk_size > record_bytes) {
                ESP_LOGI(TAG, "S72D VAD stop: buffer full");
                break;
            }

            if (vad_chunks >= 4 && vad_silence_chunks >= vad_silence_stop_chunks) {
                ESP_LOGI(TAG,
                         "S72D VAD stop: silence chunks=%u",
                         (unsigned)vad_silence_chunks);
                break;
            }
        }

        if (offset == 0 || !vad_active) {
            ESP_LOGI(TAG,
                     "S72D VAD no voice detected in cycle=%u, skip playback",
                     (unsigned)cycle);
            cycle++;
            OhosAudioEs7210LoopbackDelayStopAware(100);
            continue;
        }

        ESP_LOGI(TAG,
                 "S72D half-duplex cycle=%u RECORD done bytes=%u gain=%ld peak16=%ld nonzero=%d use=%s",
                 (unsigned)cycle,
                 (unsigned)offset,
                 (long)voice_gain,
                 (long)record_peak,
                 record_nonzero,
                 last_use_right ? "R" : "L");

        OhosAudioEs7210LoopbackDelayStopAware(80);

        ESP_LOGI(TAG, "half-duplex cycle=%u PLAY begin", (unsigned)cycle);

        size_t played = 0;
        while (played < offset && !g_es7210_loopback_stop_req) {
            size_t todo = offset - played;
            if (todo > chunk_size) {
                todo = chunk_size;
            }

            size_t bytes_written = 0;
            ret = bsp_extra_i2s_write(audio_buf_u8 + played, todo, &bytes_written, 1000);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "half-duplex write failed ret=%s", esp_err_to_name(ret));
                break;
            }

            played += bytes_written;
            if (bytes_written == 0) {
                OhosLiteosDelayMs(10);
            }
        }

        ESP_LOGI(TAG,
                 "half-duplex cycle=%u PLAY done written=%u",
                 (unsigned)cycle,
                 (unsigned)played);

        cycle++;
        OhosAudioEs7210LoopbackDelayStopAware(300);
    }

    ESP_LOGI(TAG, "S72A ES7210 loopback task stop requested");

    free(audio_buf_u8);
    audio_buf_u8 = NULL;

    g_es7210_loopback_task_started = 0;
    g_es7210_loopback_stop_req = 0;
    return;
}


uint32_t OhosAudioEs7210StartLoopbackTest(void)
{
    if (g_es7210_loopback_task_started) {
        ESP_LOGI(TAG, "ES7210 loopback task already started");
        return 0;
    }

    g_es7210_loopback_stop_req = 0;

    UINT32 ret = OhosLiteosCreateTask("ohos_audio_loopback",
                                      OhosAudioEs7210LoopbackTask,
                                      NULL,
                                      OHOS_ES7210_TASK_PRIO,
                                      OHOS_ES7210_TASK_STACK,
                                      NULL);
    ESP_LOGI(TAG, "ES7210 loopback LiteOS task create ret=%u", (unsigned)ret);

    if (ret != LOS_OK) {
        g_es7210_loopback_task_started = 0;
        return 1;
    }

    g_es7210_loopback_task_started = 1;
    return 0;
}



uint32_t OhosAudioEs7210StopLoopbackTest(void)
{
    if (!g_es7210_loopback_task_started) {
        ESP_LOGI(TAG, "S72A ES7210 loopback stop skipped: not started");
        return 0;
    }

    g_es7210_loopback_stop_req = 1;
    ESP_LOGI(TAG, "S72A ES7210 loopback stop requested");
    return 0;
}

uint32_t OhosAudioEs7210StopRecordStats(void)
{
    if (!g_es7210_record_task_started) {
        ESP_LOGI(TAG, "S71E ES7210 record stats stop skipped: not started");
        return 0;
    }

    g_es7210_record_stop_req = 1;
    ESP_LOGI(TAG, "S71E ES7210 record stats stop requested");
    return 0;
}

uint32_t OhosAudioEs7210PrepareForDownlink(uint32_t settleMs)
{
    uint32_t stopped = 0;

    g_es7210_pcm_stop_for_downlink = 1;

    if (g_es7210_record_task_started) {
        g_es7210_record_stop_req = 1;
        stopped = 1;
    }
    if (g_es7210_loopback_task_started) {
        g_es7210_loopback_stop_req = 1;
        stopped = 1;
    }
    if (g_es7210_uplink_task_started || g_es7210_uplink_running) {
        OhosAudioEs7210StopUplinkCapture();
        stopped = 1;
    }

    if (g_es7210_pcm_init_ok) {
        esp_err_t ret = bsp_extra_codec_dev_stop();
        g_es7210_pcm_init_ok = 0;
        g_es7210_pcm_need_resume = 1;
        stopped = 1;
        ESP_LOGI(TAG,
                 "prepare downlink stop rx codec ret=%s readOk=%u readFail=%u",
                 esp_err_to_name(ret),
                 (unsigned)g_es7210_pcm_read_ok,
                 (unsigned)g_es7210_pcm_read_fail);
    }

    if (settleMs > 0U && stopped) {
        OhosLiteosDelayMs(settleMs);
    }

    return 0;
}

void OhosAudioEs7210GetRecordStatsSnapshot(OhosAudioEs7210RecordStatsSnapshot *snap)
{
    if (snap == NULL) {
        return;
    }

    snap->task_started = g_es7210_record_task_started;
    snap->seq = g_es7210_record_seq;
    snap->bytes_read = g_es7210_record_bytes_read;
    snap->samples = g_es7210_record_samples;
    snap->peak = g_es7210_record_peak;
    snap->nonzero = g_es7210_record_nonzero;
    snap->last_ret = g_es7210_record_last_ret;
    snap->first0 = g_es7210_record_first0;
    snap->first1 = g_es7210_record_first1;
    snap->first2 = g_es7210_record_first2;
    snap->first3 = g_es7210_record_first3;
}

void OhosAudioEs7210GetPcmFrameSnapshot(OhosAudioEs7210PcmFrameSnapshot *snap)
{
    if (snap == NULL) {
        return;
    }

    snap->init_ok = g_es7210_pcm_init_ok;
    snap->read_ok = g_es7210_pcm_read_ok;
    snap->read_fail = g_es7210_pcm_read_fail;
    snap->bytes_read = g_es7210_pcm_bytes_read;
    snap->samples = g_es7210_pcm_samples;
    snap->peak = g_es7210_pcm_peak;
    snap->nonzero = g_es7210_pcm_nonzero;
    snap->use_right = g_es7210_pcm_use_right;
    snap->last_ret = g_es7210_pcm_last_ret;
    snap->partial_read = g_es7210_pcm_partial_read;
    snap->accum_timeout = g_es7210_pcm_accum_timeout;
}
