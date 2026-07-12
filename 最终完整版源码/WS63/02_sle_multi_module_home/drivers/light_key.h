#ifndef LIGHT_KEY_H
#define LIGHT_KEY_H

#include <stdbool.h>

void light_key_init(void);

/*
 * Returns true if a debounced key press was consumed. The caller is
 * responsible for cycling the light mode and reporting to the hub.
 */
bool light_key_consume_pressed(void);

#endif
