/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include "sdkconfig.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "driver/i2c_master.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_st7123.h"
#include "esp_lv_adapter_input.h"
#include "esp_lv_adapter.h"
#include "hw_init.h"
#include "ohos_board_i2c.h"
#include "ohos_uart_link_ui.h"
#include "ohos_audio_es7210_port.h"
#include "ohos_liteos_media_task.h"
#include "los_mux.h"
#include "osptek_app_video.h"
#include "lvgl_lfs_font.h"
#include "p4_system_font.h"
#include "p4_desktop_page.h"
#include "p4_dialog_page.h"
#include "p4_sensor_page.h"

#ifndef OHOS_MAYBE_UNUSED
#define OHOS_MAYBE_UNUSED __attribute__((unused))
#endif

/*
 * S63A layout:
 * Top 480x720 is direct camera output.
 * Bottom 480x80 is LVGL control bar.
 */
#ifndef OHOS_ALIGN_UP
#define OHOS_ALIGN_UP(num, align)    (((num) + ((align) - 1)) & ~((align) - 1))
#endif

#ifndef OHOS_CAMERA_DIRECT_VIEW_W
#define OHOS_CAMERA_DIRECT_VIEW_W    480
#endif

#ifndef OHOS_CAMERA_DIRECT_VIEW_H
#define OHOS_CAMERA_DIRECT_VIEW_H    720
#endif

#ifndef OHOS_CAMERA_BOTTOM_BAR_H
#define OHOS_CAMERA_BOTTOM_BAR_H     80
#endif

#ifndef OHOS_S63_DIRECT_BYTE_SWAP
#define OHOS_S63_DIRECT_BYTE_SWAP 0
#endif

#ifndef OHOS_S63_DIRECT_RGB_SWAP
#define OHOS_S63_DIRECT_RGB_SWAP 0
#endif

#ifndef OHOS_S63_DIRECT_MIN_FRAME_INTERVAL_MS
#define OHOS_S63_DIRECT_MIN_FRAME_INTERVAL_MS 0
#endif

#ifndef OHOS_S63_DIRECT_ROTATION
#define OHOS_S63_DIRECT_ROTATION PPA_SRM_ROTATION_ANGLE_180
#endif

#ifndef OHOS_S63_SOFTWARE_ROTATE_180
#define OHOS_S63_SOFTWARE_ROTATE_180 0
#endif

/*
 * S66A:
 * RGB565 color fine tune for slight green tint.
 * Keep S64C fast RGB565 OSPTEK video path, only adjust final 480x720 RGB565 buffer.
 * Tune these numbers if needed:
 * - green smaller => less green
 * - red/blue larger => warmer / less green
 */
#ifndef OHOS_S66_COLOR_TUNE_ENABLE
#define OHOS_S66_COLOR_TUNE_ENABLE 0
#endif

#ifndef OHOS_S66_R_GAIN_NUM
#define OHOS_S66_R_GAIN_NUM 38
#endif
#ifndef OHOS_S66_R_GAIN_DEN
#define OHOS_S66_R_GAIN_DEN 32
#endif

#ifndef OHOS_S66_G_GAIN_NUM
#define OHOS_S66_G_GAIN_NUM 44
#endif
#ifndef OHOS_S66_G_GAIN_DEN
#define OHOS_S66_G_GAIN_DEN 64
#endif

#ifndef OHOS_S66_B_GAIN_NUM
#define OHOS_S66_B_GAIN_NUM 38
#endif
#ifndef OHOS_S66_B_GAIN_DEN
#define OHOS_S66_B_GAIN_DEN 32
#endif

#include "esp_cache.h"
#include "driver/ppa.h"

/*
 * S61A fix:
 * Final product board shared I2C pins.
 * ST7123 touch / SC2336 camera / audio codecs share GPIO7(GPIO SDA) and GPIO8(GPIO SCL).
 */
#ifndef OHOS_LVGL_TOUCH_I2C_SDA
#define OHOS_LVGL_TOUCH_I2C_SDA 7
#endif

#ifndef OHOS_LVGL_TOUCH_I2C_SCL
#define OHOS_LVGL_TOUCH_I2C_SCL 8
#endif

#include "driver/i2c_master.h"
#include "esp_err.h"

static void example_show_gif_overlay(void);
static void ohos_lvgl_external_font_probe_timer_cb(lv_timer_t *timer);

extern uint32_t OhosCameraServiceStartRealPreview(void);

/* S60A: GIF page <-> camera page switch */
static uint32_t OhosCameraServiceStartOsptekPreview(void);







extern int ohos_liteos_bringup(bool start_scheduler);
#include "esp_lcd_ili9881c.h"
#include "esp_lcd_ek79007.h"
#include "st7102_ek79007_init_cmds.h"
#include "esp_flash_encrypt.h"

static const char *TAG = "example";

extern uint32_t OhosAudioServiceLoopbackTest(void);
extern uint32_t OhosAudioServiceStopLoopback(void);

extern uint32_t OhosAudioServiceStopSpeaker(void);

extern uint32_t OhosAudioServiceStopMicStats(void);
extern uint32_t OhosAudioServiceRecordStatsTest(void);
extern uint32_t OhosAudioServicePlayTest(void);
static void board_display_mark_on(int h_res, int v_res, int lanes, int dpi_clk_mhz, int bpp);
static void ohos_s60_show_gif_page(void);
static void ohos_touch_reset_for_page_change(void);
#if OHOS_ENABLE_P4_DESKTOP_UI
static void ohos_p4_show_desktop_page(lv_display_t *display);
#endif
static void ohos_s60_request_camera_page_async_cb(void *arg);
static void OHOS_MAYBE_UNUSED ohos_lvgl_create_touch_test_button(lv_display_t *display);
static void OhosCameraServiceStopOsptekPreview(void);
static void ohos_s60_camera_back_common(const char *source);
static void ohos_s60_camera_back_from_touch(uint16_t x, uint16_t y);

static volatile int g_board_display_on = 0;
static volatile int g_board_display_h_res = 0;
static volatile int g_board_display_v_res = 0;
static volatile int g_board_display_lanes = 0;
static volatile int g_board_display_dpi_clk_mhz = 0;
static volatile int g_board_display_bpp = 0;

static void board_display_mark_on(int h_res, int v_res, int lanes, int dpi_clk_mhz, int bpp)
{
    g_board_display_h_res = h_res;
    g_board_display_v_res = v_res;
    g_board_display_lanes = lanes;
    g_board_display_dpi_clk_mhz = dpi_clk_mhz;
    g_board_display_bpp = bpp;
    g_board_display_on = 1;
}

static void ohos_apply_system_ui_font(lv_obj_t *root)
{
    P4SystemFontApply(root);
}

static void ohos_s60_show_gif_page(void)
{
    /*
     * S68D:
     * Demo Center replaces the old GIF page.
     * Do not create legacy frame animation timer here.
     */
    ESP_LOGI(TAG, "S68D show main UI page without legacy GIF timer");

    ohos_touch_reset_for_page_change();

    lv_display_t *display = lv_display_get_default();
    if (display == NULL) {
        ESP_LOGW(TAG, "S68D lv_display_get_default returned NULL");
        return;
    }

#if OHOS_ENABLE_P4_DESKTOP_UI
    ohos_p4_show_desktop_page(display);
#else
    ohos_lvgl_create_touch_test_button(display);
#endif
}














////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD Spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ST7102 / YDP430BT009-V1 initial timing.
// Panel spec: 480xRGBx800, MIPI DSI 2 lane.
// NOTE: These porch values are first-try values. If screen is abnormal, tune timing later.
#define EXAMPLE_MIPI_DSI_DPI_CLK_MHZ  30
#define EXAMPLE_MIPI_DSI_LCD_H_RES    480
#define EXAMPLE_MIPI_DSI_LCD_V_RES    800
#define EXAMPLE_MIPI_DSI_LCD_HSYNC    10
#define EXAMPLE_MIPI_DSI_LCD_HBP      20
#define EXAMPLE_MIPI_DSI_LCD_HFP      20
#define EXAMPLE_MIPI_DSI_LCD_VSYNC    2
#define EXAMPLE_MIPI_DSI_LCD_VBP      8
#define EXAMPLE_MIPI_DSI_LCD_VFP      8

#define EXAMPLE_MIPI_DSI_LANE_NUM          2    // 2 data lanes
#define EXAMPLE_MIPI_DSI_LANE_BITRATE_MBPS 1000 // 1Gbps

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your Board Design //////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The "VDD_MIPI_DPHY" should be supplied with 2.5V, it can source from the internal LDO regulator or from external LDO chip
#define EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN       3  // LDO_VO3 is connected to VDD_MIPI_DPHY
#define EXAMPLE_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV 2500
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL           1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL          !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_BK_LIGHT                -1
#define EXAMPLE_PIN_NUM_LCD_RST                 -1

#if CONFIG_EXAMPLE_MONITOR_REFRESH_BY_GPIO
#define EXAMPLE_PIN_NUM_REFRESH_MONITOR         20  // Monitor the Refresh Rate by toggling the GPIO
#endif

#define ALIGN_UP(num, align)    (((num) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_DOWN(num, align)  ((num) & ~((align) - 1))

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your Application ///////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define EXAMPLE_LVGL_DRAW_BUF_LINES    (EXAMPLE_MIPI_DSI_LCD_V_RES / 10) // number of display lines in each draw buffer
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (12 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     16
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 30
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 10

#define OHOS_TOUCH_POLL_TASK_PRIO      15
#define OHOS_TOUCH_POLL_TASK_STACK     0x1000
#define OHOS_TOUCH_POLL_INTERVAL_MS    20
#define OHOS_CAMERA_TOUCH_PAUSE_MS     180
#define OHOS_UI_WORKER_TASK_PRIO       17
#define OHOS_UI_WORKER_TASK_STACK      0x2000
#define OHOS_CAMERA_START_TASK_PRIO    25
#define OHOS_CAMERA_START_TASK_STACK   0x2000

static UINT32 s_lvgl_api_mux = 0;
static uint32_t s_lvgl_api_mux_ready = 0;
static UINT32 s_lvgl_task_id = 0;
static UINT32 s_touch_poll_task_id = 0;

static i2c_master_bus_handle_t s_ohos_touch_i2c_bus = NULL;
static esp_lcd_panel_io_handle_t s_ohos_touch_io = NULL;
static esp_lcd_touch_handle_t s_ohos_touch = NULL;
static lv_display_t *s_ohos_main_display = NULL;

/* S63A: direct camera output path */
static esp_lcd_panel_handle_t s_ohos_display_panel = NULL;
static ppa_client_handle_t s_s63_ppa_srm_handle = NULL;
static uint8_t *s_s63_camera_lcd_buf = NULL;
static size_t s_s63_cache_line_size = 0;
static volatile int s_s63_direct_camera_enabled = 0;
static volatile int s_s60_camera_page_active = 0;
static int s_s64_osptek_video_fd = -1;
static volatile int s_s64_osptek_video_started = 0;

static uint32_t s_s63_last_draw_ms = 0;
static volatile uint32_t s_s63_touch_active_until_ms = 0;

static bool ohos_s63_is_touch_active(uint32_t now_ms)
{
    return (int32_t)(s_s63_touch_active_until_ms - now_ms) > 0;
}

static void ohos_lvgl_lock_init(void)
{
    if (!s_lvgl_api_mux_ready) {
        UINT32 ret = LOS_MuxCreate(&s_lvgl_api_mux);
        if (ret == LOS_OK) {
            s_lvgl_api_mux_ready = 1;
        }
        ESP_LOGI(TAG, "LiteOS LVGL mux create ret=%u mux=%u", ret, s_lvgl_api_mux);
    }
}

static void ohos_lvgl_lock(void)
{
    if (s_lvgl_api_mux_ready) {
        (void)LOS_MuxPend(s_lvgl_api_mux, LOS_WAIT_FOREVER);
    }
}

static void ohos_lvgl_unlock(void)
{
    if (s_lvgl_api_mux_ready) {
        (void)LOS_MuxPost(s_lvgl_api_mux);
    }
}




/*
 * S59A:
 * ST7123 is read by the S57A direct polling task.
 * LVGL input callback only reads the cached point, so it will not touch I2C.
 */
static volatile bool s_s59_touch_pressed = false;
static volatile bool s_s59_touch_press_latched = false;
static volatile bool s_s59_touch_release_latched = false;
static volatile bool s_s59_touch_suppress_until_release = false;
static volatile uint16_t s_s59_touch_x = 0;
static volatile uint16_t s_s59_touch_y = 0;
static volatile uint16_t s_s59_touch_strength = 0;

static lv_indev_t *s_ohos_touch_indev = NULL;

static void ohos_touch_reset_for_page_change(void)
{
    UINT32 intSave = LOS_IntLock();
    s_s59_touch_pressed = false;
    s_s59_touch_press_latched = false;
    s_s59_touch_release_latched = false;
    s_s59_touch_suppress_until_release = true;
    LOS_IntRestore(intSave);

    if (s_ohos_touch_indev != NULL) {
        lv_indev_reset(s_ohos_touch_indev, NULL);
    }
}

#define OHOS_LVGL_TOUCH_I2C_PORT      I2C_NUM_0
#define OHOS_LVGL_TOUCH_SDA           GPIO_NUM_7
#define OHOS_LVGL_TOUCH_SCL           GPIO_NUM_8
#define OHOS_LVGL_TOUCH_FREQ_HZ       400000
#define OHOS_LVGL_TOUCH_H_RES         480
#define OHOS_LVGL_TOUCH_V_RES         800

#ifndef OHOS_CAMERA_BACK_TOUCH_BAND
#define OHOS_CAMERA_BACK_TOUCH_BAND   120
#endif


static void ohos_lvgl_s59_cached_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    bool pressed;
    uint16_t x;
    uint16_t y;

    UINT32 intSave = LOS_IntLock();
    if (s_s59_touch_press_latched) {
        pressed = true;
        s_s59_touch_press_latched = false;
    } else if (s_s59_touch_release_latched) {
        pressed = false;
        s_s59_touch_release_latched = false;
    } else {
        pressed = s_s59_touch_pressed;
    }
    x = s_s59_touch_x;
    y = s_s59_touch_y;
    LOS_IntRestore(intSave);

    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

    static uint32_t s_s59b_read_cnt = 0;
    uint32_t read_cnt = s_s59b_read_cnt++;
    if ((pressed && ((read_cnt % 10U) == 0U)) ||
        (!pressed && ((read_cnt % 80U) == 0U))) {
        ESP_LOGI(TAG,
                 "S59B LVGL cached read pressed=%d x=%u y=%u state=%s",
                 pressed ? 1 : 0,
                 (unsigned)x,
                 (unsigned)y,
                 pressed ? "PRESSED" : "RELEASED");
    }
}

static void ohos_st7123_direct_poll_task(void *arg);

static esp_err_t ohos_lvgl_st7123_touch_init(void)
{
    if (s_ohos_touch != NULL) {
        return ESP_OK;
    }

    if (s_ohos_touch_i2c_bus == NULL) {
        /*
         * S61A:
         * Touch, camera, TP, audio share GPIO7/GPIO8 I2C.
         * Do not create a private I2C0 bus here, otherwise CameraService
         * will fail with "I2C bus id(0) has already been acquired".
         */
        esp_err_t ret = OhosBoardI2cGetSharedBus(&s_ohos_touch_i2c_bus);
        ESP_LOGI(TAG,
                 "S61A OHOS LVGL touch use shared I2C sda=%d scl=%d ret=%s bus=%p",
                 OHOS_LVGL_TOUCH_I2C_SDA,
                 OHOS_LVGL_TOUCH_I2C_SCL,
                 esp_err_to_name(ret),
                 s_ohos_touch_i2c_bus);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    tp_io_config.scl_speed_hz = OHOS_LVGL_TOUCH_FREQ_HZ;

    esp_err_t ret = esp_lcd_new_panel_io_i2c(s_ohos_touch_i2c_bus, &tp_io_config, &s_ohos_touch_io);
    ESP_LOGI(TAG, "OHOS LVGL touch panel io ret=%s", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        return ret;
    }

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = OHOS_LVGL_TOUCH_H_RES,
        .y_max = OHOS_LVGL_TOUCH_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
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

    ret = esp_lcd_touch_new_i2c_st7123(s_ohos_touch_io, &tp_cfg, &s_ohos_touch);
    ESP_LOGI(TAG, "OHOS LVGL ST7123 touch init ret=%s", esp_err_to_name(ret));

    /*
     * S57A:
     * Keep the stable manual LVGL display path.
     * Do not register touch through esp_lv_adapter here.
     * First verify ST7123 coordinates by a low-frequency direct poll task.
     */
    UINT32 poll_task_ret = OhosLiteosCreateTask("s57a_touch_poll",
                                                ohos_st7123_direct_poll_task,
                                                NULL,
                                                OHOS_TOUCH_POLL_TASK_PRIO,
                                                OHOS_TOUCH_POLL_TASK_STACK,
                                                &s_touch_poll_task_id);
    ESP_LOGI(TAG, "S57A direct ST7123 poll LiteOS task create ret=%u taskId=%u",
             (unsigned)poll_task_ret,
             (unsigned)s_touch_poll_task_id);
    return ESP_OK;

    return ret;
}

static void ohos_lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    if (s_ohos_touch == NULL) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    esp_err_t ret = esp_lcd_touch_read_data(s_ohos_touch);
    if (ret != ESP_OK) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    uint16_t x[1] = {0};
    uint16_t y[1] = {0};
    uint16_t strength[1] = {0};
    uint8_t point_num = 0;

    static uint32_t s_touch_read_cnt = 0;
    static uint32_t s_touch_press_cnt = 0;

    bool pressed = esp_lcd_touch_get_coordinates(s_ohos_touch, x, y, strength, &point_num, 1);
    if (pressed && point_num > 0) {
        data->point.x = x[0];
        data->point.y = y[0];
        data->state = LV_INDEV_STATE_PRESSED;

        if ((s_touch_press_cnt++ % 10U) == 0U) {
            ESP_LOGI(TAG,
                     "OHOS LVGL touch read pressed x=%u y=%u strength=%u points=%u",
                     (unsigned)x[0],
                     (unsigned)y[0],
                     (unsigned)strength[0],
                     (unsigned)point_num);
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;

        if ((s_touch_read_cnt++ % 200U) == 0U) {
            ESP_LOGI(TAG, "OHOS LVGL touch read released");
        }
    }
}

static esp_err_t ohos_lvgl_adapter_custom_touch_read(esp_lcd_touch_handle_t tp,
                                                      esp_lcd_touch_point_data_t *points,
                                                      uint8_t *count,
                                                      uint8_t max_count,
                                                      void *user_ctx)
{
    (void)user_ctx;

    static uint32_t s_custom_release_cnt = 0;
    static uint32_t s_custom_press_cnt = 0;

    if (tp == NULL || points == NULL || count == NULL || max_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *count = 0;

    esp_err_t ret = esp_lcd_touch_read_data(tp);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "OHOS adapter custom touch read_data failed ret=%s", esp_err_to_name(ret));
        return ret;
    }

    uint16_t x[1] = {0};
    uint16_t y[1] = {0};
    uint16_t strength[1] = {0};
    uint8_t point_num = 0;

    bool pressed = esp_lcd_touch_get_coordinates(tp, x, y, strength, &point_num, 1);
    if (pressed && point_num > 0) {
        points[0].x = x[0];
        points[0].y = y[0];
        points[0].strength = strength[0];
        *count = 1;

        if ((s_custom_press_cnt++ % 5U) == 0U) {
            ESP_LOGI(TAG,
                     "OHOS adapter custom touch PRESSED x=%u y=%u strength=%u point_num=%u",
                     (unsigned)x[0],
                     (unsigned)y[0],
                     (unsigned)strength[0],
                     (unsigned)point_num);
        }
    } else {
        if ((s_custom_release_cnt++ % 200U) == 0U) {
            ESP_LOGI(TAG, "OHOS adapter custom touch released");
        }
    }

    return ESP_OK;
}

