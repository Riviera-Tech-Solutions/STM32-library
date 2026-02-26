/**
 * @file bh1750.h
 * @brief BH1750 ambient light (I2C). Senior design: config, result, lux.
 */
#ifndef BH1750_H
#define BH1750_H

#include <stdint.h>
#include "../common/sensor_result.h"

#define BH1750_I2C_ADDR_LO 0x23U
#define BH1750_I2C_ADDR_HI 0x5CU

typedef struct {
  uint8_t i2c_instance;
  uint8_t i2c_addr;
} bh1750_config_t;

sensor_result_t bh1750_init(const bh1750_config_t *config);
sensor_result_t bh1750_read_lux(uint32_t *lux);

#endif /* BH1750_H */
