#ifndef XH_FAN_CONTROL_H
#define XH_FAN_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    XH_FAN_TARGET_LOCAL_GPIO = 0,
    XH_FAN_TARGET_SLE_MODULE = 1,
} xh_fan_target_t;

void xh_fan_control_init(xh_fan_target_t target);
bool xh_fan_control_set_level(uint8_t level);
uint8_t xh_fan_control_get_level(void);

#endif

