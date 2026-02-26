/**
 * @file ds18b20.h
 * @brief DS18B20 1-Wire temperature. Senior design: config struct, result + value.
 */
#ifndef DS18B20_H
#define DS18B20_H

#include <stdint.h>
#include "../common/sensor_result.h"

#ifndef DS18B20_TIMEOUT_US
#define DS18B20_TIMEOUT_US 750
#endif

typedef struct {
  uint8_t gpio_port;
  uint8_t gpio_pin;
} ds18b20_config_t;

sensor_result_t ds18b20_init(const ds18b20_config_t *config);
sensor_result_t ds18b20_read_temp_c_x16(int16_t *temp_c_x16);  /* temp * 16, e.g. 2350 = 23.5 C */

#endif /* DS18B20_H */