static void ohos_touch_test_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        ESP_LOGI(TAG, "S60A start camera button event: PRESSED");
    } else if (code == LV_EVENT_RELEASED) {
        ESP_LOGI(TAG, "S60A start camera button event: RELEASED");
    } else if (code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S60A start camera button clicked");
        lv_async_call(ohos_s60_request_camera_page_async_cb, NULL);
    } else {
        ESP_LOGI(TAG, "S60A start camera button event code=%d", (int)code);
    }
}


/* S68A: Demo Center placeholder pages */
static void ohos_s68_demo_back_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S68A demo placeholder back clicked");
        ohos_s60_show_gif_page();
    }
}

static void ohos_s68_show_placeholder_page(const char *title_text, const char *detail_text)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    ohos_apply_system_ui_font(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, title_text);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t *detail = lv_label_create(scr);
    lv_label_set_text(detail, detail_text);
    lv_obj_set_width(detail, 420);
    lv_obj_align(detail, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t *back = lv_button_create(scr);
    lv_obj_set_size(back, 220, 70);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -35);
    lv_obj_add_event_cb(back, ohos_s68_demo_back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(back);
    lv_label_set_text(label, "Back");
    lv_obj_center(label);
}


/* S70B: real Speaker Test page */
static lv_obj_t *s_s70_speaker_status_label = NULL;
static volatile uint32_t s_s70_speaker_task_running = 0;
static volatile uint32_t s_s70_speaker_last_ret = 0xffffffffU;

static void ohos_s70_speaker_set_status(const char *text)
{
    if (s_s70_speaker_status_label != NULL &&
        lv_obj_is_valid(s_s70_speaker_status_label)) {
        lv_label_set_text(s_s70_speaker_status_label, text);
    }
}

static void ohos_s70_speaker_play_done_async_cb(void *arg)
{
    (void)arg;

    char buf[160];
    snprintf(buf, sizeof(buf),
             "Speaker Test Result:\n"
             "Play ret: %u\n"
             "If you heard sound, ES8311 + I2S TX + speaker path is OK.",
             (unsigned int)s_s70_speaker_last_ret);

    ohos_s70_speaker_set_status(buf);

    ESP_LOGI(TAG,
             "S70B speaker play done ret=%u",
             (unsigned int)s_s70_speaker_last_ret);
}

static void ohos_s70_speaker_play_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "S70B speaker play task start");

    uint32_t ret = OhosAudioServicePlayTest();

    s_s70_speaker_last_ret = ret;
    s_s70_speaker_task_running = 0;

    lv_async_call(ohos_s70_speaker_play_done_async_cb, NULL);

    ESP_LOGI(TAG, "S70B speaker play task exit ret=%u", (unsigned int)ret);

    return;
}

static void ohos_s70_speaker_play_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_s70_speaker_task_running) {
        ESP_LOGW(TAG, "S70B speaker play already running");
        ohos_s70_speaker_set_status("Speaker Test:\nAlready playing, please wait...");
        return;
    }

    s_s70_speaker_task_running = 1;
    s_s70_speaker_last_ret = 0xffffffffU;

    ohos_s70_speaker_set_status("Speaker Test:\nPlaying canon.pcm...\nPlease listen to the speaker.");

    UINT32 ret = OhosLiteosCreateTask("s70_speaker_play",
                                      ohos_s70_speaker_play_task,
                                      NULL,
                                      OHOS_UI_WORKER_TASK_PRIO,
                                      OHOS_UI_WORKER_TASK_STACK,
                                      NULL);

    ESP_LOGI(TAG, "S70B speaker play LiteOS task create ret=%u", (unsigned)ret);

    if (ret != LOS_OK) {
        s_s70_speaker_task_running = 0;
        s_s70_speaker_last_ret = 1;
        ohos_s70_speaker_set_status("Speaker Test:\nCreate play task failed.");
    }
}

static void ohos_s70_speaker_back_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S70B Speaker Test back clicked");

        ESP_LOGI(TAG, "S70C Speaker Test stop play before Back");
        (void)OhosAudioServiceStopSpeaker();

        s_s70_speaker_status_label = NULL;
        ohos_s60_show_gif_page();
    }
}

static void ohos_s70_show_speaker_page(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    ohos_apply_system_ui_font(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Speaker Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    s_s70_speaker_status_label = lv_label_create(scr);
    lv_obj_set_width(s_s70_speaker_status_label, 420);
    lv_label_set_text(s_s70_speaker_status_label,
                      "Speaker Test:\n"
                      "Press Play to test ES8311 + I2S TX + speaker.\n"
                      "Audio source: embedded canon.pcm");
    lv_obj_align(s_s70_speaker_status_label, LV_ALIGN_TOP_LEFT, 30, 90);

    lv_obj_t *play_btn = lv_button_create(scr);
    lv_obj_set_size(play_btn, 300, 70);
    lv_obj_align(play_btn, LV_ALIGN_TOP_MID, 0, 390);
    lv_obj_add_event_cb(play_btn, ohos_s70_speaker_play_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *play_label = lv_label_create(play_btn);
    lv_label_set_text(play_label, "Play Speaker");
    lv_obj_center(play_label);

    lv_obj_t *back_btn = lv_button_create(scr);
    lv_obj_set_size(back_btn, 300, 70);
    lv_obj_align(back_btn, LV_ALIGN_TOP_MID, 0, 520);
    lv_obj_add_event_cb(back_btn, ohos_s70_speaker_back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "S70B Speaker Test page created");
}


static void ohos_s68_speaker_test_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S70B Speaker Test clicked");
        ohos_s70_show_speaker_page();
    }
}




/* S71B: live Mic Test page */
static lv_obj_t *s_s71_mic_status_label = NULL;
static lv_obj_t *s_s71_mic_level_label = NULL;
static lv_obj_t *s_s71_mic_level_bar = NULL;
static lv_timer_t *s_s71_mic_refresh_timer = NULL;

static volatile uint32_t s_s71_mic_started = 0;
static volatile uint32_t s_s71_mic_last_ret = 0xffffffffU;
static uint32_t ohos_s71_peak_to_level_percent(uint32_t peak)
{
    /*
     * S73C:
     * S73B Mic Stats now stores voice_peak16 into snap.peak.
     * The valid display range is 0..32768, not the old int32 range.
     */
    if (peak >= 32768U) {
        return 100U;
    }

    uint32_t level = (peak * 100U + 16384U) / 32768U;
    if (level > 100U) {
        level = 100U;
    }
    return level;
}

static void ohos_s71_mic_update_live_view(void)
{
    if (s_s71_mic_status_label == NULL ||
        !lv_obj_is_valid(s_s71_mic_status_label)) {
        return;
    }

    OhosAudioEs7210RecordStatsSnapshot snap;
    OhosAudioEs7210GetRecordStatsSnapshot(&snap);

    uint32_t level = ohos_s71_peak_to_level_percent(snap.peak);
    if (snap.seq == 0U) {
        level = 0;
    }

    char status[384];
    snprintf(status, sizeof(status),
             "Mic Test Live\n"
             "Task: %s\n"
             "Seq: %u\n"
             "Bytes: %u  Samples: %u\n"
             "Voice Peak16: %u\n"
             "Voice Level: %u%%\n"
             "Nonzero: %u\n"
             "First: %ld, %ld, %ld, %ld",
             snap.task_started ? "RUNNING" : "STOPPED",
             (unsigned int)snap.seq,
             (unsigned int)snap.bytes_read,
             (unsigned int)snap.samples,
             (unsigned int)snap.peak,
             (unsigned int)level,
             (unsigned int)snap.nonzero,
             (long)snap.first0,
             (long)snap.first1,
             (long)snap.first2,
             (long)snap.first3);

    lv_label_set_text(s_s71_mic_status_label, status);

    if (s_s71_mic_level_label != NULL &&
        lv_obj_is_valid(s_s71_mic_level_label)) {
        char level_text[64];
        snprintf(level_text, sizeof(level_text), "Voice Level: %u%%", (unsigned int)level);
        lv_label_set_text(s_s71_mic_level_label, level_text);
    }

    if (s_s71_mic_level_bar != NULL &&
        lv_obj_is_valid(s_s71_mic_level_bar)) {
        lv_bar_set_value(s_s71_mic_level_bar, (int32_t)level, LV_ANIM_OFF);
    }
}

static void ohos_s71_mic_refresh_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_s71_mic_status_label == NULL ||
        !lv_obj_is_valid(s_s71_mic_status_label)) {
        ESP_LOGW(TAG, "S71B mic refresh target invalid, delete timer");
        if (s_s71_mic_refresh_timer != NULL) {
            lv_timer_delete(s_s71_mic_refresh_timer);
            s_s71_mic_refresh_timer = NULL;
        }
        return;
    }

    ohos_s71_mic_update_live_view();
}

static void ohos_s71_mic_start_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    OhosAudioEs7210RecordStatsSnapshot snap;
    OhosAudioEs7210GetRecordStatsSnapshot(&snap);

    if (snap.task_started || s_s71_mic_started) {
        ESP_LOGW(TAG, "S71B mic stats already started");
        s_s71_mic_started = 1;
        ohos_s71_mic_update_live_view();
        return;
    }

    ESP_LOGI(TAG, "S71B Mic Test start clicked");

    uint32_t ret = OhosAudioServiceRecordStatsTest();
    s_s71_mic_last_ret = ret;

    if (ret == 0) {
        s_s71_mic_started = 1;
    }

    ESP_LOGI(TAG, "S71B Mic Test start ret=%u", (unsigned int)ret);

    ohos_s71_mic_update_live_view();
}




static volatile int s_s71_mic_back_pending = 0;

static void ohos_s71_mic_back_async_cb(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "S71C Mic Test async return to Demo Center");

    s_s71_mic_status_label = NULL;
    s_s71_mic_level_label = NULL;
    s_s71_mic_level_bar = NULL;
    s_s71_mic_back_pending = 0;

    ohos_s60_show_gif_page();
}

static void ohos_s71_mic_back_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code != LV_EVENT_PRESSED &&
        code != LV_EVENT_RELEASED &&
        code != LV_EVENT_CLICKED) {
        return;
    }

    if (s_s71_mic_back_pending) {
        return;
    }

    s_s71_mic_back_pending = 1;

    ESP_LOGI(TAG, "S71C Mic Test back event code=%d", (int)code);

    ESP_LOGI(TAG, "S71E Mic Test stop record stats before Back");
    (void)OhosAudioServiceStopMicStats();

    s_s71_mic_started = 0;
    s_s71_mic_last_ret = 0xffffffffU;
    ESP_LOGI(TAG, "S73D Mic Test UI state reset before Back");

    if (s_s71_mic_refresh_timer != NULL) {
        lv_timer_delete(s_s71_mic_refresh_timer);
        s_s71_mic_refresh_timer = NULL;
        ESP_LOGI(TAG, "S71C Mic Test refresh timer deleted");
    }

    lv_async_call(ohos_s71_mic_back_async_cb, NULL);
}






static void ohos_s71_show_mic_page(void)
{
    s_s71_mic_started = 0;
    s_s71_mic_last_ret = 0xffffffffU;
    ESP_LOGI(TAG, "S73D Mic Test UI state reset on page enter");

    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    ohos_apply_system_ui_font(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Mic Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    s_s71_mic_status_label = lv_label_create(scr);
    lv_obj_set_width(s_s71_mic_status_label, 430);
    lv_label_set_text(s_s71_mic_status_label,
                      "Mic Test Live\n"
                      "Press Start Mic Stats.\n"
                      "Then speak near the microphone.\n"
                      "The peak and level should change.");
    lv_obj_align(s_s71_mic_status_label, LV_ALIGN_TOP_LEFT, 25, 65);

    s_s71_mic_level_label = lv_label_create(scr);
    lv_label_set_text(s_s71_mic_level_label, "Input Level: 0%");
    lv_obj_align(s_s71_mic_level_label, LV_ALIGN_TOP_MID, 0, 345);

    s_s71_mic_level_bar = lv_bar_create(scr);
    lv_obj_set_size(s_s71_mic_level_bar, 380, 32);
    lv_obj_align(s_s71_mic_level_bar, LV_ALIGN_TOP_MID, 0, 385);
    lv_bar_set_range(s_s71_mic_level_bar, 0, 100);
    lv_bar_set_value(s_s71_mic_level_bar, 0, LV_ANIM_OFF);

    lv_obj_t *start_btn = lv_button_create(scr);
    lv_obj_set_size(start_btn, 300, 65);
    lv_obj_align(start_btn, LV_ALIGN_TOP_MID, 0, 455);
    lv_obj_add_event_cb(start_btn, ohos_s71_mic_start_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *start_label = lv_label_create(start_btn);
    lv_label_set_text(start_label, "Start Mic Stats");
    lv_obj_center(start_label);

    lv_obj_t *back_btn = lv_button_create(scr);
    lv_obj_set_size(back_btn, 300, 65);
    lv_obj_align(back_btn, LV_ALIGN_TOP_MID, 0, 555);
    lv_obj_add_event_cb(back_btn, ohos_s71_mic_back_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);

    if (s_s71_mic_refresh_timer != NULL) {
        lv_timer_delete(s_s71_mic_refresh_timer);
        s_s71_mic_refresh_timer = NULL;
    }

    s_s71_mic_refresh_timer = lv_timer_create(ohos_s71_mic_refresh_timer_cb, 300, NULL);

    ohos_s71_mic_update_live_view();

    ESP_LOGI(TAG, "S71B Mic Test live page created");
}


static void ohos_s68_mic_test_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S71B Mic Test clicked");
        ohos_s71_show_mic_page();
    }
}



/* S72A: real Voice Loopback page */
static lv_obj_t *s_s72_loopback_status_label = NULL;
static volatile uint32_t s_s72_loopback_task_running = 0;
static volatile uint32_t s_s72_loopback_last_ret = 0xffffffffU;

static void ohos_s72_loopback_set_status(const char *text)
{
    if (s_s72_loopback_status_label != NULL) {
        lv_label_set_text(s_s72_loopback_status_label, text);
    }
}

static void ohos_s72_loopback_start_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "S72A voice loopback task start");

    /*
     * Keep audio path mutually exclusive before starting loopback.
     * These are no-op if the corresponding test is not running.
     */
    (void)OhosAudioServiceStopMicStats();
    (void)OhosAudioServiceStopSpeaker();

    uint32_t ret = OhosAudioServiceLoopbackTest();

    s_s72_loopback_last_ret = ret;
    s_s72_loopback_task_running = 0;

    ESP_LOGI(TAG, "S72A voice loopback start ret=%u", (unsigned int)ret);

    if (ret == 0) {
        ohos_s72_loopback_set_status(
            "Voice Loopback:\n"
            "Started.\n"
            "Speak near the microphone.\n"
            "The board will record then play back repeatedly.\n"
            "Press Back to stop.");
    } else {
        ohos_s72_loopback_set_status(
            "Voice Loopback:\n"
            "Start failed.\n"
            "Check serial log for ES7210 / ES8311 / I2S errors.");
    }

    return;
}

