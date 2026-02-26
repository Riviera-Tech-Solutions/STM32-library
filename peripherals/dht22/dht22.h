/**
 * @file dht22.h
 * @brief DHT22 (AM2302) temp + humidity. Senior design: config struct, result with data.
 */
#ifndef DHT22_H
#define DHT22_H

#include <stdint.h>
#include "../common/sensor_result.h"

#ifndef DHT22_TIMEOUT_US
#define DHT22_TIMEOUT_US 1000
#endif

typedef struct {
  uint8_t gpio_port;
  uint8_t gpio_pin;
} dht22_config_t;

typedef struct {
  int16_t temp_c_x10;   /* temperature * 10 (e.g. 235 = 23.5 C) */
  uint16_t humidity_x10; /* humidity * 10 (e.g. 652 = 65.2 %) */
} dht22_data_t;

sensor_result_t dht22_init(const dht22_config_t *config);
sensor_result_t dht22_read(dht22_data_t *out);

#endif /* DHT22_H */
