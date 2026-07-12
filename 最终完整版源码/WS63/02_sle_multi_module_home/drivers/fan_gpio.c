#include "fan_gpio.h"

#include <stdio.h>

#include "common_def.h"
#include "gpio.h"
#include "pinctrl.h"

#ifndef XH_FAN_GPIO_PIN
#define XH_FAN_GPIO_PIN 9
#endif

#ifndef XH_FAN_GPIO_MODE
#define XH_FAN_GPIO_MODE 0
#endif

/*
 * Single-pin soft-PWM fan driver (ported from 07_sle_smart_fan_phone_oled).
 *
 * The fan hardware is driven by a single GPIO pin (XH_FAN_GPIO_PIN, default 9).
 * Speed levels 1/2/3 are distinguished by software PWM duty cycle, NOT by
 * multiple pins. The previous multi-pin mutual-exclusion design sent 2/3
 * level signals to GPIO10/GPIO11 which were not wired to the fan, so levels
 * 2 and 3 did not spin the fan.
 *
 * PWM: 20 steps per period, advanced once per fan_gpio_pwm_tick() call.
 *   level 1 (low)  -> 6/20  = 30% duty
 *   level 2 (mid)  -> 12/20 = 60% duty
 *   level 3 (high) -> 20/20 = 100% duty
 *   level 0 (off)  -> pin low
 *
 * The main loop must call fan_gpio_pwm_tick() periodically (~10 ms) to keep
 * the PWM running.
 */
#ifndef XH_FAN_PWM_STEPS
#define XH_FAN_PWM_STEPS 20U
#endif
#ifndef XH_FAN_PWM_THR_LOW
#define XH_FAN_PWM_THR_LOW  6U   /* level 1 duty threshold */
#endif
#ifndef XH_FAN_PWM_THR_MID
#define XH_FAN_PWM_THR_MID  12U  /* level 2 duty threshold */
#endif
/* level 3 uses full duty (XH_FAN_PWM_STEPS) */

#define XH_FAN_LEVEL_OFF  0U
#define XH_FAN_LEVEL_LOW  1U
#define XH_FAN_LEVEL_MID  2U
#define XH_FAN_LEVEL_HIGH 3U

static bool g_fan_gpio_inited;
static uint8_t g_fan_level;
static uint8_t g_fan_pwm_idx;

static void fan_gpio_drive(bool energized)
{
    (void)uapi_gpio_set_val((pin_t)XH_FAN_GPIO_PIN,
                            energized ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

void fan_gpio_init(void)
{
    if (g_fan_gpio_inited) {
        return;
    }

    (void)uapi_pin_set_mode((pin_t)XH_FAN_GPIO_PIN, (pin_mode_t)XH_FAN_GPIO_MODE);
    (void)uapi_pin_set_ds((pin_t)XH_FAN_GPIO_PIN, PIN_DS_7);
    (void)uapi_gpio_set_dir((pin_t)XH_FAN_GPIO_PIN, GPIO_DIRECTION_OUTPUT);
    fan_gpio_drive(false);

    g_fan_level = XH_FAN_LEVEL_OFF;
    g_fan_pwm_idx = 0U;
    g_fan_gpio_inited = true;
    printf("[fan_gpio] init pin=%d mode=%d soft-pwm steps=%u thr<%u/%u/%u\r\n",
        XH_FAN_GPIO_PIN, XH_FAN_GPIO_MODE,
        (unsigned int)XH_FAN_PWM_STEPS,
        (unsigned int)XH_FAN_PWM_THR_LOW,
        (unsigned int)XH_FAN_PWM_THR_MID,
        (unsigned int)XH_FAN_PWM_STEPS);
}

static uint8_t fan_gpio_pwm_threshold(uint8_t level)
{
    if (level <= XH_FAN_LEVEL_LOW) {
        return XH_FAN_PWM_THR_LOW;
    } else if (level == XH_FAN_LEVEL_MID) {
        return XH_FAN_PWM_THR_MID;
    } else {
        return XH_FAN_PWM_STEPS;
    }
}

void fan_gpio_pwm_tick(void)
{
    if (!g_fan_gpio_inited) {
        return;
    }

    if (g_fan_level == XH_FAN_LEVEL_OFF) {
        fan_gpio_drive(false);
        g_fan_pwm_idx = 0U;
        return;
    }

    uint8_t thr = fan_gpio_pwm_threshold(g_fan_level);
    fan_gpio_drive(g_fan_pwm_idx < thr);
    g_fan_pwm_idx++;
    if (g_fan_pwm_idx >= XH_FAN_PWM_STEPS) {
        g_fan_pwm_idx = 0U;
    }
}

void fan_gpio_set_level(uint8_t level)
{
    if (!g_fan_gpio_inited) {
        fan_gpio_init();
    }

    if (level > XH_FAN_LEVEL_HIGH) {
        level = XH_FAN_LEVEL_HIGH;
    }

    g_fan_level = level;
    g_fan_pwm_idx = 0U;

    /* Turn the pin off immediately when stopping; PWM tick keeps it running
     * for non-zero levels. */
    if (level == XH_FAN_LEVEL_OFF) {
        fan_gpio_drive(false);
    }

    printf("[fan_gpio] set level=%u\r\n", level);
}

uint8_t fan_gpio_get_level(void)
{
    return g_fan_level;
}

bool fan_gpio_get_on(void)
{
    return g_fan_level != XH_FAN_LEVEL_OFF;
}

void fan_gpio_sync_from_output(void)
{
    /* With soft PWM the pin toggles rapidly, so the level cannot be inferred
     * from the instantaneous pin value. Tracking is done via g_fan_level. */
    if (!g_fan_gpio_inited) {
        fan_gpio_init();
    }
}

void fan_gpio_cycle(void)
{
    if (!g_fan_gpio_inited) {
        fan_gpio_init();
    }
    uint8_t next = g_fan_level + 1;
    if (next > XH_FAN_LEVEL_HIGH) {
        next = XH_FAN_LEVEL_OFF;
    }
    printf("[fan_gpio] cycle %u->%u\r\n", g_fan_level, next);
    fan_gpio_set_level(next);
}