static void ohos_s72_loopback_start_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ESP_LOGI(TAG, "S72A Voice Loopback start clicked");

    if (s_s72_loopback_task_running) {
        ESP_LOGW(TAG, "S72A Voice Loopback already starting/running");
        ohos_s72_loopback_set_status(
            "Voice Loopback:\n"
            "Already starting/running.\n"
            "Press Back to stop.");
        return;
    }

    s_s72_loopback_task_running = 1;
    s_s72_loopback_last_ret = 0xffffffffU;

    ohos_s72_loopback_set_status(
        "Voice Loopback:\n"
        "Starting ES7210 + ES8311 half-duplex loopback...\n"
        "Please wait.");

    UINT32 ret = OhosLiteosCreateTask("s72_loopback_start",
                                      ohos_s72_loopback_start_task,
                                      NULL,
                                      OHOS_UI_WORKER_TASK_PRIO,
                                      OHOS_UI_WORKER_TASK_STACK,
                                      NULL);

    ESP_LOGI(TAG, "S72A voice loopback start LiteOS task create ret=%u", (unsigned)ret);

    if (ret != LOS_OK) {
        s_s72_loopback_task_running = 0;
        s_s72_loopback_last_ret = 1;
        ohos_s72_loopback_set_status(
            "Voice Loopback:\n"
            "Create start task failed.");
    }
}

static void ohos_s72_loopback_back_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ESP_LOGI(TAG, "S72A Voice Loopback back clicked");

    ESP_LOGI(TAG, "S72A Voice Loopback stop before Back");
    (void)OhosAudioServiceStopLoopback();

    s_s72_loopback_task_running = 0;
    s_s72_loopback_status_label = NULL;

    ohos_s60_show_gif_page();
}

static void ohos_s72_show_loopback_page(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    ohos_apply_system_ui_font(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Voice Loopback");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

    s_s72_loopback_status_label = lv_label_create(scr);
    lv_obj_set_width(s_s72_loopback_status_label, 420);
    lv_label_set_text(s_s72_loopback_status_label,
                      "Voice Loopback:\n"
                      "Half-duplex test:\n"
                      "1. Record microphone for about 1.2s.\n"
                      "2. Play recorded voice through speaker.\n"
                      "Press Start to begin.");
    lv_obj_align(s_s72_loopback_status_label, LV_ALIGN_TOP_LEFT, 30, 90);

    lv_obj_t *start_btn = lv_button_create(scr);
    lv_obj_set_size(start_btn, 300, 70);
    lv_obj_align(start_btn, LV_ALIGN_TOP_MID, 0, 390);
    lv_obj_add_event_cb(start_btn, ohos_s72_loopback_start_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *start_label = lv_label_create(start_btn);
    lv_label_set_text(start_label, "Start Loopback");
    lv_obj_center(start_label);

    lv_obj_t *back_btn = lv_button_create(scr);
    lv_obj_set_size(back_btn, 300, 70);
    lv_obj_align(back_btn, LV_ALIGN_TOP_MID, 0, 520);
    lv_obj_add_event_cb(back_btn, ohos_s72_loopback_back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "S72A Voice Loopback page created");
}

static void ohos_s68_loopback_test_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S72A Voice Loopback clicked");
        ohos_s72_show_loopback_page();
    }
}



/* S69C: real WS63 UART Test page */
static lv_obj_t *s_s69_ws63_info_label = NULL;
static lv_obj_t *s_s69_ws63_action_label = NULL;
static lv_obj_t *s_s69_ws63_text_label = NULL;
static char s_s69_ws63_info_buf[1280];
static char s_s69_ws63_down_text_buf[384];
static char s_s69_ws63_action_buf[128];
static char s_s69_ws63_tts_buf[448];
static volatile uint32_t s_s69_ws63_up_opus_action_busy = 0;
static volatile uint32_t s_s69_ws63_up_opus_last_ok = 0;
static volatile uint32_t s_s69_ws63_up_opus_last_start = 0;

static const char *ohos_s69_agent_status_name(uint32_t status)
{
    switch (status) {
        case 0U:
            return "Idle";
        case 1U:
            return "Wakeup";
        case 2U:
            return "Listening";
        case 3U:
            return "Speaking";
        default:
            return "Unknown";
    }
}

static const char *ohos_s69_binary_status_name(uint32_t value)
{
    if (value == 0xFFFFFFFFU) {
        return "Unknown";
    }
    return value ? "ON" : "OFF";
}

static void ohos_s69_ws63_update_labels(const char *action, uint32_t action_ok)
{
    OhosUartLinkUiSnapshot snap;
    OhosUartLinkUiGetSnapshot(&snap);

    char *info = s_s69_ws63_info_buf;
    char *down_text = s_s69_ws63_down_text_buf;
    const char *last_name = OhosUartLinkUiCmdName(snap.last_rx_cmd);

    /*
     * S69D:
     * On this ESP32-P4 toolchain, uint32_t may be typedef'ed as unsigned long.
     * Cast to unsigned int before using %u/%x to satisfy -Werror=format.
     */
    const unsigned int tx_count = (unsigned int)snap.tx_count;
    const unsigned int query_tx_ok = (unsigned int)snap.query_tx_ok;
    const unsigned int query_tx_count = (unsigned int)snap.query_tx_count;
    const unsigned int rx_frame_count = (unsigned int)snap.rx_frame_count;
    const unsigned int rx_raw_count = (unsigned int)snap.rx_raw_count;
    const unsigned int rx_raw_bytes = (unsigned int)snap.rx_raw_bytes;
    const unsigned int rx_parse_fail_count = (unsigned int)snap.rx_parse_fail_count;
    const unsigned int last_rx_cmd = (unsigned int)snap.last_rx_cmd;
    const unsigned int last_system_status = (unsigned int)snap.last_system_status;
    const unsigned int agent_status = (unsigned int)snap.agent_status;
    const unsigned int agent_status_rx_count = (unsigned int)snap.agent_status_rx_count;
    const unsigned int agent_status_invalid_count = (unsigned int)snap.agent_status_invalid_count;
    const unsigned int wake_status_rx_count = (unsigned int)snap.wake_status_rx_count;
    const unsigned int wake_status_invalid_count = (unsigned int)snap.wake_status_invalid_count;
    const unsigned int wake_tx_ok = (unsigned int)snap.wake_tx_ok;
    const unsigned int wake_tx_count = (unsigned int)snap.wake_tx_count;
    const unsigned int wake_pending = (unsigned int)snap.wake_pending;
    const unsigned int wake_pending_seq = (unsigned int)snap.wake_pending_seq;
    const unsigned int wake_pending_retries = (unsigned int)snap.wake_pending_retries;
    const unsigned int wake_pending_ack_count = (unsigned int)snap.wake_pending_ack_count;
    const unsigned int wake_pending_timeout_count = (unsigned int)snap.wake_pending_timeout_count;
    const unsigned int agent_query_tx_ok = (unsigned int)snap.agent_query_tx_ok;
    const unsigned int agent_query_tx_count = (unsigned int)snap.agent_query_tx_count;
    const unsigned int interrupt_mode_rx_count = (unsigned int)snap.interrupt_mode_rx_count;
    const unsigned int interrupt_mode_invalid_count = (unsigned int)snap.interrupt_mode_invalid_count;
    const unsigned int interrupt_mode_tx_ok = (unsigned int)snap.interrupt_mode_tx_ok;
    const unsigned int interrupt_mode_tx_count = (unsigned int)snap.interrupt_mode_tx_count;
    const unsigned int voice_user_action_count = (unsigned int)snap.voice_user_action_count;
    const unsigned int voice_user_action_wake_count = (unsigned int)snap.voice_user_action_wake_count;
    const unsigned int voice_user_action_query_count = (unsigned int)snap.voice_user_action_query_count;
    const unsigned int voice_user_action_noop_count = (unsigned int)snap.voice_user_action_noop_count;
    const unsigned int voice_user_action_disabled_count = (unsigned int)snap.voice_user_action_disabled_count;
    const unsigned int user_interrupt_active = (unsigned int)snap.user_interrupt_active;
    const unsigned int user_interrupt_seq = (unsigned int)snap.user_interrupt_seq;
    const unsigned int downlink_suppress_seq = (unsigned int)snap.downlink_suppress_seq;
    const unsigned int downlink_suppress_drop = (unsigned int)snap.downlink_suppress_drop;
    const unsigned int ws_url_len = (unsigned int)snap.ws_url_len;
    const unsigned int wifi_ssid_len = (unsigned int)snap.wifi_ssid_len;
    const unsigned int down_text_count = (unsigned int)snap.down_text_count;
    const unsigned int down_text_type = (unsigned int)snap.down_text_type;
    const unsigned int down_text_len = (unsigned int)snap.down_text_len;
    const unsigned int down_audio_count = (unsigned int)snap.down_audio_count;
    const unsigned int down_audio_bytes = (unsigned int)snap.down_audio_bytes;
    const unsigned int down_audio_last_len = (unsigned int)snap.down_audio_last_len;
    const unsigned int down_audio_start_count = (unsigned int)snap.down_audio_start_count;
    const unsigned int down_audio_stop_count = (unsigned int)snap.down_audio_stop_count;
    const unsigned int down_audio_stop_seen = (unsigned int)snap.down_audio_stop_seen;
    const unsigned int down_audio_stream_open = (unsigned int)snap.down_audio_stream_open;
    const unsigned int down_audio_prepared_on_start = (unsigned int)snap.down_audio_prepared_on_start;
    const unsigned int down_audio_wait_more_count = (unsigned int)snap.down_audio_wait_more_count;
    const unsigned int down_audio_wait_more_timeout = (unsigned int)snap.down_audio_wait_more_timeout;
    const unsigned int down_audio_stream_underrun = (unsigned int)snap.down_audio_stream_underrun;
    const unsigned int down_audio_stream_min_level = (unsigned int)snap.down_audio_stream_min_level;
    const unsigned int downlink_session_seq = (unsigned int)snap.downlink_session_seq;
    const unsigned int downlink_session_prepared_seq = (unsigned int)snap.downlink_session_prepared_seq;
    const unsigned int up_pcm_ok = (unsigned int)snap.up_opus_pcm_ok;
    const unsigned int up_pcm_fail = (unsigned int)snap.up_opus_pcm_fail;
    const unsigned int up_enc_ok = (unsigned int)snap.up_opus_encode_ok;
    const unsigned int up_enc_fail = (unsigned int)snap.up_opus_encode_fail;
    const unsigned int up_tx_ok = (unsigned int)snap.up_opus_tx_ok;
    const unsigned int up_tx_fail = (unsigned int)snap.up_opus_tx_fail;
    const unsigned int up_last_len = (unsigned int)snap.up_opus_last_len;
    const unsigned int up_last_peak = (unsigned int)snap.up_opus_last_peak;
    const unsigned int up_last_nonzero = (unsigned int)snap.up_opus_last_nonzero;
    const unsigned int up_vad_start_ok = (unsigned int)snap.up_opus_vad_start_ok;
    const unsigned int up_vad_start_fail = (unsigned int)snap.up_opus_vad_start_fail;
    const unsigned int up_vad_end_ok = (unsigned int)snap.up_opus_vad_end_ok;
    const unsigned int up_vad_end_fail = (unsigned int)snap.up_opus_vad_end_fail;
    const char *agent_name = ohos_s69_agent_status_name(snap.agent_status);
    const char *wake_name = ohos_s69_binary_status_name(snap.wake_status);
    const char *interrupt_name = snap.interrupt_mode ? "ON" : "OFF";

    OhosUartLinkUiGetDownText(down_text, sizeof(down_text));

    if (snap.last_system_status == 0xFFFFFFFFU) {
        snprintf(info, sizeof(s_s69_ws63_info_buf),
                 "Driver: %s\n"
                 "RX Task: %s\n"
                 "Handshake: %s\n"
                 "Link: %s\n"
                 "TX Count: %u\n"
                 "Query: %u/%u\n"
                 "RX Frames: %u\n"
                 "RX Raw: %u packets / %u bytes\n"
                  "Parse Fail: %u\n"
                  "Last RX Cmd: 0x%04x %s\n"
                  "Status: unknown\n"
                  "Agent: %s (%u) rx=%u invalid=%u query=%u/%u\n"
                  "Wake: %s rx=%u invalid=%u tx=%u/%u pending=%u seq=%u retry=%u ack=%u timeout=%u\n"
                  "Interrupt: %s rx=%u invalid=%u tx=%u/%u\n"
                  "Voice Action: total=%u wake=%u query=%u noop=%u disabled=%u int=%u/%u sup=%u drop=%u\n"
                   "SSID Len: %u URL Len: %u\n"
                   "TTS: #%u type=%u len=%u\n"
                  "Opus Down: %u frames/%uB last=%u b=%u/%u open=%u stop=%u pre=%u wait=%u/%u ur=%u min=%u seq=%u/%u\n"
                  "Opus Up: %s task=%u vadS=%u/%u vadE=%u/%u pcm=%u/%u enc=%u/%u tx=%u/%u len=%u peak=%u nz=%u",
                  snap.driver_init_ok ? "OK" : "NO",
                  snap.rx_task_started ? "ON" : "NO",
                 snap.handshake_done ? "DONE" : "RUN/NO",
                 snap.link_ready ? "READY" : "NOT READY",
                 tx_count,
                 query_tx_ok,
                 query_tx_count,
                 rx_frame_count,
                 rx_raw_count,
                 rx_raw_bytes,
                 rx_parse_fail_count,
                  last_rx_cmd,
                  last_name ? last_name : "UNKNOWN",
                  agent_name,
                  agent_status,
                  agent_status_rx_count,
                  agent_status_invalid_count,
                  agent_query_tx_ok,
                  agent_query_tx_count,
                  wake_name,
                  wake_status_rx_count,
                  wake_status_invalid_count,
                  wake_tx_ok,
                  wake_tx_count,
                  wake_pending,
                  wake_pending_seq,
                  wake_pending_retries,
                  wake_pending_ack_count,
                  wake_pending_timeout_count,
                  interrupt_name,
                  interrupt_mode_rx_count,
                  interrupt_mode_invalid_count,
                  interrupt_mode_tx_ok,
                  interrupt_mode_tx_count,
                  voice_user_action_count,
                  voice_user_action_wake_count,
                  voice_user_action_query_count,
                  voice_user_action_noop_count,
                  voice_user_action_disabled_count,
                  user_interrupt_active,
                  user_interrupt_seq,
                  downlink_suppress_seq,
                  downlink_suppress_drop,
                  wifi_ssid_len,
                  ws_url_len,
                 down_text_count,
                 down_text_type,
                  down_text_len,
                  down_audio_count,
                  down_audio_bytes,
                  down_audio_last_len,
                  down_audio_start_count,
                  down_audio_stop_count,
                  down_audio_stream_open,
                  down_audio_stop_seen,
                  down_audio_prepared_on_start,
                  down_audio_wait_more_count,
                  down_audio_wait_more_timeout,
                  down_audio_stream_underrun,
                  down_audio_stream_min_level,
                  downlink_session_seq,
                  downlink_session_prepared_seq,
                  snap.up_opus_running ? "RUN" : "STOP",
                 (unsigned int)snap.up_opus_task_started,
                 up_vad_start_ok,
                 up_vad_start_fail,
                 up_vad_end_ok,
                 up_vad_end_fail,
                 up_pcm_ok,
                 up_pcm_fail,
                 up_enc_ok,
                 up_enc_fail,
                 up_tx_ok,
                  up_tx_fail,
                  up_last_len,
                  up_last_peak,
                  up_last_nonzero);
    } else {
        snprintf(info, sizeof(s_s69_ws63_info_buf),
                 "Driver: %s\n"
                 "RX Task: %s\n"
                 "Handshake: %s\n"
                 "Link: %s\n"
                 "TX Count: %u\n"
                 "Query: %u/%u\n"
                 "RX Frames: %u\n"
                 "RX Raw: %u packets / %u bytes\n"
                  "Parse Fail: %u\n"
                  "Last RX Cmd: 0x%04x %s\n"
                  "Status: %u\n"
                  "Agent: %s (%u) rx=%u invalid=%u query=%u/%u\n"
                  "Wake: %s rx=%u invalid=%u tx=%u/%u pending=%u seq=%u retry=%u ack=%u timeout=%u\n"
                  "Interrupt: %s rx=%u invalid=%u tx=%u/%u\n"
                  "Voice Action: total=%u wake=%u query=%u noop=%u disabled=%u int=%u/%u sup=%u drop=%u\n"
                   "SSID Len: %u URL Len: %u\n"
                   "TTS: #%u type=%u len=%u\n"
                  "Opus Down: %u frames/%uB last=%u b=%u/%u open=%u stop=%u pre=%u wait=%u/%u ur=%u min=%u seq=%u/%u\n"
                  "Opus Up: %s task=%u vadS=%u/%u vadE=%u/%u pcm=%u/%u enc=%u/%u tx=%u/%u len=%u peak=%u nz=%u",
                  snap.driver_init_ok ? "OK" : "NO",
                  snap.rx_task_started ? "ON" : "NO",
                 snap.handshake_done ? "DONE" : "RUN/NO",
                 snap.link_ready ? "READY" : "NOT READY",
                 tx_count,
                 query_tx_ok,
                 query_tx_count,
                 rx_frame_count,
                 rx_raw_count,
                 rx_raw_bytes,
                 rx_parse_fail_count,
                  last_rx_cmd,
                  last_name ? last_name : "UNKNOWN",
                  last_system_status,
                  agent_name,
                  agent_status,
                  agent_status_rx_count,
                  agent_status_invalid_count,
                  agent_query_tx_ok,
                  agent_query_tx_count,
                  wake_name,
                  wake_status_rx_count,
                  wake_status_invalid_count,
                  wake_tx_ok,
                  wake_tx_count,
                  wake_pending,
                  wake_pending_seq,
                  wake_pending_retries,
                  wake_pending_ack_count,
                  wake_pending_timeout_count,
                  interrupt_name,
                  interrupt_mode_rx_count,
                  interrupt_mode_invalid_count,
                  interrupt_mode_tx_ok,
                  interrupt_mode_tx_count,
                  voice_user_action_count,
                  voice_user_action_wake_count,
                  voice_user_action_query_count,
                  voice_user_action_noop_count,
                  voice_user_action_disabled_count,
                  user_interrupt_active,
                  user_interrupt_seq,
                  downlink_suppress_seq,
                  downlink_suppress_drop,
                  wifi_ssid_len,
                  ws_url_len,
                 down_text_count,
                 down_text_type,
                  down_text_len,
                  down_audio_count,
                  down_audio_bytes,
                  down_audio_last_len,
                  down_audio_start_count,
                  down_audio_stop_count,
                  down_audio_stream_open,
                  down_audio_stop_seen,
                  down_audio_prepared_on_start,
                  down_audio_wait_more_count,
                  down_audio_wait_more_timeout,
                  down_audio_stream_underrun,
                  down_audio_stream_min_level,
                  downlink_session_seq,
                  downlink_session_prepared_seq,
                  snap.up_opus_running ? "RUN" : "STOP",
                 (unsigned int)snap.up_opus_task_started,
                 up_vad_start_ok,
                 up_vad_start_fail,
                 up_vad_end_ok,
                 up_vad_end_fail,
                 up_pcm_ok,
                 up_pcm_fail,
                 up_enc_ok,
                 up_enc_fail,
                 up_tx_ok,
                  up_tx_fail,
                  up_last_len,
                  up_last_peak,
                  up_last_nonzero);
    }

    if (s_s69_ws63_info_label != NULL && lv_obj_is_valid(s_s69_ws63_info_label)) {
        lv_label_set_text(s_s69_ws63_info_label, info);
    }

    if (s_s69_ws63_action_label != NULL && lv_obj_is_valid(s_s69_ws63_action_label)) {
        char *act = s_s69_ws63_action_buf;
        if (action != NULL) {
            if (action_ok && snap.wake_pending && strcmp(action, "Wake Pending") == 0) {
                snprintf(act, sizeof(s_s69_ws63_action_buf),
                         "Last Action: \xE5\x94\xA4\xE9\x86\x92\xE4\xB8\xAD");
            } else {
                snprintf(act, sizeof(s_s69_ws63_action_buf), "Last Action: %s %s",
                         action,
                         action_ok ? "OK" : "FAIL");
            }
        } else {
            snprintf(act, sizeof(s_s69_ws63_action_buf), "Last Action: Refresh");
        }
        lv_label_set_text(s_s69_ws63_action_label, act);
    }

    if (s_s69_ws63_text_label != NULL && lv_obj_is_valid(s_s69_ws63_text_label)) {
        char *text = s_s69_ws63_tts_buf;
        if (down_text[0] != '\0') {
            snprintf(text, sizeof(s_s69_ws63_tts_buf), "TTS Text:\n%s", down_text);
        } else {
            snprintf(text, sizeof(s_s69_ws63_tts_buf), "TTS Text:\n<empty>");
        }
        lv_label_set_text(s_s69_ws63_text_label, text);
    }

    ESP_LOGI(TAG,
             "S69D WS63 UI refresh action=%s ok=%u link=%u tx=%u rxBytes=%u lastCmd=0x%04x status=%u",
             action ? action : "refresh",
             (unsigned int)action_ok,
             (unsigned int)snap.link_ready,
             tx_count,
             rx_raw_bytes,
             last_rx_cmd,
             last_system_status);
}


