/**
 * @file bh1750.c
 * @brief BH1750 – I2C write power on + mode, read 2 bytes; lux = (raw * 10) / 12.
 */
#include "bh1750.h"
#include <stddef.h>
#include <stdbool.h>
#include "../../drivers/i2c/stm32_i2c.h"

#define BH1750_CMD_POWER_ON  0x01U
#define BH1750_CMD_CONT_H    0x10U
#define BH1750_CMD_ONE_H     0x20U
#define BH1750_READ_LEN      2

static bh1750_config_t s_config;
static bool s_initialized;

sensor_result_t bh1750_init(const bh1750_config_t *config) {
  if (config == NULL) return SENSOR_ERR_PARAM;
  s_config = *config;
  i2c_instance_t inst = (i2c_instance_t)s_config.i2c_instance;
  if (!i2c_init(inst, 100000)) return SENSOR_ERR_BUS;
  uint8_t cmd[] = { BH1750_CMD_POWER_ON };
  if (!i2c_write(inst, s_config.i2c_addr, cmd, 1)) return SENSOR_ERR_BUS;
  cmd[0] = BH1750_CMD_CONT_H;
  if (!i2c_write(inst, s_config.i2c_addr, cmd, 1)) return SENSOR_ERR_BUS;
  s_initialized = true;
  return SENSOR_OK;
}

sensor_result_t bh1750_read_lux(uint32_t *lux) {
  if (lux == NULL || !s_initialized) return SENSOR_ERR_PARAM;
  i2c_instance_t inst = (i2c_instance_t)s_config.i2c_instance;
  uint8_t buf[BH1750_READ_LEN];
  if (!i2c_read(inst, s_config.i2c_addr, buf, BH1750_READ_LEN)) return SENSOR_ERR_BUS;
  uint16_t raw = (uint16_t)buf[0] << 8 | buf[1];
  *lux = ((uint32_t)raw * 10U) / 12U;
  return SENSOR_OK;
}
