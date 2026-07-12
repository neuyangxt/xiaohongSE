/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

static void example_show_gif_overlay(void);
void OhosCameraPreviewStartLvTimer(void);



extern int ohos_liteos_probe_only(void);
extern int ohos_liteos_kernelinit_no_tick_probe(void);
extern int ohos_liteos_bringup(bool start_scheduler);

static volatile int g_board_display_on = 0;
static volatile int g_board_display_h_res = 0;
static volatile int g_board_display_v_res = 0;
static volatile int g_board_display_lanes = 0;
static volatile int g_board_display_dpi_clk_mhz = 0;
static volatile int g_board_display_bpp = 0;

void board_display_mark_on(int h_res, int v_res, int lanes, int dpi_clk_mhz, int bpp)
{
    g_board_display_h_res = h_res;
    g_board_display_v_res = v_res;
    g_board_display_lanes = lanes;
    g_board_display_dpi_clk_mhz = dpi_clk_mhz;
    g_board_display_bpp = bpp;
    g_board_display_on = 1;
}

int board_display_get_info(int *on, int *h_res, int *v_res, int *lanes, int *dpi_clk_mhz, int *bpp)
{
    if (on) {
        *on = g_board_display_on;
    }
    if (h_res) {
        *h_res = g_board_display_h_res;
    }
    if (v_res) {
        *v_res = g_board_display_v_res;
    }
    if (lanes) {
        *lanes = g_board_display_lanes;
    }
    if (dpi_clk_mhz) {
        *dpi_clk_mhz = g_board_display_dpi_clk_mhz;
    }
    if (bpp) {
        *bpp = g_board_display_bpp;
    }
    return 0;
}


extern int ohos_liteos_bringup(bool start_scheduler);
#include "esp_lcd_ili9881c.h"
#include "esp_lcd_ek79007.h"
#include "st7102_ek79007_init_cmds.h"
#include "esp_flash_encrypt.h"

static const char *TAG = "example";


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
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1000 / CONFIG_FREERTOS_HZ

// LVGL library is not thread-safe, this example will call LVGL APIs from different tasks, so use a mutex to protect it
static _lock_t lvgl_api_lock;

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

static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t time_till_next_ms = 0;
    while (1) {
        _lock_acquire(&lvgl_api_lock);
        time_till_next_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);
        // in case of triggering a task watch dog time out
        time_till_next_ms = MAX(time_till_next_ms, EXAMPLE_LVGL_TASK_MIN_DELAY_MS);
        // in case of lvgl display not ready yet
        time_till_next_ms = MIN(time_till_next_ms, EXAMPLE_LVGL_TASK_MAX_DELAY_MS);
        usleep(1000 * time_till_next_ms);
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

    // create MIPI DSI bus first, it will initialize the DSI PHY as well
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = EXAMPLE_MIPI_DSI_LANE_NUM,
        .lane_bit_rate_mbps = EXAMPLE_MIPI_DSI_LANE_BITRATE_MBPS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install MIPI DSI LCD control IO");
    esp_lcd_panel_io_handle_t mipi_dbi_io;
    // we use DBI interface to send LCD commands and parameters
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,   // according to the LCD spec
        .lcd_param_bits = 8, // according to the LCD spec
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

    ESP_LOGI(TAG, "Install MIPI DSI LCD data panel");
    esp_lcd_panel_handle_t mipi_dpi_panel = NULL;
    esp_lcd_dpi_panel_config_t dpi_config = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = EXAMPLE_MIPI_DSI_DPI_CLK_MHZ,
        .in_color_format = LCD_COLOR_FMT_RGB888,
        .video_timing = {
            .h_size = EXAMPLE_MIPI_DSI_LCD_H_RES,
            .v_size = EXAMPLE_MIPI_DSI_LCD_V_RES,
            .hsync_back_porch = EXAMPLE_MIPI_DSI_LCD_HBP,
            .hsync_pulse_width = EXAMPLE_MIPI_DSI_LCD_HSYNC,
            .hsync_front_porch = EXAMPLE_MIPI_DSI_LCD_HFP,
            .vsync_back_porch = EXAMPLE_MIPI_DSI_LCD_VBP,
            .vsync_pulse_width = EXAMPLE_MIPI_DSI_LCD_VSYNC,
            .vsync_front_porch = EXAMPLE_MIPI_DSI_LCD_VFP,
        },
#if CONFIG_EXAMPLE_USE_DMA2D_COPY_FRAME
        .flags.use_dma2d = true, // use DMA2D to copy draw buffer into frame buffer
#endif
    };