static void ohos_s69_ws63_back_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S69C WS63 UART back clicked");
        s_s69_ws63_info_label = NULL;
        s_s69_ws63_action_label = NULL;
        s_s69_ws63_text_label = NULL;
        ohos_s60_show_gif_page();
    }
}

static void ohos_s69_ws63_agent_query_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        uint32_t ok = OhosUartLinkUiQueryAgentStatus();
        ohos_s69_ws63_update_labels("Agent Query", ok);
    }
}

static void ohos_s69_ws63_wake_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        uint32_t ok = OhosUartLinkUiVoiceUserAction();
        ohos_s69_ws63_update_labels(ok ? "Voice Action" : "Voice Action Fail", ok);
    }
}

static void ohos_s69_ws63_interrupt_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        OhosUartLinkUiSnapshot snap;
        OhosUartLinkUiGetSnapshot(&snap);
        uint32_t next = snap.interrupt_mode ? 0U : 1U;
        uint32_t ok = OhosUartLinkUiSetInterruptMode(next);
        ohos_s69_ws63_update_labels(next ? "Interrupt ON" : "Interrupt OFF", ok);
    }
}

static void ohos_s69_ws63_alarm_on_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        uint32_t ok = OhosUartLinkUiSendModuleControl(OHOS_UART_MODULE_DEVICE_ALARM,
                                                      OHOS_UART_MODULE_ACTION_ON);
        ohos_s69_ws63_update_labels(ok ? "Alarm ON" : "Alarm ON Fail", ok);
    }
}

static void ohos_s69_ws63_alarm_off_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        uint32_t ok = OhosUartLinkUiSendModuleControl(OHOS_UART_MODULE_DEVICE_ALARM,
                                                      OHOS_UART_MODULE_ACTION_OFF);
        ohos_s69_ws63_update_labels(ok ? "Alarm OFF" : "Alarm OFF Fail", ok);
    }
}

static void ohos_s69_ws63_up_opus_done_async_cb(void *arg)
{
    (void)arg;

    const char *action = s_s69_ws63_up_opus_last_start ? "Start Up Opus" : "Stop Up Opus";
    uint32_t ok = s_s69_ws63_up_opus_last_ok;

    ESP_LOGI(TAG,
             "S69C Up Opus worker done action=%s ok=%u",
             action,
             (unsigned)ok);

    ohos_s69_ws63_update_labels(action, ok);
}

static void ohos_s69_ws63_up_opus_task(void *arg)
{
    (void)arg;

    uint32_t start = s_s69_ws63_up_opus_last_start;
    uint32_t ok;

    if (start) {
        ok = OhosUartLinkUiStartUplinkOpusTest();
    } else {
        ok = OhosUartLinkUiStopUplinkOpusTest();
    }

    s_s69_ws63_up_opus_last_ok = ok;
    s_s69_ws63_up_opus_action_busy = 0U;
    lv_async_call(ohos_s69_ws63_up_opus_done_async_cb, NULL);

    ESP_LOGI(TAG,
             "S69C Up Opus worker exit start=%u ok=%u",
             (unsigned)start,
             (unsigned)ok);

    return;
}

static void ohos_s69_ws63_stop_up_opus_direct(const char *reason)
{
    uint32_t ok;

    s_s69_ws63_up_opus_last_start = 0U;
    ok = OhosUartLinkUiStopUplinkOpusTest();
    s_s69_ws63_up_opus_last_ok = ok;
    s_s69_ws63_up_opus_action_busy = 0U;

    ESP_LOGI(TAG,
             "S69C Up Opus direct stop reason=%s ok=%u",
             (reason != NULL) ? reason : "unknown",
             (unsigned)ok);

    ohos_s69_ws63_update_labels("Stop Up Opus", ok);
}

static void ohos_s69_ws63_up_opus_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        OhosUartLinkUiSnapshot snap;

        OhosUartLinkUiGetSnapshot(&snap);

        if (s_s69_ws63_up_opus_action_busy) {
            if (snap.up_opus_running) {
                ohos_s69_ws63_stop_up_opus_direct("busy_running_click");
                return;
            }
            ESP_LOGW(TAG, "S69C Up Opus action already pending");
            if (s_s69_ws63_action_label != NULL &&
                lv_obj_is_valid(s_s69_ws63_action_label)) {
                lv_label_set_text(s_s69_ws63_action_label, "Last Action: Up Opus pending");
            }
            return;
        }

        if (snap.up_opus_running) {
            ohos_s69_ws63_stop_up_opus_direct("running_click");
            return;
        }

        s_s69_ws63_up_opus_last_start = snap.up_opus_running ? 0U : 1U;
        s_s69_ws63_up_opus_action_busy = 1U;

        if (s_s69_ws63_action_label != NULL &&
            lv_obj_is_valid(s_s69_ws63_action_label)) {
            lv_label_set_text(s_s69_ws63_action_label,
                              s_s69_ws63_up_opus_last_start ?
                              "Last Action: Start Up Opus pending" :
                              "Last Action: Stop Up Opus pending");
        }

        UINT32 ret = OhosLiteosCreateTask("s69_up_opus",
                                          ohos_s69_ws63_up_opus_task,
                                          NULL,
                                          OHOS_UI_WORKER_TASK_PRIO,
                                          OHOS_UI_WORKER_TASK_STACK,
                                          NULL);
        ESP_LOGI(TAG,
                 "S69C Up Opus worker create ret=%u start=%u",
                 (unsigned)ret,
                 (unsigned)s_s69_ws63_up_opus_last_start);
        if (ret != LOS_OK) {
            s_s69_ws63_up_opus_action_busy = 0U;
            s_s69_ws63_up_opus_last_ok = 0U;
            ohos_s69_ws63_update_labels("Up Opus Task", 0);
        }
    }
}

static void ohos_s69_ws63_refresh_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ohos_s69_ws63_update_labels(NULL, 1);
    }
}

static lv_obj_t *ohos_s69_create_button(lv_obj_t *parent,
                                        const char *text,
                                        int x,
                                        int y,
                                        lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 205, 50);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, x, y);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);

    return btn;
}

static void ohos_s69_apply_external_font(lv_obj_t *label)
{
    const lv_font_t *font;

    if (label == NULL || !lv_obj_is_valid(label)) {
        return;
    }

    if (!OhosLvglExternalFontIsLoaded()) {
        ESP_LOGW(TAG, "P4 external dialog font not loaded yet, keep default font");
        return;
    }

    font = OhosLvglExternalFontGet();
    if (font == NULL) {
        return;
    }

    if (font->line_height < 14) {
        ESP_LOGW(TAG,
                 "P4 external dialog font loaded but too small line_height=%d, keep default font",
                 (int)font->line_height);
        return;
    }

    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    ESP_LOGI(TAG,
             "P4 external dialog font applied to WS63 text label line_height=%d base_line=%d",
             (int)font->line_height,
             (int)font->base_line);
}

static void ohos_s69_show_ws63_uart_page(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_clean(scr);
    ohos_apply_system_ui_font(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "WS63 UART Test");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    s_s69_ws63_info_label = lv_label_create(scr);
    lv_obj_set_width(s_s69_ws63_info_label, 430);
    lv_obj_set_height(s_s69_ws63_info_label, 360);
    lv_obj_set_style_text_font(s_s69_ws63_info_label, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_label_set_long_mode(s_s69_ws63_info_label, LV_LABEL_LONG_DOT);
    lv_obj_align(s_s69_ws63_info_label, LV_ALIGN_TOP_LEFT, 25, 60);

    s_s69_ws63_action_label = lv_label_create(scr);
    lv_obj_set_width(s_s69_ws63_action_label, 430);
    lv_obj_set_height(s_s69_ws63_action_label, 24);
    lv_label_set_long_mode(s_s69_ws63_action_label, LV_LABEL_LONG_DOT);
    lv_obj_align(s_s69_ws63_action_label, LV_ALIGN_TOP_LEFT, 25, 430);

    s_s69_ws63_text_label = lv_label_create(scr);
    lv_obj_set_width(s_s69_ws63_text_label, 430);
    lv_obj_set_height(s_s69_ws63_text_label, 70);
    lv_label_set_long_mode(s_s69_ws63_text_label, LV_LABEL_LONG_DOT);
    lv_obj_align(s_s69_ws63_text_label, LV_ALIGN_TOP_LEFT, 25, 460);
    ohos_s69_apply_external_font(s_s69_ws63_text_label);

    ohos_s69_create_button(scr, "Agent Query", -110, 550, ohos_s69_ws63_agent_query_event_cb);
    ohos_s69_create_button(scr, "Voice Action", 110, 550, ohos_s69_ws63_wake_event_cb);
    ohos_s69_create_button(scr, "Alarm ON", -110, 610, ohos_s69_ws63_alarm_on_event_cb);
    ohos_s69_create_button(scr, "Alarm OFF", 110, 610, ohos_s69_ws63_alarm_off_event_cb);
    ohos_s69_create_button(scr, "Refresh", -110, 670, ohos_s69_ws63_refresh_event_cb);
    ohos_s69_create_button(scr, "Back", 110, 670, ohos_s69_ws63_back_event_cb);

    ohos_s69_ws63_update_labels(NULL, 1);

    ESP_LOGI(TAG, "S69C WS63 UART Test page created");
}


static void ohos_s68_ws63_uart_test_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S69C WS63 UART Test clicked");
        ohos_s69_show_ws63_uart_page();
    }
}

#if OHOS_ENABLE_P4_DIALOG_UI
static void ohos_s68_dialog_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S75A Dialog clicked");
        P4DialogPageSetBackHandler(ohos_s60_show_gif_page);
        P4DialogPageShow();
    }
}
#endif

static void ohos_s68_system_status_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S74A System Status clicked");

        OhosUartLinkUiSnapshot snap;
        OhosUartLinkUiGetSnapshot(&snap);

        const char *ws63_tx_status =
            (snap.query_tx_ok > 0U || snap.tx_count > 0U) ? "OK" :
            (snap.driver_init_ok ? "WAITING" : "NO");
        const char *ws63_rx_status =
            (snap.link_ready || snap.rx_frame_count > 0U || snap.rx_raw_bytes > 0U) ? "OK" :
            (snap.rx_task_started ? "WAITING" : "NO");
        const char *ws63_link_status = snap.link_ready ? "READY" : "WAITING";

        char detail[384];
        snprintf(detail,
                 sizeof(detail),
                 "Display: OK\n"
                 "Touch: OK\n"
                 "Camera: OK\n"
                 "Speaker: OK\n"
                 "Mic: OK\n"
                 "Voice Loopback: OK\n"
                 "WS63 UART TX: %s\n"
                 "WS63 UART RX: %s\n"
                 "WS63 Link: %s\n"
                 "WS63 RX Frames: %u\n"
                 "WS63 RX Bytes: %u",
                 ws63_tx_status,
                 ws63_rx_status,
                 ws63_link_status,
                 (unsigned int)snap.rx_frame_count,
                 (unsigned int)snap.rx_raw_bytes);

        ohos_s68_show_placeholder_page("System Status",
                                       detail);
    }
}

#if OHOS_ENABLE_P4_DESKTOP_UI
static void ohos_p4_desktop_placeholder(const char *title, const char *detail)
{
    ohos_s68_show_placeholder_page(title, detail);
}

static void ohos_p4_show_diagnostics_page(void)
{
    lv_obj_t *parent = lv_screen_active();
    if (parent == NULL) {
        return;
    }

    lv_obj_clean(parent);
    ohos_apply_system_ui_font(parent);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "诊断");
    lv_obj_set_style_text_font(title, P4SystemFontGet(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    typedef struct {
        const char *text;
        lv_event_cb_t cb;
    } ohos_diag_btn_t;

    static const ohos_diag_btn_t buttons[] = {
        {"WS63 UART Test", ohos_s68_ws63_uart_test_event_cb},
        {"System Status",  ohos_s68_system_status_event_cb},
        {"Speaker Test",   ohos_s68_speaker_test_event_cb},
        {"Mic Test",       ohos_s68_mic_test_event_cb},
        {"Voice Loopback", ohos_s68_loopback_test_event_cb},
    };

    const int btn_w = 340;
    const int btn_h = 56;
    const int start_y = 86;
    const int gap_y = 66;

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        lv_obj_t *btn = lv_button_create(parent);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, start_y + (int)i * gap_y);
        lv_obj_add_event_cb(btn, buttons[i].cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label = lv_label_create(btn);
        lv_obj_set_style_text_font(label, P4SystemFontGet(), LV_PART_MAIN);
        lv_label_set_text(label, buttons[i].text);
        lv_obj_center(label);
    }

    lv_obj_t *back = lv_button_create(parent);
    lv_obj_set_size(back, 260, 58);
    lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -32);
    lv_obj_add_event_cb(back, ohos_s68_demo_back_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(back);
    lv_obj_set_style_text_font(back_label, P4SystemFontGet(), LV_PART_MAIN);
    lv_label_set_text(back_label, "返回桌面");
    lv_obj_center(back_label);

    ESP_LOGI(TAG, "P4 desktop diagnostics page created");
}

