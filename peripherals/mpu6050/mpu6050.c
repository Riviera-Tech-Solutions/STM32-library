/**
 * @file mpu6050.c
 * @brief MPU6050 – I2C read of ACCEL_* and GYRO_* registers. Senior design: register map as defines.
 */
#include "mpu6050.h"
#include <stddef.h>
#include <stdbool.h>
#include "../../drivers/i2c/stm32_i2c.h"

#define MPU6050_REG_WHO_AM_I    0x75U
#define MPU6050_REG_PWR_MGMT_1  0x6BU
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
#define MPU6050_READ_LEN        14

static mpu6050_config_t s_config;
static bool s_initialized;

static int16_t be16(const uint8_t *p) {
  return (int16_t)((uint16_t)p[0] << 8 | p[1]);
}

sensor_result_t mpu6050_init(const mpu6050_config_t *config) {
  if (config == NULL) return SENSOR_ERR_PARAM;
  s_config = *config;
  i2c_instance_t inst = (i2c_instance_t)s_config.i2c_instance;
  if (!i2c_init(inst, 400000)) return SENSOR_ERR_BUS;
  uint8_t reg = MPU6050_REG_WHO_AM_I;
  uint8_t id;
  if (!i2c_write(inst, s_config.i2c_addr, &reg, 1)) return SENSOR_ERR_BUS;
  if (!i2c_read(inst, s_config.i2c_addr, &id, 1)) return SENSOR_ERR_BUS;
  if (id != 0x68U) return SENSOR_ERR_BUS;
  uint8_t wake[] = { MPU6050_REG_PWR_MGMT_1, 0x00 };
  if (!i2c_write(inst, s_config.i2c_addr, wake, 2)) return SENSOR_ERR_BUS;
  s_initialized = true;
  return SENSOR_OK;
}

sensor_result_t mpu6050_read(mpu6050_data_t *out) {
  if (out == NULL || !s_initialized) return SENSOR_ERR_PARAM;
  i2c_instance_t inst = (i2c_instance_t)s_config.i2c_instance;
  uint8_t reg = MPU6050_REG_ACCEL_XOUT_H;
  if (!i2c_write(inst, s_config.i2c_addr, &reg, 1)) return SENSOR_ERR_BUS;
  uint8_t buf[MPU6050_READ_LEN];
  if (!i2c_read(inst, s_config.i2c_addr, buf, MPU6050_READ_LEN)) return SENSOR_ERR_BUS;
  out->accel_x = be16(&buf[0]);
  out->accel_y = be16(&buf[2]);
  out->accel_z = be16(&buf[4]);
  out->gyro_x  = be16(&buf[8]);
  out->gyro_y  = be16(&buf[10]);
  out->gyro_z  = be16(&buf[12]);
  return SENSOR_OK;
}