// Use EK79007 component as a generic MIPI DSI/DPI wrapper,
// but send ST7102 vendor initialization commands.
    ek79007_vendor_config_t vendor_config = {
        .init_cmds = st7102_lcd_init_cmds,
        .init_cmds_size = st7102_lcd_init_cmds_size,
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
            .lane_num = EXAMPLE_MIPI_DSI_LANE_NUM,
        },
    };
    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 24,
        .vendor_config = &vendor_config,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(mipi_dbi_io, &lcd_dev_config, &mipi_dpi_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(mipi_dpi_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(mipi_dpi_panel));

    ESP_LOGI(TAG, "ST7102 manual send 0x29 Display ON after DPI init");
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(mipi_dbi_io, 0x29, NULL, 0));
    ESP_LOGI(TAG, "ST7102 manual send 0x29 done");
    vTaskDelay(pdMS_TO_TICKS(120));

    // turn on backlight
    example_bsp_set_lcd_backlight(EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // create a lvgl display
    lv_display_t *display = lv_display_create(EXAMPLE_MIPI_DSI_LCD_H_RES, EXAMPLE_MIPI_DSI_LCD_V_RES);
    // associate the mipi panel handle to the display
    lv_display_set_user_data(display, mipi_dpi_panel);
    // set color depth
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
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
    size_t draw_buffer_sz = EXAMPLE_MIPI_DSI_LCD_H_RES * EXAMPLE_LVGL_DRAW_BUF_LINES * sizeof(lv_color_t);

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

    ESP_LOGI(TAG, "Use esp_timer as LVGL tick timer");
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Display LVGL Meter Widget");
    board_display_mark_on(EXAMPLE_MIPI_DSI_LCD_H_RES,
                          EXAMPLE_MIPI_DSI_LCD_V_RES,
                          EXAMPLE_MIPI_DSI_LANE_NUM,
                          EXAMPLE_MIPI_DSI_DPI_CLK_MHZ,
                          24);
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
    _lock_acquire(&lvgl_api_lock);
    // example_lvgl_demo_ui(display);  // disabled for frame animation test
    example_show_gif_overlay();
    ESP_LOGI(TAG, "Display LVGL frame animation test");
    ESP_LOGI(TAG, "Display real HW init done; camera preview will be started by CameraService");
    _lock_release(&lvgl_api_lock);


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

#define OHOS_CAMERA_PREVIEW_W 320
#define OHOS_CAMERA_PREVIEW_H 180

static lv_obj_t *s_camera_preview_img = NULL;
static uint8_t *s_camera_preview_buf = NULL;
static lv_image_dsc_t s_camera_preview_dsc;
static lv_timer_t *s_camera_preview_timer = NULL;
static volatile int s_camera_preview_frame_ready = 0;
static volatile uint32_t s_camera_preview_frame_id = 0;
static uint32_t s_camera_preview_frame_drawn_id = 0;


static volatile int s_camera_preview_async_pending = 0;

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
    } else {
        /*
         * Reuse the existing image object. The pixel buffer content has been
         * updated before this async callback, so only invalidate the image.
         */
        lv_image_set_src(s_camera_preview_img, &s_camera_preview_dsc);
        lv_obj_invalidate(s_camera_preview_img);
    }

    s_camera_preview_async_pending = 0;
}


void OhosCameraPreviewShowRgb565Frame(const uint8_t *rgb565,
                                      uint32_t src_w,
                                      uint32_t src_h,
                                      uint32_t src_stride)
{
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
        uint32_t sy = (y * src_h) / OHOS_CAMERA_PREVIEW_H;
        const uint8_t *src_line = rgb565 + sy * src_stride;
        uint8_t *dst_line = s_camera_preview_buf + y * OHOS_CAMERA_PREVIEW_W * 2;

        for (uint32_t x = 0; x < OHOS_CAMERA_PREVIEW_W; x++) {
            uint32_t sx = (x * src_w) / OHOS_CAMERA_PREVIEW_W;
            const uint8_t *src_px = src_line + sx * 2;
            uint8_t *dst_px = dst_line + x * 2;
            // RGB565 little-endian, swap R/B channels test
            uint16_t px = ((uint16_t)src_px[1] << 8) | src_px[0];
            uint16_t r = (px >> 11) & 0x1f;
            uint16_t g = (px >> 5) & 0x3f;
            uint16_t b = px & 0x1f;
            uint16_t out = (b << 11) | (g << 5) | r;
            dst_px[0] = (uint8_t)(out & 0xff);
            dst_px[1] = (uint8_t)(out >> 8);
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
        vTaskDelay(pdMS_TO_TICKS(5));
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
    } else {
        lv_image_set_src(s_camera_preview_img, &s_camera_preview_dsc);
        lv_obj_invalidate(s_camera_preview_img);
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



