#ifndef FAN_KEY_H
#define FAN_KEY_H

#include <stdbool.h>
#include <stdint.h>

void fan_key_init(void);
bool fan_key_consume_cycled(uint8_t *fan_level, const char **source);

#endif
