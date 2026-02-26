/**
 * @file mpu6050.h
 * @brief MPU6050 accel + gyro. Senior design: config, result, data struct.
 */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>
#include "../common/sensor_result.h"

#define MPU6050_I2C_ADDR_AD0_LOW 0x68U
#define MPU6050_I2C_ADDR_AD0_HIGH 0x69U

typedef struct {
  uint8_t i2c_instance;
  uint8_t i2c_addr;
} mpu6050_config_t;

typedef struct {
  int16_t accel_x;
  int16_t accel_y;
  int16_t accel_z;
  int16_t gyro_x;
  int16_t gyro_y;
  int16_t gyro_z;
} mpu6050_data_t;

sensor_result_t mpu6050_init(const mpu6050_config_t *config);
sensor_result_t mpu6050_read(mpu6050_data_t *out);

#endif /* MPU6050_H */
