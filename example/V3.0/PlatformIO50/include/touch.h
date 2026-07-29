#pragma once

#include <stdint.h>

/** Initialize and probe the GT911 controller. */
void touch_init();
/** Read one mapped point; returns false when no finger is active. */
bool touch_read_point(int16_t & x, int16_t & y);
