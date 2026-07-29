#pragma once

#include <stdint.h>

/** Initialize the DHT20 sensor. */
bool dht20_init();
/** Read integer temperature and humidity values. */
bool dht20_read(int & temperature, int & humidity);