static void ohos_p4_desktop_entry_handler(P4DesktopEntry entry)
{
    switch (entry) {
        case P4_DESKTOP_ENTRY_DIALOG:
        #if OHOS_ENABLE_P4_DIALOG_UI
            ESP_LOGI(TAG, "P4 desktop Dialog entry");
            P4DialogPageSetBackHandler(ohos_s60_show_gif_page);
            P4DialogPageShow();
        #else
            ohos_p4_desktop_placeholder("对话", "Dialog UI disabled.");
        #endif
            break;
        case P4_DESKTOP_ENTRY_SETTINGS:
            ohos_p4_desktop_placeholder("设置",
                                        "音量、亮度和打断模式入口已预留。\n"
                                        "后续通过 ConfigService 保存，不在 UI 回调直接写文件。");
            break;
        case P4_DESKTOP_ENTRY_WIFI:
            ohos_p4_desktop_placeholder("Wi-Fi",
                                        "Wi-Fi 配网入口已预留。\n"
                                        "正式动作需严格按协议发送 0x0D05。");
            break;
        case P4_DESKTOP_ENTRY_DEVICE_INFO:
            ohos_p4_desktop_placeholder("设备信息",
                                        "设备 ID、P4/WS63 版本和字库版本入口已预留。\n"
                                        "页面只读取 snapshot，不做耗时探测。");
            break;
        case P4_DESKTOP_ENTRY_DIAGNOSTICS:
            ohos_p4_show_diagnostics_page();
            break;
        case P4_DESKTOP_ENTRY_SENSOR:
        #if OHOS_ENABLE_P4_SENSOR_UI
            ESP_LOGI(TAG, "P4 desktop Sensor entry");
            P4SensorPageSetBackHandler(ohos_s60_show_gif_page);
            P4SensorPageShow();
        #else
            ohos_p4_desktop_placeholder("传感器数据",
                                        "传感器数据页面已关闭。\n"
                                        "协议解析仍可按回退方案保留。");
        #endif
            break;
        case P4_DESKTOP_ENTRY_OTA:
            ohos_p4_desktop_placeholder("OTA",
                                        "OTA 入口已预留。\n"
                                        "下载和升级需后续独立方案接入。");
            break;
        case P4_DESKTOP_ENTRY_CAMERA:
            ESP_LOGI(TAG, "P4 desktop Camera entry");
            lv_async_call(ohos_s60_request_camera_page_async_cb, NULL);
            break;
        default:
            ESP_LOGW(TAG, "P4 desktop unknown entry=%u", (unsigned)entry);
            break;
    }
}

static void ohos_p4_show_desktop_page(lv_display_t *display)
{
    (void)display;

    static const P4DesktopPageOps ops = {
        .on_entry = ohos_p4_desktop_entry_handler,
    };

    P4DesktopPageShow(&ops);
}
#endif


static void OHOS_MAYBE_UNUSED ohos_lvgl_create_touch_test_button(lv_display_t *display)
{
    lv_obj_t *parent = lv_display_get_screen_active(display);
    lv_obj_clean(parent);
    ohos_apply_system_ui_font(parent);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "ESP32-P4 OpenHarmony Demo Center");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    typedef struct {
        const char *text;
        lv_event_cb_t cb;
    } ohos_demo_btn_t;

    static const ohos_demo_btn_t buttons[] = {
        {"Camera Test",       ohos_touch_test_btn_event_cb},
        {"Speaker Test",      ohos_s68_speaker_test_event_cb},
        {"Mic Test",          ohos_s68_mic_test_event_cb},
        {"Voice Loopback",    ohos_s68_loopback_test_event_cb},
    #if OHOS_ENABLE_P4_DIALOG_UI
        {"Dialog",            ohos_s68_dialog_event_cb},
    #endif
        {"WS63 UART Test",    ohos_s68_ws63_uart_test_event_cb},
        {"System Status",     ohos_s68_system_status_event_cb},
    };

    const int btn_w = 340;
    const int btn_h = 56;
    const int start_y = 78;
    const int gap_y = 64;

    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        lv_obj_t *btn = lv_button_create(parent);
        lv_obj_set_size(btn, btn_w, btn_h);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, start_y + (int)i * gap_y);
        lv_obj_add_event_cb(btn, buttons[i].cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *label = lv_label_create(btn);
        lv_label_set_text(label, buttons[i].text);
        lv_obj_center(label);
    }

    ESP_LOGI(TAG, "S68A Demo Center UI created");
}



static void ohos_st7123_direct_poll_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "S57A direct ST7123 poll task start");

    uint32_t release_cnt = 0;
    uint32_t press_cnt = 0;
    bool was_pressed = false;

    while (1) {
        if (s_ohos_touch != NULL) {
            esp_err_t ret = esp_lcd_touch_read_data(s_ohos_touch);

            esp_lcd_touch_point_data_t points[5] = {0};
            uint8_t count = 0;
            esp_err_t get_ret = esp_lcd_touch_get_data(s_ohos_touch, points, &count, 5);

            if (ret == ESP_OK && get_ret == ESP_OK && count > 0) {
                uint32_t now_ms = (uint32_t)esp_log_timestamp();
                UINT32 intSave = LOS_IntLock();
                bool suppress_touch = s_s59_touch_suppress_until_release;
                if (suppress_touch) {
                    s_s59_touch_pressed = false;
                } else {
                    s_s59_touch_pressed = true;
                    if (!was_pressed) {
                        s_s59_touch_press_latched = true;
                    }
                }
                s_s59_touch_x = points[0].x;
                s_s59_touch_y = points[0].y;
                s_s59_touch_strength = points[0].strength;
                s_s63_touch_active_until_ms = now_ms + OHOS_CAMERA_TOUCH_PAUSE_MS;
                LOS_IntRestore(intSave);
                was_pressed = true;

                if (!suppress_touch && ((press_cnt++ % 10U) == 0U)) {
                    ESP_LOGI(TAG,
                             "S57A direct ST7123 PRESSED count=%u x=%u y=%u strength=%u pauseCameraUntil=%u",
                             (unsigned)count,
                             (unsigned)points[0].x,
                             (unsigned)points[0].y,
                             (unsigned)points[0].strength,
                             (unsigned)(now_ms + OHOS_CAMERA_TOUCH_PAUSE_MS));
                }

                if (!suppress_touch) {
                    ohos_s60_camera_back_from_touch(points[0].x, points[0].y);
                }
            } else {
                /*
                 * S59C:
                 * Make sure LVGL sees a real RELEASED transition.
                 * Previous S59B log showed S57A released but cached read still stayed PRESSED.
                 */
                UINT32 intSave = LOS_IntLock();
                s_s59_touch_pressed = false;
                if (s_s59_touch_suppress_until_release) {
                    s_s59_touch_suppress_until_release = false;
                    s_s59_touch_press_latched = false;
                    s_s59_touch_release_latched = false;
                } else if (was_pressed) {
                    s_s59_touch_release_latched = true;
                }
                LOS_IntRestore(intSave);
                was_pressed = false;

                if ((release_cnt++ % 30U) == 0U) {
                    ESP_LOGI(TAG,
                             "S57A direct ST7123 released read=%s get=%s count=%u cached_released=1",
                             esp_err_to_name(ret),
                             esp_err_to_name(get_ret),
                             (unsigned)count);
                }
                if (s_s60_camera_page_active && ((release_cnt % 60U) == 0U)) {
                    ESP_LOGI(TAG,
                             "S57A camera touch poll alive read=%s get=%s count=%u",
                             esp_err_to_name(ret),
                             esp_err_to_name(get_ret),
                             (unsigned)count);
                }
            }
        }

        OhosLiteosDelayMs(OHOS_TOUCH_POLL_INTERVAL_MS);
    }
}

static esp_err_t ohos_lvgl_register_st7123_touch(lv_display_t *display)
{
    esp_err_t ret = ohos_lvgl_st7123_touch_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OHOS LVGL touch init failed ret=%s", esp_err_to_name(ret));
        return ret;
    }

    if (s_ohos_touch_indev != NULL) {
        return ESP_OK;
    }

    /*
     * S56C:
     * Use the same touch registration path as the OSPTEK official demo:
     * ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG + esp_lv_adapter_register_touch().
     */
    /*
     * S57B:
     * Direct poll only. Keep stable manual display path and do NOT register
     * ST7123 through esp_lv_adapter yet, otherwise two readers may access the
     * same touch controller and hide the real result.
     */
    /*
     * S59A:
     * Register a LVGL pointer input device using cached S57A direct poll data.
     * Do not use esp_lv_adapter_register_touch().
     */
    if (s_ohos_touch_indev == NULL) {
        s_ohos_touch_indev = lv_indev_create();
        lv_indev_set_type(s_ohos_touch_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(s_ohos_touch_indev, display);
        lv_indev_set_read_cb(s_ohos_touch_indev, ohos_lvgl_s59_cached_touch_read_cb);
        ESP_LOGI(TAG, "S59A LVGL cached ST7123 indev registered");
    } else {
        ESP_LOGI(TAG, "S59A LVGL cached ST7123 indev already registered");
    }
    return ESP_OK;

    esp_lv_adapter_touch_config_t touch_config =
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, s_ohos_touch);

    s_ohos_touch_indev = esp_lv_adapter_register_touch(&touch_config);
    if (s_ohos_touch_indev == NULL) {
        ESP_LOGE(TAG, "OHOS LVGL esp_lv_adapter_register_touch failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OHOS LVGL ST7123 touch registered by esp_lv_adapter");
    return ESP_OK;
}



extern void example_lvgl_demo_ui(lv_display_t *disp);

#if CONFIG_EXAMPLE_USE_DMA2D_COPY_FRAME
void example_rounder_flush_area_cb(lv_event_t * event)
{
    lv_area_t * area = lv_event_get_invalidated_area(event);
    area->x1 = ALIGN_DOWN(area->x1, 16);
    area->x2 = ALIGN_UP(area->x2, 16) - 1;
}
#endif

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // pass the draw buffer to the driver
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
}

static void ohos_lvgl_external_font_probe_timer_cb(lv_timer_t *timer)
{
    ESP_LOGI(TAG,
             "P4 external dialog font initial probe skip LVGL-task file IO loaded=%u",
             (unsigned)OhosLvglExternalFontIsLoaded());
    lv_timer_delete(timer);
}

static void example_lvgl_port_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t time_till_next_ms = 0;
    while (1) {
        ohos_lvgl_lock();
        time_till_next_ms = lv_timer_handler();
    #if OHOS_ENABLE_P4_DIALOG_UI
        P4DialogPageProcessPending();
    #endif
    #if OHOS_ENABLE_P4_SENSOR_UI
        P4SensorPageProcessPending();
    #endif
        ohos_lvgl_unlock();
        // in case of triggering a task watch dog time out
        time_till_next_ms = MAX(time_till_next_ms, EXAMPLE_LVGL_TASK_MIN_DELAY_MS);
        // in case of lvgl display not ready yet
        time_till_next_ms = MIN(time_till_next_ms, EXAMPLE_LVGL_TASK_MAX_DELAY_MS);
        lv_tick_inc(time_till_next_ms);
        OhosLiteosDelayMs(time_till_next_ms);
    }
}

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

#if CONFIG_EXAMPLE_MONITOR_REFRESH_BY_GPIO
static bool example_monitor_refresh_rate(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    static int io_level = 0;
    // please note, the real refresh rate should be 2*frequency of this GPIO toggling
    gpio_set_level(EXAMPLE_PIN_NUM_REFRESH_MONITOR, io_level);
    io_level = !io_level;
    return false;
}
#endif

static void example_bsp_enable_dsi_phy_power(void)
{
    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;
#ifdef EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN
    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = EXAMPLE_MIPI_DSI_PHY_PWR_LDO_CHAN,
        .voltage_mv = EXAMPLE_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
#endif
}

static void example_bsp_init_lcd_backlight(void)
{
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_BK_LIGHT
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
#endif
}

static void example_bsp_set_lcd_backlight(uint32_t level)
{
#if EXAMPLE_PIN_NUM_BK_LIGHT >= 0
    gpio_set_level(EXAMPLE_PIN_NUM_BK_LIGHT, level);
#endif
}

#if CONFIG_EXAMPLE_MONITOR_REFRESH_BY_GPIO
static void example_bsp_init_refresh_monitor_io(void)
{
    gpio_config_t monitor_io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_REFRESH_MONITOR,
    };
    ESP_ERROR_CHECK(gpio_config(&monitor_io_conf));
}
#endif



void OhosMainlineDisplayStartRealHw(void)
{
#if CONFIG_EXAMPLE_MONITOR_REFRESH_BY_GPIO
    example_bsp_init_refresh_monitor_io();
#endif

    example_bsp_enable_dsi_phy_power();
    example_bsp_init_lcd_backlight();
    example_bsp_set_lcd_backlight(EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL);

    /*
     * S58A:
     * Keep the stable manual LVGL path, but replace the EK79007 wrapper panel
     * initialization with the OSPTEK official ST7102 hw_lcd_init().
     * Do NOT use esp_lv_adapter_register_display().
     */
    ESP_LOGI(TAG, "S58A use official ST7102 hw_lcd_init, keep manual LVGL path");

    esp_lcd_panel_handle_t mipi_dpi_panel = NULL;
    esp_lcd_panel_io_handle_t mipi_dbi_io = NULL;

    ESP_ERROR_CHECK(hw_lcd_init(&mipi_dpi_panel,
                                &mipi_dbi_io,
                                ESP_LV_ADAPTER_TEAR_AVOID_MODE_DEFAULT_MIPI_DSI,
                                ESP_LV_ADAPTER_ROTATE_0));

    ESP_LOGI(TAG, "S58A official ST7102 panel init done panel=%p io=%p",
             mipi_dpi_panel, mipi_dbi_io);
    s_ohos_display_panel = mipi_dpi_panel;
    ESP_LOGI(TAG, "S63A cached display panel handle=%p", s_ohos_display_panel);

    OhosLiteosDelayMs(120);

    // turn on backlight
    example_bsp_set_lcd_backlight(EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    ohos_lvgl_lock_init();

    /*
     * S56D:
     * Initialize esp_lvgl_adapter core first, otherwise
     * esp_lv_adapter_register_touch() fails with:
     * "Adapter LVGL mutex not initialized".
     */
    static bool s_ohos_lvgl_adapter_inited = false;
    if (!s_ohos_lvgl_adapter_inited) {
        esp_lv_adapter_config_t adapter_cfg = ESP_LV_ADAPTER_DEFAULT_CONFIG();
        esp_err_t adapter_ret = esp_lv_adapter_init(&adapter_cfg);
        ESP_LOGI(TAG, "OHOS LVGL adapter init returned: %s", esp_err_to_name(adapter_ret));
        if (adapter_ret == ESP_OK) {
            s_ohos_lvgl_adapter_inited = true;
        }
    }

    // create a lvgl display
    lv_display_t *display = lv_display_create(EXAMPLE_MIPI_DSI_LCD_H_RES, EXAMPLE_MIPI_DSI_LCD_V_RES);
    s_ohos_main_display = display;
    // associate the mipi panel handle to the display
    lv_display_set_user_data(display, mipi_dpi_panel);
    // set color depth
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // create draw buffer
    void *buf1 = NULL;
    void *buf2 = NULL;
    ESP_LOGI(TAG, "Allocate separate LVGL draw buffers");

    size_t alignment = 1;
#if CONFIG_EXAMPLE_USE_DMA2D_COPY_FRAME
    if (esp_flash_encryption_enabled()) {
        alignment = SOC_GDMA_EXT_MEM_ENC_ALIGNMENT;
        if (EXAMPLE_MIPI_DSI_LCD_H_RES % alignment != 0) {
            ESP_LOGW(TAG, "EXAMPLE_MIPI_DSI_LCD_H_RES is not aligned to %d, may cause MSPI error", alignment);
        }
    }
#endif
    size_t draw_buffer_sz = EXAMPLE_MIPI_DSI_LCD_H_RES * EXAMPLE_LVGL_DRAW_BUF_LINES * 2;

    // Note:
    // Keep the display buffer in **internal** RAM can speed up the UI because LVGL uses it a lot and it should have a fast access time
    // This example allocate the buffer from PSRAM mainly because we want to save the internal RAM
    buf1 = heap_caps_aligned_calloc(alignment, 1, draw_buffer_sz, MALLOC_CAP_SPIRAM);
    assert(buf1);
    buf2 = heap_caps_aligned_calloc(alignment, 1, draw_buffer_sz, MALLOC_CAP_SPIRAM);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);

    esp_err_t touch_ret = ohos_lvgl_register_st7123_touch(display);
    ESP_LOGI(TAG, "OHOS LVGL touch register returned: %s", esp_err_to_name(touch_ret));

#if CONFIG_EXAMPLE_USE_DMA2D_COPY_FRAME
    // If flash encryption is enabled, DMA2D requires the flush buffer address and size to be aligned to 16 bytes.
    // We need to round the flush area to the multiple of 16.
    if (esp_flash_encryption_enabled()) {
        ESP_LOGI(TAG, "Register event callback for LVGL flush area rounding");
        lv_display_add_event_cb(display, example_rounder_flush_area_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    }
#endif

    ESP_LOGI(TAG, "Register DPI panel event callback for LVGL flush ready notification");
    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = example_notify_lvgl_flush_ready,
#if CONFIG_EXAMPLE_MONITOR_REFRESH_BY_GPIO
        .on_refresh_done = example_monitor_refresh_rate,
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(mipi_dpi_panel, &cbs, display));

    ESP_LOGI(TAG, "Use LiteOS LVGL task loop for LVGL tick");

    (void)OhosLvglLfsFontRegister();
    for (int font_try = 0; font_try < 1 && !OhosLvglExternalFontIsLoaded(); ++font_try) {
        uint32_t font_ret = OhosLvglExternalFontLoad();
        ESP_LOGI(TAG,
                 "P4 external dialog font preload try=%d ret=%u loaded=%u",
                 font_try + 1,
                 (unsigned)font_ret,
                 (unsigned)OhosLvglExternalFontIsLoaded());
        if (font_ret == 0U) {
            break;
        }
        OhosLiteosDelayMs(250);
    }
    if (!OhosLvglExternalFontIsLoaded()) {
        ESP_LOGW(TAG, "external font unavailable during boot; continue with LVGL default font");
    }

    ESP_LOGI(TAG, "Create LVGL task");
    UINT32 lvgl_task_ret = OhosLiteosCreateTask("LVGL",
                                                example_lvgl_port_task,
                                                NULL,
                                                EXAMPLE_LVGL_TASK_PRIORITY,
                                                EXAMPLE_LVGL_TASK_STACK_SIZE,
                                                &s_lvgl_task_id);
    ESP_LOGI(TAG, "LVGL LiteOS task create ret=%u taskId=%u",
             (unsigned)lvgl_task_ret,
             (unsigned)s_lvgl_task_id);

    ESP_LOGI(TAG, "Display LVGL Meter Widget");
    board_display_mark_on(EXAMPLE_MIPI_DSI_LCD_H_RES,
                          EXAMPLE_MIPI_DSI_LCD_V_RES,
                          EXAMPLE_MIPI_DSI_LANE_NUM,
                          EXAMPLE_MIPI_DSI_DPI_CLK_MHZ,
                          16);
    ESP_LOGI(TAG, "Board display state marked on: %dx%d lanes=%d dpi=%dMHz bpp=%d",
             EXAMPLE_MIPI_DSI_LCD_H_RES,
             EXAMPLE_MIPI_DSI_LCD_V_RES,
             EXAMPLE_MIPI_DSI_LANE_NUM,
             EXAMPLE_MIPI_DSI_DPI_CLK_MHZ,
             24);
    ESP_LOGI(TAG, "Display init done; OHOS mainline bringup will run from app_main");
#if 0
#if 0
    ESP_LOGI(TAG, "Start OpenHarmony LiteOS-M bringup(false) after display on");
    int ohos_ret = ohos_liteos_bringup(false);
    ESP_LOGI(TAG, "OpenHarmony LiteOS-M bringup(false) returned %d", ohos_ret);
#endif
#endif
    ohos_lvgl_lock();
    // example_lvgl_demo_ui(display);  // disabled for frame animation test
    lv_timer_create(ohos_lvgl_external_font_probe_timer_cb, 1500, NULL);
    example_show_gif_overlay();
    ESP_LOGI(TAG, "Display LVGL frame animation test");
    ohos_s60_show_gif_page();
    ESP_LOGI(TAG, "Display real HW init done; camera preview will be started by CameraService");
    ohos_lvgl_unlock();


}



