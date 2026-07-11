#ifndef FAN_GPIO_H
#define FAN_GPIO_H

#include <stdbool.h>
#include <stdint.h>

void fan_gpio_init(void);
void fan_gpio_set_level(uint8_t level);
uint8_t fan_gpio_get_level(void);
bool fan_gpio_get_on(void);
void fan_gpio_cycle(void);
void fan_gpio_sync_from_output(void);

/*
 * Advance the soft-PWM by one step. Must be called periodically (~10 ms)
 * from the fan module main loop to keep the fan spinning at the set level.
 */
void fan_gpio_pwm_tick(void);

#endif
