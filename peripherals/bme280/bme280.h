/**
 * @file bme280.h
 * @brief BME280 temp, humidity, pressure. Senior design: config, result, data struct.
 */
#ifndef BME280_H
#define BME280_H

#include <stdint.h>
#include "../common/sensor_result.h"

#define BME280_I2C_ADDR_PRIM 0x76U
#define BME280_I2C_ADDR_SEC  0x77U

typedef struct {
  uint8_t i2c_instance;
  uint8_t i2c_addr;
} bme280_config_t;

typedef struct {
  int32_t temp_c_x100;    /* e.g. 2350 = 23.50 C */
  uint32_t humidity_x1024; /* e.g. 65300 = 63.77 % */
  uint32_t pressure_pa;    /* Pascals */
} bme280_data_t;

sensor_result_t bme280_init(const bme280_config_t *config);
sensor_result_t bme280_read(bme280_data_t *out);

#endif /* BME280_H */