static lv_obj_t *s_spin_img = NULL;
static uint32_t s_spin_idx = 0;

static void spin_frame_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    extern const lv_image_dsc_t * const spin_frames[];
    extern const uint32_t spin_frame_count;

    if (s_spin_img == NULL || spin_frame_count == 0) {
        return;
    }

    s_spin_idx = (s_spin_idx + 1U) % spin_frame_count;
    if (s_spin_img == NULL || !lv_obj_is_valid(s_spin_img)) {
        ESP_LOGW(TAG, "S68B legacy frame timer target invalid, delete timer");
        lv_timer_delete(timer);
        return;
    }

    lv_image_set_src(s_spin_img, spin_frames[s_spin_idx]);
}

static void example_show_gif_overlay(void)
{
    extern const lv_image_dsc_t * const spin_frames[];
    extern const uint32_t spin_frame_count;

    lv_obj_clean(lv_screen_active());

    if (spin_frame_count == 0) {
        ESP_LOGW(TAG, "No spin frames available");
        return;
    }

    s_spin_idx = 0;
    s_spin_img = lv_image_create(lv_screen_active());
    lv_image_set_src(s_spin_img, spin_frames[0]);
    lv_obj_center(s_spin_img);

    lv_timer_create(spin_frame_timer_cb, 120, NULL);

    ESP_LOGI(TAG, "LVGL frame animation object created: 96x96 frames=%u", (unsigned)spin_frame_count);
}




static esp_err_t board_camera_read_reg16_8(i2c_master_dev_handle_t dev, uint16_t reg, uint8_t *val)
{
    uint8_t tx[2] = {
        (uint8_t)(reg >> 8),
        (uint8_t)(reg & 0xff),
    };

    return i2c_master_transmit_receive(dev, tx, sizeof(tx), val, 1, 200);
}

int board_camera_get_info(int *detected,
                          int *pid,
                          int *h_res,
                          int *v_res,
                          int *fps,
                          int *sccb_scl,
                          int *sccb_sda)
{
    const int cam_scl = 8;
    const int cam_sda = 7;
    const uint8_t cam_addr = 0x30;

    if (detected) *detected = 0;
    if (pid) *pid = 0;
    if (h_res) *h_res = 0;
    if (v_res) *v_res = 0;
    if (fps) *fps = 0;
    if (sccb_scl) *sccb_scl = cam_scl;
    if (sccb_sda) *sccb_sda = cam_sda;

    i2c_master_bus_handle_t bus = NULL;
    i2c_master_dev_handle_t dev = NULL;

    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = cam_sda,
        .scl_io_num = cam_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &bus);
    if (ret != ESP_OK) {
        esp_rom_printf("[BOARD-CAMERA] create I2C bus failed ret=%d\n", ret);
        return -1;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = cam_addr,
        .scl_speed_hz = 100000,
    };

    ret = i2c_master_bus_add_device(bus, &dev_cfg, &dev);
    if (ret != ESP_OK) {
        esp_rom_printf("[BOARD-CAMERA] add SC2336 device failed ret=%d\n", ret);
        i2c_del_master_bus(bus);
        return -1;
    }

    uint8_t pid_h = 0;
    uint8_t pid_l = 0;

    esp_err_t r1 = board_camera_read_reg16_8(dev, 0x3107, &pid_h);
    esp_err_t r2 = board_camera_read_reg16_8(dev, 0x3108, &pid_l);

    uint16_t sensor_pid = ((uint16_t)pid_h << 8) | pid_l;
    int ok = (r1 == ESP_OK && r2 == ESP_OK && sensor_pid == 0xcb3a);

    esp_rom_printf("[BOARD-CAMERA] SC2336 real PID probe scl=%d sda=%d addr=0x%02x r1=%d r2=%d pid=0x%04x ok=%d\n",
                   cam_scl,
                   cam_sda,
                   cam_addr,
                   r1,
                   r2,
                   sensor_pid,
                   ok);

    i2c_master_bus_rm_device(dev);
    i2c_del_master_bus(bus);

    if (!ok) {
        return -1;
    }

    if (detected) *detected = 1;
    if (pid) *pid = sensor_pid;
    if (h_res) *h_res = 1280;
    if (v_res) *v_res = 720;
    if (fps) *fps = 30;
    if (sccb_scl) *sccb_scl = cam_scl;
    if (sccb_sda) *sccb_sda = cam_sda;

    return 0;
}


#include "esp_heap_caps.h"

#define OHOS_CAMERA_PREVIEW_W 480
#define OHOS_CAMERA_PREVIEW_H 800

/*
 * S62B:
 * Camera source is 1280x720 while LCD is 480x800.
 * Use center-crop + scale-to-fill to avoid skinny/stretch distortion.
 *
 * LVGL RGB565 byte order may differ from direct esp_lcd_panel_draw_bitmap path.
 * Try byte swap first. If color becomes worse, change this to 0.
 */
#ifndef OHOS_CAMERA_RGB565_BYTE_SWAP
#define OHOS_CAMERA_RGB565_BYTE_SWAP 0
#endif

#ifndef OHOS_CAMERA_RGB565_RB_SWAP
#define OHOS_CAMERA_RGB565_RB_SWAP 1

#ifndef OHOS_ALIGN_UP
#define OHOS_ALIGN_UP(num, align)    (((num) + ((align) - 1)) & ~((align) - 1))
#endif

#define OHOS_CAMERA_DIRECT_VIEW_W    480
#define OHOS_CAMERA_DIRECT_VIEW_H    720
#define OHOS_CAMERA_BOTTOM_BAR_H     80
#endif


static lv_obj_t *s_camera_preview_img = NULL;
static uint8_t *s_camera_preview_buf = NULL;
static lv_image_dsc_t s_camera_preview_dsc;
static lv_timer_t *s_camera_preview_timer = NULL;
static volatile int s_camera_preview_frame_ready = 0;
static volatile uint32_t s_camera_preview_frame_id = 0;
static uint32_t s_camera_preview_frame_drawn_id = 0;


static volatile int s_camera_preview_async_pending = 0;

/* S60A: page state */
static volatile int s_s60_camera_start_task_running = 0;
static volatile int s_s60_camera_back_pending = 0;
static volatile int s_s60_camera_direct_back_async_pending = 0;
static volatile int s_s60_camera_back_ui_async_pending = 0;
static lv_obj_t *s_camera_back_bar = NULL;
static lv_obj_t *s_camera_back_btn = NULL;

static void ohos_s60_camera_start_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "S64B camera start task begin: OSPTEK official video flow");
    uint32_t ret = OhosCameraServiceStartOsptekPreview();
    ESP_LOGI(TAG, "S64B OhosCameraServiceStartOsptekPreview ret=%u", (unsigned)ret);
    s_s60_camera_start_task_running = 0;
    return;
}

static void ohos_s60_camera_back_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "S60A camera back event=%d, return to Demo Center", (int)code);
        ohos_s60_camera_back_common("lvgl-event");
    }
}

static void ohos_s60_camera_back_ui_async_cb(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "S60A camera back UI async, show Demo Center");
    s_camera_back_bar = NULL;
    s_camera_back_btn = NULL;
    ohos_s60_show_gif_page();
    s_s60_camera_back_ui_async_pending = 0;
    s_s60_camera_back_pending = 0;
}

static void ohos_s60_camera_back_common(const char *source)
{
    if (s_s60_camera_back_pending) {
        return;
    }

    s_s60_camera_back_pending = 1;

    ESP_LOGI(TAG, "S60A camera back source=%s, return to Demo Center",
             source ? source : "unknown");

    s_s60_camera_page_active = 0;
    s_s63_direct_camera_enabled = 0;
    s_s63_touch_active_until_ms = (uint32_t)esp_log_timestamp() + 1000U;
    s_camera_back_bar = NULL;
    s_camera_back_btn = NULL;

    OhosCameraServiceStopOsptekPreview();

    if (!s_s60_camera_back_ui_async_pending) {
        s_s60_camera_back_ui_async_pending = 1;
        lv_async_call(ohos_s60_camera_back_ui_async_cb, NULL);
    }
}

static bool ohos_s60_camera_touch_hits_back(uint16_t x,
                                            uint16_t y,
                                            const char **zone)
{
    const uint16_t band = OHOS_CAMERA_BACK_TOUCH_BAND;
    const uint16_t w = OHOS_LVGL_TOUCH_H_RES;
    const uint16_t h = OHOS_LVGL_TOUCH_V_RES;

    if (zone != NULL) {
        *zone = "none";
    }

    if (y >= (uint16_t)(h - band)) {
        if (zone != NULL) {
            *zone = "bottom";
        }
        return true;
    }

    if (y <= band) {
        if (zone != NULL) {
            *zone = "mirror-y-top";
        }
        return true;
    }

    /*
     * ST7123 can report coordinates in the panel-native orientation while LVGL
     * sees a portrait screen. Treat the swapped/mirrored bottom band as Back too.
     */
    if (x >= (uint16_t)(h - band)) {
        if (zone != NULL) {
            *zone = "swap-x-bottom";
        }
        return true;
    }

    if (x <= band && y > w) {
        if (zone != NULL) {
            *zone = "swap-x-mirror-bottom";
        }
        return true;
    }

    if (y > h) {
        if (zone != NULL) {
            *zone = "raw-outside";
        }
        return true;
    }

    return false;
}

static void ohos_s60_camera_back_from_touch(uint16_t x, uint16_t y)
{
    if (!s_s60_camera_page_active || s_s60_camera_back_pending) {
        return;
    }

    const char *zone = "none";
    bool hit = ohos_s60_camera_touch_hits_back(x, y, &zone);

    static uint32_t s_s60_camera_touch_log_cnt = 0;
    if (hit || ((s_s60_camera_touch_log_cnt++ % 10U) == 0U)) {
        ESP_LOGI(TAG,
                 "S60A camera touch x=%u y=%u backHit=%d zone=%s",
                 (unsigned)x,
                 (unsigned)y,
                 hit ? 1 : 0,
                 zone);
    }

    if (!hit) {
        return;
    }

    ESP_LOGI(TAG, "S60A direct back touch x=%u y=%u zone=%s",
             (unsigned)x,
             (unsigned)y,
             zone);
    if (!s_s60_camera_direct_back_async_pending) {
        s_s63_direct_camera_enabled = 0;
        s_s60_camera_direct_back_async_pending = 1;
        ohos_s60_camera_back_common("direct-touch");
        s_s60_camera_direct_back_async_pending = 0;
    }
}

