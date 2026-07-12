/*
 * Light key driver — GPIO interrupt + debounce, modelled on fan_key.c.
 *
 * The key is wired to XH_LIGHT_KEY_PIN (default GPIO5, active-low with
 * internal pull-up). On a falling edge the ISR sets a flag; the main loop
 * calls light_key_consume_pressed() which debounces and returns true if the
 * press is real. The caller then cycles the light mode.
 */

#include "light_key.h"

#include <stdio.h>

#include "common_def.h"
#include "gpio.h"
#include "pinctrl.h"
#include "soc_osal.h"

#ifndef XH_LIGHT_KEY_PIN
#define XH_LIGHT_KEY_PIN 5
#endif

#ifndef XH_LIGHT_KEY_MODE
#define XH_LIGHT_KEY_MODE 4
#endif

#ifndef XH_LIGHT_KEY_DEBOUNCE_MS
#define XH_LIGHT_KEY_DEBOUNCE_MS 30
#endif

static bool g_light_key_inited;
static volatile bool g_light_key_irq_event;
static volatile bool g_light_key_wait_release;
static volatile bool g_light_key_enabled;

static void light_key_isr(void)
{
    if (!g_light_key_enabled || g_light_key_wait_release) {
        return;
    }
    g_light_key_irq_event = true;
}

static bool light_key_check_after_debounce(const char *event_source)
{
    g_light_key_wait_release = true;
    g_light_key_irq_event = false;

    osal_msleep(XH_LIGHT_KEY_DEBOUNCE_MS);
    gpio_level_t level = uapi_gpio_get_val((pin_t)XH_LIGHT_KEY_PIN);
    if (level != GPIO_LEVEL_LOW) {
        g_light_key_wait_release = false;
        (void)uapi_gpio_clear_interrupt((pin_t)XH_LIGHT_KEY_PIN);
        printf("[light_key] %s ignored level=%u\r\n", event_source, (unsigned int)level);
        return false;
    }
    return true;
}

void light_key_init(void)
{
    if (g_light_key_inited) {
        return;
    }

    g_light_key_enabled = false;
    g_light_key_irq_event = false;
    g_light_key_wait_release = false;

    uapi_pin_set_mode((pin_t)XH_LIGHT_KEY_PIN, (pin_mode_t)XH_LIGHT_KEY_MODE);
    uapi_gpio_set_dir((pin_t)XH_LIGHT_KEY_PIN, GPIO_DIRECTION_INPUT);
    uapi_gpio_set_val((pin_t)XH_LIGHT_KEY_PIN, GPIO_LEVEL_HIGH);
    gpio_level_t level = uapi_gpio_get_val((pin_t)XH_LIGHT_KEY_PIN);
    errcode_t ret = uapi_gpio_register_isr_func((pin_t)XH_LIGHT_KEY_PIN,
        GPIO_INTERRUPT_FALLING_EDGE, light_key_isr);
    (void)uapi_gpio_clear_interrupt((pin_t)XH_LIGHT_KEY_PIN);

    g_light_key_wait_release = (level == GPIO_LEVEL_LOW);
    g_light_key_enabled = true;
    g_light_key_inited = true;
    printf("[light_key] init pin=%d mode=%d irq=falling ret=0x%x level=%u wait_release=%u\r\n",
        XH_LIGHT_KEY_PIN, XH_LIGHT_KEY_MODE, (unsigned int)ret,
        (unsigned int)level, (unsigned int)g_light_key_wait_release);
}

bool light_key_consume_pressed(void)
{
    if (!g_light_key_inited) {
        return false;
    }

    gpio_level_t level = uapi_gpio_get_val((pin_t)XH_LIGHT_KEY_PIN);

    /* If we are waiting for the key to be released, check for release. */
    if (g_light_key_wait_release) {
        if (level == GPIO_LEVEL_HIGH) {
            g_light_key_wait_release = false;
            g_light_key_irq_event = false;
            (void)uapi_gpio_clear_interrupt((pin_t)XH_LIGHT_KEY_PIN);
            printf("[light_key] released\r\n");
        }
        return false;
    }

    /* ISR-triggered path */
    if (g_light_key_irq_event) {
        g_light_key_irq_event = false;
        if (light_key_check_after_debounce("irq")) {
            return true;
        }
        return false;
    }

    /* Polling fallback: key held low without ISR edge */
    if (level != GPIO_LEVEL_LOW) {
        return false;
    }

    return light_key_check_after_debounce("poll");
}
