/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lv_adapter_display.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* S58A: fixed final board display/touch configuration */
#define HW_LCD_H_RES    480
#define HW_LCD_V_RES    800
#define HW_USE_TOUCH    1
#define HW_USE_ENCODER  0


esp_err_t hw_lcd_init(esp_lcd_panel_handle_t *panel_handle,
                      esp_lcd_panel_io_handle_t *io_handle,
                      esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode,
                      esp_lv_adapter_rotation_t rotation);

#if HW_USE_TOUCH
#include "esp_lcd_touch.h"

esp_err_t hw_touch_init(esp_lcd_touch_handle_t *touch_handle,
                        esp_lv_adapter_rotation_t rotation);

#endif

#if HW_USE_ENCODER
#include "iot_knob.h"
#include "iot_button.h"

const knob_config_t *hw_knob_get_config(void);
button_handle_t hw_knob_get_button(void);

#endif

#ifdef __cplusplus
}
#endif