static void ohos_s60_create_camera_back_button(void)
{
    if (s_camera_back_btn != NULL && lv_obj_is_valid(s_camera_back_btn)) {
        if (s_camera_back_bar != NULL && lv_obj_is_valid(s_camera_back_bar)) {
            lv_obj_move_foreground(s_camera_back_bar);
        }
        lv_obj_move_foreground(s_camera_back_btn);
        return;
    }
    s_camera_back_bar = NULL;
    s_camera_back_btn = NULL;

    /*
     * S63A:
     * Camera is drawn directly to the top 480x720 region.
     * Bottom 480x80 region is kept for LVGL controls.
     */
    lv_obj_t *bar = lv_obj_create(lv_screen_active());
    s_camera_back_bar = bar;
    lv_obj_set_size(bar, 480, OHOS_CAMERA_BOTTOM_BAR_H);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    s_camera_back_btn = lv_button_create(bar);
    lv_obj_set_size(s_camera_back_btn, 300, 55);
    lv_obj_align(s_camera_back_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(s_camera_back_btn, ohos_s60_camera_back_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *label = lv_label_create(s_camera_back_btn);
    lv_label_set_text(label, "Back");
    lv_obj_center(label);

    ESP_LOGI(TAG, "S63A camera bottom back button created");
    lv_obj_invalidate(bar);
    if (s_ohos_main_display != NULL) {
        lv_refr_now(s_ohos_main_display);
    }
}



static void ohos_s60_request_camera_page(void)
{
    ESP_LOGI(TAG, "S60A request camera page");

    s_s60_camera_page_active = 1;
    s_s63_direct_camera_enabled = 1;
    s_s60_camera_back_pending = 0;
    s_s60_camera_direct_back_async_pending = 0;
    s_camera_preview_img = NULL;
    s_camera_back_bar = NULL;
    s_camera_back_btn = NULL;

    lv_obj_clean(lv_screen_active());
    s_spin_img = NULL;

    /*
     * S63A:
     * The camera will be drawn directly to y=0..719.
     * LVGL only keeps the bottom control bar.
     */
    ohos_s60_create_camera_back_button();

    if (!s_s60_camera_start_task_running) {
        s_s60_camera_start_task_running = 1;
        UINT32 ret = OhosLiteosCreateTask("s60_cam_start",
                                          ohos_s60_camera_start_task,
                                          NULL,
                                          OHOS_CAMERA_START_TASK_PRIO,
                                          OHOS_CAMERA_START_TASK_STACK,
                                          NULL);
        ESP_LOGI(TAG, "S60A camera start LiteOS task create ret=%u", (unsigned)ret);
        if (ret != LOS_OK) {
            s_s60_camera_start_task_running = 0;
        }
    } else {
        ESP_LOGI(TAG, "S60A camera start task already running");
    }
}

static void ohos_s60_request_camera_page_async_cb(void *arg)
{
    (void)arg;
    ohos_s60_request_camera_page();
}

static void camera_preview_async_cb(void *arg)
{
    (void)arg;

    if (s_camera_preview_buf == NULL) {
        ESP_LOGE(TAG, "camera preview async: no buffer");
        s_camera_preview_async_pending = 0;
        return;
    }

    memset(&s_camera_preview_dsc, 0, sizeof(s_camera_preview_dsc));
#if LVGL_VERSION_MAJOR >= 9
    s_camera_preview_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
    s_camera_preview_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s_camera_preview_dsc.header.w = OHOS_CAMERA_PREVIEW_W;
    s_camera_preview_dsc.header.h = OHOS_CAMERA_PREVIEW_H;
    s_camera_preview_dsc.header.stride = OHOS_CAMERA_PREVIEW_W * 2;
    s_camera_preview_dsc.data_size = OHOS_CAMERA_PREVIEW_W * OHOS_CAMERA_PREVIEW_H * 2;
    s_camera_preview_dsc.data = s_camera_preview_buf;

    if (s_camera_preview_img == NULL) {
        ESP_LOGI(TAG, "camera preview async: create LVGL image begin");

        lv_obj_clean(lv_screen_active());
        s_spin_img = NULL;

        s_camera_preview_img = lv_image_create(lv_screen_active());
        lv_image_set_src(s_camera_preview_img, &s_camera_preview_dsc);
        lv_obj_center(s_camera_preview_img);

        ESP_LOGI(TAG, "Camera live RGB565 image object created: preview=%ux%u",
                 OHOS_CAMERA_PREVIEW_W,
                 OHOS_CAMERA_PREVIEW_H);
        s_camera_back_btn = NULL;
        ohos_s60_create_camera_back_button();
        if (s_camera_back_btn != NULL) {
            lv_obj_move_foreground(s_camera_back_btn);
        }
    } else {
        /*
         * Reuse the existing image object. The pixel buffer content has been
         * updated before this async callback, so only invalidate the image.
         */
        lv_image_set_src(s_camera_preview_img, &s_camera_preview_dsc);
        lv_obj_invalidate(s_camera_preview_img);
        if (s_camera_back_btn != NULL) {
            lv_obj_move_foreground(s_camera_back_btn);
        }
    }

    s_camera_preview_async_pending = 0;
}



static esp_err_t ohos_s63_ensure_ppa_direct_buffer(void)
{
    if (s_s63_ppa_srm_handle == NULL) {
        ppa_client_config_t ppa_srm_config = {
            .oper_type = PPA_OPERATION_SRM,
        };
        esp_err_t ret = ppa_register_client(&ppa_srm_config, &s_s63_ppa_srm_handle);
        ESP_LOGI(TAG, "S63A ppa_register_client ret=%s handle=%p",
                 esp_err_to_name(ret), s_s63_ppa_srm_handle);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    if (s_s63_cache_line_size == 0) {
        esp_err_t ret = ESP_OK;
        s_s63_cache_line_size = 128;
        ESP_LOGI(TAG, "S63A esp_cache_get_alignment ret=%s align=%u",
                 esp_err_to_name(ret), (unsigned)s_s63_cache_line_size);
        if (ret != ESP_OK) {
            return ret;
        }
        if (s_s63_cache_line_size == 0) {
            s_s63_cache_line_size = 128;
        }
    }

    if (s_s63_camera_lcd_buf == NULL) {
        size_t out_size = OHOS_ALIGN_UP(OHOS_CAMERA_DIRECT_VIEW_W *
                                        OHOS_CAMERA_DIRECT_VIEW_H * 2,
                                        s_s63_cache_line_size);
        s_s63_camera_lcd_buf = heap_caps_aligned_calloc(s_s63_cache_line_size,
                                                        1,
                                                        out_size,
                                                        MALLOC_CAP_SPIRAM);
        ESP_LOGI(TAG, "S63A direct lcd buf alloc ptr=%p size=%u",
                 s_s63_camera_lcd_buf, (unsigned)out_size);
        if (s_s63_camera_lcd_buf == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }

    return ESP_OK;
}


static inline uint32_t ohos_s66_clamp_u32(uint32_t v, uint32_t max_v)
{
    return (v > max_v) ? max_v : v;
}

static void ohos_s66_tune_rgb565_buffer(uint8_t *buf, uint32_t w, uint32_t h)
{
#if OHOS_S66_COLOR_TUNE_ENABLE
    if (buf == NULL || w == 0 || h == 0) {
        return;
    }

    uint16_t *px = (uint16_t *)buf;
    uint32_t total = w * h;

    for (uint32_t i = 0; i < total; ++i) {
        uint16_t v = px[i];

        uint32_t r = (v >> 11) & 0x1f;
        uint32_t g = (v >> 5) & 0x3f;
        uint32_t b = v & 0x1f;

        r = ohos_s66_clamp_u32((r * OHOS_S66_R_GAIN_NUM) / OHOS_S66_R_GAIN_DEN, 31);
        g = ohos_s66_clamp_u32((g * OHOS_S66_G_GAIN_NUM) / OHOS_S66_G_GAIN_DEN, 63);
        b = ohos_s66_clamp_u32((b * OHOS_S66_B_GAIN_NUM) / OHOS_S66_B_GAIN_DEN, 31);

        px[i] = (uint16_t)((r << 11) | (g << 5) | b);
    }
#endif
}

static void ohos_s63_rotate_rgb565_180(uint8_t *buf, uint32_t w, uint32_t h)
{
#if OHOS_S63_SOFTWARE_ROTATE_180
    if (buf == NULL || w == 0 || h == 0) {
        return;
    }

    uint16_t *px = (uint16_t *)buf;
    uint32_t left = 0;
    uint32_t right = (w * h) - 1U;

    while (left < right) {
        uint16_t tmp = px[left];
        px[left] = px[right];
        px[right] = tmp;
        left++;
        right--;
    }
#else
    (void)buf;
    (void)w;
    (void)h;
#endif
}


static void ohos_s63_draw_rgb565_direct(const uint8_t *rgb565,
                                        uint32_t src_w,
                                        uint32_t src_h,
                                        uint32_t src_stride)
{
    if (!s_s63_direct_camera_enabled || !s_s60_camera_page_active) {
        return;
    }

    if (rgb565 == NULL || src_w == 0 || src_h == 0) {
        return;
    }

    if (src_stride == 0) {
        src_stride = src_w * 2;
    }

    if (s_ohos_display_panel == NULL) {
        ESP_LOGW(TAG, "S63A display panel is NULL, skip direct frame");
        return;
    }

    uint32_t now_ms = (uint32_t)(esp_log_timestamp());
    if (ohos_s63_is_touch_active(now_ms)) {
        static uint32_t s_s63_touch_skip_cnt = 0;
        if ((s_s63_touch_skip_cnt++ % 30U) == 0U) {
            ESP_LOGI(TAG,
                     "S63C skip RGB565 direct frame while touch active now=%u until=%u",
                     (unsigned)now_ms,
                     (unsigned)s_s63_touch_active_until_ms);
        }
        return;
    }

    if (ohos_s63_ensure_ppa_direct_buffer() != ESP_OK) {
        return;
    }

    /*
     * Official-like path:
     * crop the center 480x720 block from 1280x720 RGB565 frame,
     * then draw it directly to ST7102 top area.
     * Bottom 80 pixels are reserved for LVGL Back button.
     */
    uint32_t crop_w = (src_w > OHOS_CAMERA_DIRECT_VIEW_W) ? OHOS_CAMERA_DIRECT_VIEW_W : src_w;
    uint32_t crop_h = (src_h > OHOS_CAMERA_DIRECT_VIEW_H) ? OHOS_CAMERA_DIRECT_VIEW_H : src_h;
    uint32_t crop_x = (src_w > crop_w) ? ((src_w - crop_w) / 2U) : 0;
    uint32_t crop_y = (src_h > crop_h) ? ((src_h - crop_h) / 2U) : 0;

    ppa_srm_oper_config_t srm_config = {
        .in.buffer = (void *)rgb565,
        .in.pic_w = src_w,
        .in.pic_h = src_h,
        .in.block_w = crop_w,
        .in.block_h = crop_h,
        .in.block_offset_x = crop_x,
        .in.block_offset_y = crop_y,
        .in.srm_cm = PPA_SRM_COLOR_MODE_RGB565,

        .out.buffer = s_s63_camera_lcd_buf,
        .out.buffer_size = OHOS_ALIGN_UP(OHOS_CAMERA_DIRECT_VIEW_W *
                                         OHOS_CAMERA_DIRECT_VIEW_H * 2,
                                         s_s63_cache_line_size),
        .out.pic_w = OHOS_CAMERA_DIRECT_VIEW_W,
        .out.pic_h = OHOS_CAMERA_DIRECT_VIEW_H,
        .out.block_offset_x = 0,
        .out.block_offset_y = 0,
        .out.srm_cm = PPA_SRM_COLOR_MODE_RGB565,

        .rotation_angle = OHOS_S63_DIRECT_ROTATION,
        .scale_x = 1,
        .scale_y = 1,
        .rgb_swap = OHOS_S63_DIRECT_RGB_SWAP,
        .byte_swap = OHOS_S63_DIRECT_BYTE_SWAP,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

#if OHOS_S63_DIRECT_MIN_FRAME_INTERVAL_MS > 0
    if (s_s63_last_draw_ms != 0 &&
        (uint32_t)(now_ms - s_s63_last_draw_ms) < OHOS_S63_DIRECT_MIN_FRAME_INTERVAL_MS) {
        return;
    }
#endif
    s_s63_last_draw_ms = now_ms;

    static uint32_t s_s63_frame_cnt = 0;
    if ((s_s63_frame_cnt++ % 30U) == 0U) {
        ESP_LOGI(TAG,
                 "S63B direct camera frame src=%ux%u stride=%u crop=(%u,%u %ux%u) draw=%ux%u byte_swap=%d rgb_swap=%d",
                 (unsigned)src_w,
                 (unsigned)src_h,
                 (unsigned)src_stride,
                 (unsigned)crop_x,
                 (unsigned)crop_y,
                 (unsigned)crop_w,
                 (unsigned)crop_h,
                 OHOS_CAMERA_DIRECT_VIEW_W,
                 OHOS_CAMERA_DIRECT_VIEW_H,
                 OHOS_S63_DIRECT_BYTE_SWAP,
                 OHOS_S63_DIRECT_RGB_SWAP);
        ESP_LOGI(TAG,
                 "S63B direct camera orientation ppa_rotation=%d sw_rotate_180=%d",
                 (int)OHOS_S63_DIRECT_ROTATION,
                 OHOS_S63_SOFTWARE_ROTATE_180);
    }

    uint32_t frame_start_ms = now_ms;
    esp_err_t ret = ppa_do_scale_rotate_mirror(s_s63_ppa_srm_handle, &srm_config);
    uint32_t ppa_done_ms = (uint32_t)esp_log_timestamp();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "S63A ppa_do_scale_rotate_mirror failed: %s", esp_err_to_name(ret));
        return;
    }

    ohos_s66_tune_rgb565_buffer(s_s63_camera_lcd_buf,
                                OHOS_CAMERA_DIRECT_VIEW_W,
                                OHOS_CAMERA_DIRECT_VIEW_H);
    ohos_s63_rotate_rgb565_180(s_s63_camera_lcd_buf,
                               OHOS_CAMERA_DIRECT_VIEW_W,
                               OHOS_CAMERA_DIRECT_VIEW_H);

#if OHOS_S66_COLOR_TUNE_ENABLE || OHOS_S63_SOFTWARE_ROTATE_180
    /*
     * CPU may modify the PSRAM RGB565 buffer after PPA.
     * Flush cache before LCD/DSI DMA reads it, otherwise color tuning may
     * look ineffective or software rotation may not reach the panel.
     */
    esp_cache_msync(s_s63_camera_lcd_buf,
                    OHOS_CAMERA_DIRECT_VIEW_W * OHOS_CAMERA_DIRECT_VIEW_H * 2,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M);
#endif

    ret = esp_lcd_panel_draw_bitmap(s_ohos_display_panel,
                                    0,
                                    0,
                                    OHOS_CAMERA_DIRECT_VIEW_W,
                                    OHOS_CAMERA_DIRECT_VIEW_H,
                                    s_s63_camera_lcd_buf);
    uint32_t draw_done_ms = (uint32_t)esp_log_timestamp();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "S63A esp_lcd_panel_draw_bitmap failed: %s", esp_err_to_name(ret));
    }

    static uint32_t s_s63_perf_start_ms = 0;
    static uint32_t s_s63_perf_frames = 0;
    if (s_s63_perf_start_ms == 0) {
        s_s63_perf_start_ms = frame_start_ms;
    }
    s_s63_perf_frames++;
    uint32_t perf_elapsed_ms = (uint32_t)(draw_done_ms - s_s63_perf_start_ms);
    if (perf_elapsed_ms >= 3000U) {
        uint32_t fps_x10 = (s_s63_perf_frames * 10000U) / perf_elapsed_ms;
        ESP_LOGI(TAG,
                 "S63D direct display fps=%u.%u frames=%u elapsed=%ums ppa=%ums draw=%ums tune=%d",
                 (unsigned)(fps_x10 / 10U),
                 (unsigned)(fps_x10 % 10U),
                 (unsigned)s_s63_perf_frames,
                 (unsigned)perf_elapsed_ms,
                 (unsigned)(ppa_done_ms - frame_start_ms),
                 (unsigned)(draw_done_ms - ppa_done_ms),
                 OHOS_S66_COLOR_TUNE_ENABLE);
        s_s63_perf_start_ms = draw_done_ms;
        s_s63_perf_frames = 0;
    }
}


static void ohos_s65_draw_rgb888_direct(const uint8_t *rgb888,
                                        uint32_t src_w,
                                        uint32_t src_h,
                                        uint32_t src_stride)
{
    if (!s_s63_direct_camera_enabled || !s_s60_camera_page_active) {
        return;
    }

    if (rgb888 == NULL || src_w == 0 || src_h == 0) {
        return;
    }

    if (src_stride == 0) {
        src_stride = src_w * 3;
    }

    if (s_ohos_display_panel == NULL) {
        ESP_LOGW(TAG, "S65A display panel is NULL, skip RGB888 direct frame");
        return;
    }

    uint32_t now_ms = (uint32_t)esp_log_timestamp();
    if (ohos_s63_is_touch_active(now_ms)) {
        static uint32_t s_s65_touch_skip_cnt = 0;
        if ((s_s65_touch_skip_cnt++ % 30U) == 0U) {
            ESP_LOGI(TAG,
                     "S65B skip RGB888 direct frame while touch active now=%u until=%u",
                     (unsigned)now_ms,
                     (unsigned)s_s63_touch_active_until_ms);
        }
        return;
    }

    if (s_s63_ppa_srm_handle == NULL) {
        ppa_client_config_t ppa_srm_config = {
            .oper_type = PPA_OPERATION_SRM,
        };
        esp_err_t ret = ppa_register_client(&ppa_srm_config, &s_s63_ppa_srm_handle);
        ESP_LOGI(TAG, "S65A ppa_register_client ret=%s handle=%p",
                 esp_err_to_name(ret), s_s63_ppa_srm_handle);
        if (ret != ESP_OK) {
            return;
        }
    }

    if (s_s63_cache_line_size == 0) {
        s_s63_cache_line_size = 128;
    }

    if (s_s63_camera_lcd_buf == NULL) {
        size_t out_size = OHOS_ALIGN_UP(OHOS_CAMERA_DIRECT_VIEW_W *
                                        OHOS_CAMERA_DIRECT_VIEW_H * 3,
                                        s_s63_cache_line_size);
        s_s63_camera_lcd_buf = heap_caps_aligned_calloc(s_s63_cache_line_size,
                                                        1,
                                                        out_size,
                                                        MALLOC_CAP_SPIRAM);
        ESP_LOGI(TAG, "S65A RGB888 direct lcd buf alloc ptr=%p size=%u",
                 s_s63_camera_lcd_buf, (unsigned)out_size);
        if (s_s63_camera_lcd_buf == NULL) {
            return;
        }
    }

    uint32_t crop_w = (src_w > OHOS_CAMERA_DIRECT_VIEW_W) ? OHOS_CAMERA_DIRECT_VIEW_W : src_w;
    uint32_t crop_h = (src_h > OHOS_CAMERA_DIRECT_VIEW_H) ? OHOS_CAMERA_DIRECT_VIEW_H : src_h;
    uint32_t crop_x = (src_w > crop_w) ? ((src_w - crop_w) / 2U) : 0;
    uint32_t crop_y = (src_h > crop_h) ? ((src_h - crop_h) / 2U) : 0;

#if OHOS_S63_DIRECT_MIN_FRAME_INTERVAL_MS > 0
    if (s_s63_last_draw_ms != 0 &&
        (uint32_t)(now_ms - s_s63_last_draw_ms) < OHOS_S63_DIRECT_MIN_FRAME_INTERVAL_MS) {
        return;
    }
#endif
    s_s63_last_draw_ms = now_ms;

    ppa_srm_oper_config_t srm_config = {
        .in.buffer = (void *)rgb888,
        .in.pic_w = src_w,
        .in.pic_h = src_h,
        .in.block_w = crop_w,
        .in.block_h = crop_h,
        .in.block_offset_x = crop_x,
        .in.block_offset_y = crop_y,
        .in.srm_cm = PPA_SRM_COLOR_MODE_RGB888,

        .out.buffer = s_s63_camera_lcd_buf,
        .out.buffer_size = OHOS_ALIGN_UP(OHOS_CAMERA_DIRECT_VIEW_W *
                                         OHOS_CAMERA_DIRECT_VIEW_H * 3,
                                         s_s63_cache_line_size),
        .out.pic_w = OHOS_CAMERA_DIRECT_VIEW_W,
        .out.pic_h = OHOS_CAMERA_DIRECT_VIEW_H,
        .out.block_offset_x = 0,
        .out.block_offset_y = 0,
        .out.srm_cm = PPA_SRM_COLOR_MODE_RGB888,

        .rotation_angle = OHOS_S63_DIRECT_ROTATION,
        .scale_x = 1,
        .scale_y = 1,
        .rgb_swap = 0,
        .byte_swap = 0,
        .mode = PPA_TRANS_MODE_BLOCKING,
    };

    static uint32_t s_s65_frame_cnt = 0;
    if ((s_s65_frame_cnt++ % 30U) == 0U) {
        ESP_LOGI(TAG,
                 "S65A RGB888 direct camera frame src=%ux%u stride=%u crop=(%u,%u %ux%u) draw=%ux%u",
                 (unsigned)src_w,
                 (unsigned)src_h,
                 (unsigned)src_stride,
                 (unsigned)crop_x,
                 (unsigned)crop_y,
                 (unsigned)crop_w,
                 (unsigned)crop_h,
                 OHOS_CAMERA_DIRECT_VIEW_W,
                 OHOS_CAMERA_DIRECT_VIEW_H);
    }

    esp_err_t ret = ppa_do_scale_rotate_mirror(s_s63_ppa_srm_handle, &srm_config);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "S65A ppa_do_scale_rotate_mirror failed: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_lcd_panel_draw_bitmap(s_ohos_display_panel,
                                    0,
                                    0,
                                    OHOS_CAMERA_DIRECT_VIEW_W,
                                    OHOS_CAMERA_DIRECT_VIEW_H,
                                    s_s63_camera_lcd_buf);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "S65A esp_lcd_panel_draw_bitmap failed: %s", esp_err_to_name(ret));
    }
}


