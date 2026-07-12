#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_st7123.h"
#include "ohos_liteos_media_task.h"

#define OHOS_TOUCH_TAG              "OHOS-TOUCH-ST7123"

#define OHOS_TOUCH_I2C_PORT         I2C_NUM_0
#define OHOS_TOUCH_I2C_SDA          GPIO_NUM_7
#define OHOS_TOUCH_I2C_SCL          GPIO_NUM_8
#define OHOS_TOUCH_I2C_FREQ_HZ      400000

#define OHOS_TOUCH_H_RES            480
#define OHOS_TOUCH_V_RES            800

/* Follow OSPTEK MIPI DSI example: touch reset/int are not controlled by GPIO. */
#define OHOS_TOUCH_RST              GPIO_NUM_NC
#define OHOS_TOUCH_INT              GPIO_NUM_NC
#define OHOS_TOUCH_TASK_PRIO        25
#define OHOS_TOUCH_TASK_STACK       0x1000

static i2c_master_bus_handle_t s_touch_i2c_bus = NULL;
static esp_lcd_panel_io_handle_t s_touch_io = NULL;
static esp_lcd_touch_handle_t s_touch_handle = NULL;
static volatile uint32_t g_touch_probe_started = 0;

static esp_err_t OhosTouchI2cInit(void)
{
    if (s_touch_i2c_bus != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = OHOS_TOUCH_I2C_PORT,
        .sda_io_num = OHOS_TOUCH_I2C_SDA,
        .scl_io_num = OHOS_TOUCH_I2C_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_bus_conf, &s_touch_i2c_bus);
    ESP_LOGI(OHOS_TOUCH_TAG,
             "i2c_new_master_bus port=%d sda=%d scl=%d freq=%d ret=%s",
             (int)OHOS_TOUCH_I2C_PORT,
             (int)OHOS_TOUCH_I2C_SDA,
             (int)OHOS_TOUCH_I2C_SCL,
             OHOS_TOUCH_I2C_FREQ_HZ,
             esp_err_to_name(ret));

    return ret;
}

static esp_err_t OhosTouchSt7123Init(void)
{
    esp_err_t ret = OhosTouchI2cInit();
    if (ret != ESP_OK) {
        return ret;
    }

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = OHOS_TOUCH_H_RES,
        .y_max = OHOS_TOUCH_V_RES,
        .rst_gpio_num = OHOS_TOUCH_RST,
        .int_gpio_num = OHOS_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
    };

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    tp_io_config.scl_speed_hz = OHOS_TOUCH_I2C_FREQ_HZ;

    ret = esp_lcd_new_panel_io_i2c(s_touch_i2c_bus, &tp_io_config, &s_touch_io);
    ESP_LOGI(OHOS_TOUCH_TAG, "esp_lcd_new_panel_io_i2c ret=%s", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        return ret;
    }

    ret = esp_lcd_touch_new_i2c_st7123(s_touch_io, &tp_cfg, &s_touch_handle);
    ESP_LOGI(OHOS_TOUCH_TAG,
             "esp_lcd_touch_new_i2c_st7123 ret=%s x_max=%d y_max=%d rst=%d int=%d",
             esp_err_to_name(ret),
             OHOS_TOUCH_H_RES,
             OHOS_TOUCH_V_RES,
             (int)OHOS_TOUCH_RST,
             (int)OHOS_TOUCH_INT);

    return ret;
}

static void OhosTouchProbeTask(void *arg)
{
    (void)arg;

    ESP_LOGI(OHOS_TOUCH_TAG, "touch probe task start");

    esp_err_t ret = OhosTouchSt7123Init();
    if (ret != ESP_OK) {
        ESP_LOGE(OHOS_TOUCH_TAG, "touch init failed ret=%s", esp_err_to_name(ret));
        g_touch_probe_started = 0;
        return;
    }

    ESP_LOGI(OHOS_TOUCH_TAG, "touch init done, start polling");

    uint32_t last_print = 0;

    while (1) {
        ret = esp_lcd_touch_read_data(s_touch_handle);
        if (ret != ESP_OK) {
            ESP_LOGW(OHOS_TOUCH_TAG, "read_data ret=%s", esp_err_to_name(ret));
            OhosLiteosDelayMs(200);
            continue;
        }

        uint16_t x[5] = {0};
        uint16_t y[5] = {0};
        uint16_t strength[5] = {0};
        uint8_t point_num = 0;

        bool pressed = esp_lcd_touch_get_coordinates(
            s_touch_handle, x, y, strength, &point_num, 5);

        if (pressed && point_num > 0) {
            ESP_LOGI(OHOS_TOUCH_TAG,
                     "pressed=%d points=%u x0=%u y0=%u strength0=%u",
                     pressed ? 1 : 0,
                     (unsigned)point_num,
                     (unsigned)x[0],
                     (unsigned)y[0],
                     (unsigned)strength[0]);
            last_print = 0;
        } else {
            if ((last_print++ % 50U) == 0U) {
                ESP_LOGI(OHOS_TOUCH_TAG, "no touch");
            }
        }

        OhosLiteosDelayMs(20);
    }
}

uint32_t OhosTouchSt7123ProbeStart(void)
{
    if (g_touch_probe_started) {
        ESP_LOGI(OHOS_TOUCH_TAG, "touch probe already started");
        return 0;
    }

    UINT32 ret = OhosLiteosCreateTask("ohos_touch_st7123",
                                      OhosTouchProbeTask,
                                      NULL,
                                      OHOS_TOUCH_TASK_PRIO,
                                      OHOS_TOUCH_TASK_STACK,
                                      NULL);

    ESP_LOGI(OHOS_TOUCH_TAG, "touch probe LiteOS task create ret=%u", (unsigned)ret);

    if (ret != LOS_OK) {
        g_touch_probe_started = 0;
        return 1;
    }

    g_touch_probe_started = 1;
    return 0;
}
