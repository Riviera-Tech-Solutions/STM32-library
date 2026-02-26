/**
 * @file bme280.c
 * @brief BME280 – I2C read of calibration and data. Senior design: register map as defines.
 * Requires I2C driver (stm32_i2c). Write reg addr then read; two transactions.
 */
#include "bme280.h"
#include <stddef.h>
#include <stdbool.h>
#include "../../drivers/i2c/stm32_i2c.h"

#define BME280_REG_ID         0xD0U
#define BME280_REG_CTRL_HUM   0xF2U
#define BME280_REG_CTRL_MEAS  0xF4U
#define BME280_REG_PRESS      0xF7U
#define BME280_REG_TEMP       0xFAU
#define BME280_REG_HUM        0xFDU
#define BME280_CALIB_T1_START 0x88U
#define BME280_CALIB_T1_LEN   6
#define BME280_CALIB_P_START  0x8EU
#define BME280_CALIB_P_LEN    18
#define BME280_CALIB_H1       0xA1U
#define BME280_RAW_LEN        8

static bme280_config_t s_config;
static bool s_initialized;

/* Minimal compensation: raw temp to C x 100 (simplified; full formula uses calib T1..T3) */
static int32_t compensate_temp(uint32_t raw) {
  (void)raw;
  /* Placeholder: real implementation uses calib coeffs. Return raw as approximate. */
  return (int32_t)((raw * 5) / 16 - 2500); /* rough: raw is 20-bit, 0.01 C per LSB */
}

sensor_result_t bme280_init(const bme280_config_t *config) {
  if (config == NULL) return SENSOR_ERR_PARAM;
  s_config = *config;
  i2c_instance_t inst = (i2c_instance_t)s_config.i2c_instance;
  if (!i2c_init(inst, 100000)) return SENSOR_ERR_BUS;
  uint8_t id;
  uint8_t reg = BME280_REG_ID;
  if (!i2c_write(inst, s_config.i2c_addr, &reg, 1)) return SENSOR_ERR_BUS;
  if (!i2c_read(inst, s_config.i2c_addr, &id, 1)) return SENSOR_ERR_BUS;
  if (id != 0x60U) return SENSOR_ERR_BUS;
  uint8_t ctrl[] = { BME280_REG_CTRL_HUM, 0x01 };
  if (!i2c_write(inst, s_config.i2c_addr, ctrl, 2)) return SENSOR_ERR_BUS;
  uint8_t meas[] = { BME280_REG_CTRL_MEAS, 0xB7 };
  if (!i2c_write(inst, s_config.i2c_addr, meas, 2)) return SENSOR_ERR_BUS;
  s_initialized = true;
  return SENSOR_OK;
}

sensor_result_t bme280_read(bme280_data_t *out) {
  if (out == NULL || !s_initialized) return SENSOR_ERR_PARAM;
  i2c_instance_t inst = (i2c_instance_t)s_config.i2c_instance;
  uint8_t reg = BME280_REG_PRESS;
  if (!i2c_write(inst, s_config.i2c_addr, &reg, 1)) return SENSOR_ERR_BUS;
  uint8_t raw[BME280_RAW_LEN];
  if (!i2c_read(inst, s_config.i2c_addr, raw, BME280_RAW_LEN)) return SENSOR_ERR_BUS;
  uint32_t p_raw = (uint32_t)raw[0] << 12 | (uint32_t)raw[1] << 4 | (raw[2] >> 4);
  uint32_t t_raw = (uint32_t)raw[3] << 12 | (uint32_t)raw[4] << 4 | (raw[5] >> 4);
  uint16_t h_raw = (uint16_t)raw[6] << 8 | raw[7];
  out->pressure_pa = (uint32_t)(p_raw * 4); /* simplified; full uses calib */
  out->temp_c_x100 = compensate_temp(t_raw);
  out->humidity_x1024 = (uint32_t)h_raw * 1024 / 1024; /* simplified */
  return SENSOR_OK;
}