static void ohos_s64_osptek_video_frame_cb(uint8_t *camera_buf,
                                           uint8_t camera_buf_index,
                                           uint32_t camera_buf_hes,
                                           uint32_t camera_buf_ves,
                                           size_t camera_buf_len,
                                           void *user_data)
{
    (void)camera_buf_index;
    (void)camera_buf_len;
    (void)user_data;

    if (!s_s63_direct_camera_enabled || !s_s60_camera_page_active) {
        return;
    }

    if (camera_buf == NULL || camera_buf_hes == 0 || camera_buf_ves == 0) {
        return;
    }

    /*
     * S64B:
     * Use the official OSPTEK app_video frame source.
     * Display is still handled by our S63 direct top-area renderer.
     */
#if OSPTEK_VIDEO_FMT == OSPTEK_VIDEO_FMT_RGB565
    ohos_s63_draw_rgb565_direct(camera_buf,
                                camera_buf_hes,
                                camera_buf_ves,
                                camera_buf_hes * 2);
#elif OSPTEK_VIDEO_FMT == OSPTEK_VIDEO_FMT_RGB888
    ohos_s65_draw_rgb888_direct(camera_buf,
                                camera_buf_hes,
                                camera_buf_ves,
                                camera_buf_hes * 3);
#else
    static uint32_t s_s64_unsupported_fmt_log_cnt = 0;
    if ((s_s64_unsupported_fmt_log_cnt++ % 60U) == 0U) {
        ESP_LOGW(TAG, "S64B unsupported OSPTEK_VIDEO_FMT=%u", (unsigned)OSPTEK_VIDEO_FMT);
    }
#endif
}

static uint32_t OhosCameraServiceStartOsptekPreview(void)
{
    if (s_s64_osptek_video_started) {
        ESP_LOGI(TAG, "S64B OSPTEK video already started fd=%d", s_s64_osptek_video_fd);
        return 0;
    }

    i2c_master_bus_handle_t shared_i2c = NULL;
    esp_err_t ret = OhosBoardI2cGetSharedBus(&shared_i2c);
    ESP_LOGI(TAG, "S64B get shared I2C ret=%s bus=%p", esp_err_to_name(ret), shared_i2c);
    if (ret != ESP_OK) {
        return 1;
    }

    ret = osptek_video_main(shared_i2c);
    ESP_LOGI(TAG, "S64B osptek_video_main ret=%s", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        return 2;
    }

    int fd = osptek_video_open(EXAMPLE_CAM_DEV_PATH, OSPTEK_VIDEO_FMT);
    ESP_LOGI(TAG, "S64B osptek_video_open fd=%d fmt=0x%08x", fd, (unsigned)OSPTEK_VIDEO_FMT);
    if (fd < 0) {
        return 3;
    }

    ret = osptek_video_set_bufs(fd, EXAMPLE_CAM_BUF_NUM, NULL);
    ESP_LOGI(TAG, "S64B osptek_video_set_bufs ret=%s", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        return 4;
    }

    ret = osptek_video_register_frame_operation_cb(ohos_s64_osptek_video_frame_cb);
    ESP_LOGI(TAG, "S64B osptek_video_register_frame_operation_cb ret=%s", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        return 5;
    }

    /*
     * Mark the stream fd before creating the video task. On LiteOS the new
     * task may preempt this starter immediately; Back must still see a valid fd.
     */
    s_s64_osptek_video_fd = fd;
    s_s64_osptek_video_started = 1;

    ret = osptek_video_stream_task_start(fd, 1, NULL);
    ESP_LOGI(TAG, "S64B osptek_video_stream_task_start ret=%s", esp_err_to_name(ret));
    if (ret != ESP_OK) {
        s_s64_osptek_video_started = 0;
        s_s64_osptek_video_fd = -1;
        return 6;
    }

    if (!s_s60_camera_page_active || !s_s63_direct_camera_enabled) {
        ESP_LOGI(TAG,
                 "S64B camera page already left during start, stop fresh fd=%d",
                 fd);
        OhosCameraServiceStopOsptekPreview();
    }

    return 0;
}

static void OhosCameraServiceStopOsptekPreview(void)
{
    if (!s_s64_osptek_video_started || s_s64_osptek_video_fd < 0) {
        ESP_LOGI(TAG,
                 "S67F OSPTEK video already stopped fd=%d started=%d",
                 s_s64_osptek_video_fd,
                 (int)s_s64_osptek_video_started);
        s_s64_osptek_video_started = 0;
        s_s64_osptek_video_fd = -1;
        return;
    }

    int fd = s_s64_osptek_video_fd;

    /*
     * S67F:
     * Do NOT close(fd) here.
     * CSI DMA/video task may still finish callbacks after stream_task_stop().
     */
    esp_err_t ret = osptek_video_stream_task_stop(fd);
    ESP_LOGI(TAG,
             "S67F osptek_video_stream_task_stop fd=%d ret=%s, keep fd unclosed",
             fd,
             esp_err_to_name(ret));

    s_s64_osptek_video_started = 0;

    /*
     * Force next Start Camera to open a fresh fd.
     * Intentionally do not close the old fd in this demo stage.
     */
    s_s64_osptek_video_fd = -1;
}













void OhosCameraPreviewShowRgb565Frame(const uint8_t *rgb565,
                                      uint32_t src_w,
                                      uint32_t src_h,
                                      uint32_t src_stride)
{

    if (s_s63_direct_camera_enabled) {
        ohos_s63_draw_rgb565_direct(rgb565, src_w, src_h, src_stride);
        return;
    }

    if (!s_s60_camera_page_active) {
        /* S60A: ignore camera frame while GIF page is active */
        return;
    }

    if (rgb565 == NULL || src_w == 0 || src_h == 0) {
        ESP_LOGW(TAG, "camera preview frame invalid");
        return;
    }

    if (s_camera_preview_async_pending) {
        ESP_LOGW(TAG, "camera preview async already pending, skip");
        return;
    }

    if (src_stride == 0) {
        src_stride = src_w * 2;
    }

    /*
     * S62B aspect-ratio preserving fill:
     * source 1280x720 -> target 480x800.
     * Do not squeeze the full source into portrait screen.
     * Crop the source center area first, then scale to 480x800.
     */
    uint32_t crop_x = 0;
    uint32_t crop_y = 0;
    uint32_t crop_w = src_w;
    uint32_t crop_h = src_h;

    uint64_t src_scaled = (uint64_t)src_w * OHOS_CAMERA_PREVIEW_H;
    uint64_t dst_scaled = (uint64_t)src_h * OHOS_CAMERA_PREVIEW_W;

    if (src_scaled > dst_scaled) {
        crop_w = (uint32_t)(((uint64_t)src_h * OHOS_CAMERA_PREVIEW_W) / OHOS_CAMERA_PREVIEW_H);
        crop_x = (src_w > crop_w) ? ((src_w - crop_w) / 2U) : 0;
    } else if (src_scaled < dst_scaled) {
        crop_h = (uint32_t)(((uint64_t)src_w * OHOS_CAMERA_PREVIEW_H) / OHOS_CAMERA_PREVIEW_W);
        crop_y = (src_h > crop_h) ? ((src_h - crop_h) / 2U) : 0;
    }

    static uint32_t s_s62b_crop_log_cnt = 0;
    if ((s_s62b_crop_log_cnt++ % 30U) == 0U) {
        ESP_LOGI(TAG,
                 "S62B camera crop src=%ux%u crop=(%u,%u %ux%u) dst=%ux%u byte_swap=%d rb_swap=%d",
                 (unsigned)src_w,
                 (unsigned)src_h,
                 (unsigned)crop_x,
                 (unsigned)crop_y,
                 (unsigned)crop_w,
                 (unsigned)crop_h,
                 OHOS_CAMERA_PREVIEW_W,
                 OHOS_CAMERA_PREVIEW_H,
                 OHOS_CAMERA_RGB565_BYTE_SWAP,
                 OHOS_CAMERA_RGB565_RB_SWAP);
    }

    if (s_camera_preview_buf == NULL) {
        s_camera_preview_buf = (uint8_t *)heap_caps_malloc(
            OHOS_CAMERA_PREVIEW_W * OHOS_CAMERA_PREVIEW_H * 2,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (s_camera_preview_buf == NULL) {
            ESP_LOGE(TAG, "camera preview buffer alloc failed");
            return;
        }
    }

    ESP_LOGI(TAG, "camera preview copy begin: src=%ux%u stride=%u preview=%ux%u",
             (unsigned)src_w,
             (unsigned)src_h,
             (unsigned)src_stride,
             OHOS_CAMERA_PREVIEW_W,
             OHOS_CAMERA_PREVIEW_H);

    for (uint32_t y = 0; y < OHOS_CAMERA_PREVIEW_H; y++) {
        uint32_t sy = crop_y + (uint32_t)(((uint64_t)y * crop_h) / OHOS_CAMERA_PREVIEW_H);
        const uint8_t *src_line = rgb565 + sy * src_stride;
        uint8_t *dst_line = s_camera_preview_buf + y * OHOS_CAMERA_PREVIEW_W * 2;

        for (uint32_t x = 0; x < OHOS_CAMERA_PREVIEW_W; x++) {
            uint32_t sx = crop_x + (uint32_t)(((uint64_t)x * crop_w) / OHOS_CAMERA_PREVIEW_W);
            const uint8_t *src_px = src_line + sx * 2;
            uint8_t *dst_px = dst_line + x * 2;
            /*
             * S62B:
             * LVGL RGB565 image byte order may differ from direct panel draw.
             * Try byte swap first for the green-tint issue.
             */
#if OHOS_CAMERA_RGB565_BYTE_SWAP
            uint16_t px = ((uint16_t)src_px[0] << 8) | src_px[1];
#else
            uint16_t px = ((uint16_t)src_px[1] << 8) | src_px[0];
#endif

#if OHOS_CAMERA_RGB565_RB_SWAP
            uint16_t r = (px >> 11) & 0x1f;
            uint16_t g = (px >> 5) & 0x3f;
            uint16_t b = px & 0x1f;
            px = (uint16_t)((b << 11) | (g << 5) | r);
#endif

            dst_px[0] = (uint8_t)(px & 0xff);
            dst_px[1] = (uint8_t)(px >> 8);
        }
    }

    ESP_LOGI(TAG, "camera preview copy done, schedule LVGL async");

    s_camera_preview_async_pending = 1;
    lv_async_call(camera_preview_async_cb, NULL);

    /*
     * Give LVGL task a chance to run the async callback.
     * Without this yield/wait, the camera capture loop can keep running,
     * and all later preview frames will be skipped because async_pending stays 1.
     */
    for (int wait_i = 0; wait_i < 20 && s_camera_preview_async_pending; wait_i++) {
        OhosLiteosDelayMs(5);
    }

    if (s_camera_preview_async_pending) {
        ESP_LOGW(TAG, "camera preview async still pending after wait");
    }
}





static void camera_preview_lv_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!s_camera_preview_frame_ready || s_camera_preview_buf == NULL) {
        return;
    }

    uint32_t frame_id = s_camera_preview_frame_id;
    if (frame_id == s_camera_preview_frame_drawn_id) {
        s_camera_preview_frame_ready = 0;
        return;
    }

    memset(&s_camera_preview_dsc, 0, sizeof(s_camera_preview_dsc));
#if LVGL_VERSION_MAJOR >= 9
    s_camera_preview_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
#endif
    s_camera_preview_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s_camera_preview_dsc.header.w = OHOS_CAMERA_PREVIEW_W;
    s_camera_preview_dsc.header.h = OHOS_CAMERA_PREVIEW_H;
    s_camera_preview_dsc.header.stride = OHOS_CAMERA_PREVIEW_W * 2;
    s_camera_preview_dsc.data_size = OHOS_CAMERA_PREVIEW_W * OHOS_CAMERA_PREVIEW_H * 2;
    s_camera_preview_dsc.data = s_camera_preview_buf;

    if (s_camera_preview_img == NULL) {
        ESP_LOGI(TAG, "camera preview timer: create LVGL image object");

        lv_obj_clean(lv_screen_active());
        s_spin_img = NULL;

        s_camera_preview_img = lv_image_create(lv_screen_active());
        lv_image_set_src(s_camera_preview_img, &s_camera_preview_dsc);
        lv_obj_center(s_camera_preview_img);

        /*
         * S62A:
         * The camera image is created after the Back button, so it can cover
         * the button. Re-create/bring the Back button to foreground.
         */
        ohos_s60_create_camera_back_button();
        if (s_camera_back_btn != NULL) {
            lv_obj_move_foreground(s_camera_back_btn);
        }
    } else {
        lv_image_set_src(s_camera_preview_img, &s_camera_preview_dsc);
        lv_obj_invalidate(s_camera_preview_img);
        if (s_camera_back_btn != NULL) {
            lv_obj_move_foreground(s_camera_back_btn);
        }
    }

    s_camera_preview_frame_drawn_id = frame_id;
    s_camera_preview_frame_ready = 0;

    ESP_LOGI(TAG, "Camera timer preview frame displayed id=%u preview=%ux%u",
             (unsigned)frame_id,
             OHOS_CAMERA_PREVIEW_W,
             OHOS_CAMERA_PREVIEW_H);
}

void OhosCameraPreviewStartLvTimer(void)
{
    if (s_camera_preview_timer == NULL) {
        s_camera_preview_timer = lv_timer_create(camera_preview_lv_timer_cb, 100, NULL);
        ESP_LOGI(TAG, "Camera preview LVGL timer created period=100ms");
    }
}


void OhosCameraPreviewShowRgb888Frame(const uint8_t *rgb888,
                                      uint32_t src_w,
                                      uint32_t src_h,
                                      uint32_t src_stride)
{
    if (!s_s60_camera_page_active) {
        /* S60A: ignore camera frame while GIF page is active */
        return;
    }

    if (rgb888 == NULL || src_w == 0 || src_h == 0) {
        ESP_LOGW(TAG, "camera RGB888 preview frame invalid");
        return;
    }

    if (s_camera_preview_frame_ready) {
        /* LVGL timer has not consumed the previous frame yet. Drop this one. */
        return;
    }

    if (src_stride == 0) {
        src_stride = src_w * 3;
    }

    if (s_camera_preview_buf == NULL) {
        s_camera_preview_buf = (uint8_t *)heap_caps_malloc(
            OHOS_CAMERA_PREVIEW_W * OHOS_CAMERA_PREVIEW_H * 2,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (s_camera_preview_buf == NULL) {
            ESP_LOGE(TAG, "camera RGB888 RBG preview buffer alloc failed");
            return;
        }
    }

    for (uint32_t y = 0; y < OHOS_CAMERA_PREVIEW_H; y++) {
        uint32_t sy = (y * src_h) / OHOS_CAMERA_PREVIEW_H;
        const uint8_t *src_line = rgb888 + sy * src_stride;
        uint8_t *dst_line = s_camera_preview_buf + y * OHOS_CAMERA_PREVIEW_W * 2;

        for (uint32_t x = 0; x < OHOS_CAMERA_PREVIEW_W; x++) {
            uint32_t sx = (x * src_w) / OHOS_CAMERA_PREVIEW_W;
            const uint8_t *src_px = src_line + sx * 3;
            uint8_t *dst_px = dst_line + x * 2;

            /*
             * RBG + software gain color tune v3.
             * Current issue after v2: image is too red.
             * Tune strategy: reduce red, restore green/blue slightly,
             * keep a small brightness lift.
             */
            int r0 = src_px[0];
            int g0 = src_px[2];
            int b0 = src_px[1];

            int r = (r0 * 135) / 100 + 14;
            int g = (g0 * 98) / 100 + 12;
            int b = (b0 * 88) / 100 + 12;

            if (r > 255) r = 255;
            if (g > 255) g = 255;
            if (b > 255) b = 255;
            if (r < 0) r = 0;
            if (g < 0) g = 0;
            if (b < 0) b = 0;

            uint16_t out = ((uint16_t)(r & 0xF8) << 8) |
                           ((uint16_t)(g & 0xFC) << 3) |
                           ((uint16_t)(b) >> 3);

            dst_px[0] = (uint8_t)(out & 0xff);
            dst_px[1] = (uint8_t)(out >> 8);
        }
    }

    s_camera_preview_frame_id++;
    s_camera_preview_frame_ready = 1;

    ESP_LOGI(TAG, "camera RGB888 RBG preview frame ready id=%u src=%ux%u preview=%ux%u",
             (unsigned)s_camera_preview_frame_id,
             (unsigned)src_w,
             (unsigned)src_h,
             OHOS_CAMERA_PREVIEW_W,
             OHOS_CAMERA_PREVIEW_H);
}
