#include "ohos_board_i2c.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "los_mux.h"
#include "ohos_liteos_media_task.h"

#define OHOS_BOARD_I2C_TAG "OHOS-BOARD-I2C"

static i2c_master_bus_handle_t g_board_i2c_bus = NULL;
static UINT32 g_board_i2c_mux = 0;
static uint32_t g_board_i2c_mux_ready = 0;


static void OhosBoardI2cScanBus(i2c_master_bus_handle_t bus)
{
    if (bus == NULL) {
        ESP_LOGW(OHOS_BOARD_I2C_TAG, "I2C scan skipped: bus is NULL");
        return;
    }

    ESP_LOGI(OHOS_BOARD_I2C_TAG, "I2C scan begin on port=%d scl=%d sda=%d",
             (int)OHOS_BOARD_I2C_PORT,
             (int)OHOS_BOARD_I2C_SCL_IO,
             (int)OHOS_BOARD_I2C_SDA_IO);

    int found = 0;
    for (uint16_t addr = 0x03; addr <= 0x77; ++addr) {
        esp_err_t ret = i2c_master_probe(bus, addr, 50);
        if (ret == ESP_OK) {
            ESP_LOGI(OHOS_BOARD_I2C_TAG, "I2C probe addr=0x%02x OK", addr);
            found++;
        }
    }

    ESP_LOGI(OHOS_BOARD_I2C_TAG, "I2C scan done, found=%d", found);
}

esp_err_t OhosBoardPeripheralsResetForFinalBoard(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << OHOS_BOARD_TP_RST_IO) | (1ULL << OHOS_BOARD_CSI_RST_IO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(OHOS_BOARD_I2C_TAG, "config TP/CSI reset gpio failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(OHOS_BOARD_I2C_TAG, "final board reset begin: TP_RST=%d CSI_RST=%d",
             (int)OHOS_BOARD_TP_RST_IO,
             (int)OHOS_BOARD_CSI_RST_IO);

    gpio_set_level(OHOS_BOARD_TP_RST_IO, 0);
    gpio_set_level(OHOS_BOARD_CSI_RST_IO, 0);
    OhosLiteosDelayMs(20);

    gpio_set_level(OHOS_BOARD_TP_RST_IO, 1);
    gpio_set_level(OHOS_BOARD_CSI_RST_IO, 1);
    OhosLiteosDelayMs(200);

    ESP_LOGI(OHOS_BOARD_I2C_TAG, "final board reset done");
    return ESP_OK;
}

void OhosBoardI2cScanSharedBus(void)
{
    OhosBoardI2cScanBus(g_board_i2c_bus);
}


esp_err_t OhosBoardI2cGetSharedBus(i2c_master_bus_handle_t *out_bus)
{
    if (out_bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_board_i2c_mux_ready) {
        UINT32 mux_ret = LOS_MuxCreate(&g_board_i2c_mux);
        if (mux_ret != LOS_OK) {
            ESP_LOGE(OHOS_BOARD_I2C_TAG, "create LiteOS mux failed ret=%u", mux_ret);
            return ESP_FAIL;
        }
        g_board_i2c_mux_ready = 1;
    }

    (void)LOS_MuxPend(g_board_i2c_mux, LOS_WAIT_FOREVER);

    if (g_board_i2c_bus == NULL) {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = OHOS_BOARD_I2C_PORT,
            .sda_io_num = OHOS_BOARD_I2C_SDA_IO,
            .scl_io_num = OHOS_BOARD_I2C_SCL_IO,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };

        esp_err_t ret = i2c_new_master_bus(&bus_cfg, &g_board_i2c_bus);
        if (ret != ESP_OK) {
            ESP_LOGE(OHOS_BOARD_I2C_TAG,
                     "create shared I2C bus failed port=%d scl=%d sda=%d ret=%s",
                     (int)OHOS_BOARD_I2C_PORT,
                     (int)OHOS_BOARD_I2C_SCL_IO,
                     (int)OHOS_BOARD_I2C_SDA_IO,
                     esp_err_to_name(ret));
            (void)LOS_MuxPost(g_board_i2c_mux);
            return ret;
        }

        ESP_LOGI(OHOS_BOARD_I2C_TAG,
                 "shared I2C bus created port=%d scl=%d sda=%d",
                 (int)OHOS_BOARD_I2C_PORT,
                 (int)OHOS_BOARD_I2C_SCL_IO,
                 (int)OHOS_BOARD_I2C_SDA_IO);
    } else {
        ESP_LOGI(OHOS_BOARD_I2C_TAG, "reuse shared I2C bus");
    }

    *out_bus = g_board_i2c_bus;

    (void)LOS_MuxPost(g_board_i2c_mux);
    return ESP_OK;
}

i2c_master_bus_handle_t OhosBoardI2cGetCachedBus(void)
{
    return g_board_i2c_bus;
}
