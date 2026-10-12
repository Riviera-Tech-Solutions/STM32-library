/**
 * @file bme280.c
 * @brief Bosch BME280 temperature/pressure/humidity over I2C.
 *
 * Reads the factory calibration coefficients once at init and applies the
 * datasheet fixed-point compensation on every read. All math is 32/64-bit
 * integer (no floating point), so it runs identically on the no-FPU cores
 * (M0/M0+/M3) and the FPU cores. Outputs:
 *   temp_c_x100    : degrees C * 100        (2350 = 23.50 C)
 *   pressure_pa    : Pascals
 *   humidity_x1024 : %RH * 1024             (65300 ~ 63.77 %RH)
 *
 * Single-instance (one BME280) — a second sensor of the same type needs a
 * per-instance calibration store, tracked separately.
 */
#include "bme280.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "../../drivers/i2c/stm32_i2c.h"

#define BME280_REG_ID         0xD0U
#define BME280_REG_RESET      0xE0U
#define BME280_REG_CTRL_HUM   0xF2U
#define BME280_REG_CTRL_MEAS  0xF4U
#define BME280_REG_CONFIG     0xF5U
#define BME280_REG_PRESS      0xF7U

#define BME280_CHIP_ID        0x60U

/* Calibration register blocks. */
#define BME280_CALIB_TP       0x88U   /* dig_T1..T3, dig_P1..P9 : 26 bytes -> 0x88..0xA1 */
#define BME280_CALIB_TP_LEN   26
#define BME280_CALIB_H2       0xE1U   /* dig_H2..H6 : 7 bytes */
#define BME280_CALIB_H2_LEN   7
#define BME280_RAW_LEN        8

/* Factory calibration coefficients (see BME280 datasheet, Table 16). */
typedef struct {
  uint16_t T1; int16_t T2, T3;
  uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9;
  uint8_t  H1; int16_t H2; uint8_t H3; int16_t H4, H5; int8_t H6;
} bme280_calib_t;

static bme280_config_t s_config;
static bme280_calib_t  s_cal;
static int32_t         s_t_fine;      /* carries temperature into P and H comp */
static bool            s_initialized;

static bool read_regs(uint8_t reg, uint8_t *buf, uint16_t len) {
  i2c_instance_t inst = (i2c_instance_t)s_config.i2c_instance;
  if (!i2c_write(inst, s_config.i2c_addr, &reg, 1)) return false;
  return i2c_read(inst, s_config.i2c_addr, buf, len);
}

static bool write_reg(uint8_t reg, uint8_t val) {
  uint8_t b[2] = { reg, val };
  return i2c_write((i2c_instance_t)s_config.i2c_instance, s_config.i2c_addr, b, 2);
}

static bool read_calibration(void) {
  uint8_t tp[BME280_CALIB_TP_LEN];
  uint8_t h[BME280_CALIB_H2_LEN];
  if (!read_regs(BME280_CALIB_TP, tp, BME280_CALIB_TP_LEN)) return false;
  if (!read_regs(BME280_CALIB_H2, h, BME280_CALIB_H2_LEN)) return false;

  s_cal.T1 = (uint16_t)(tp[0] | (tp[1] << 8));
  s_cal.T2 = (int16_t)(tp[2] | (tp[3] << 8));
  s_cal.T3 = (int16_t)(tp[4] | (tp[5] << 8));
  s_cal.P1 = (uint16_t)(tp[6] | (tp[7] << 8));
  s_cal.P2 = (int16_t)(tp[8] | (tp[9] << 8));
  s_cal.P3 = (int16_t)(tp[10] | (tp[11] << 8));
  s_cal.P4 = (int16_t)(tp[12] | (tp[13] << 8));
  s_cal.P5 = (int16_t)(tp[14] | (tp[15] << 8));
  s_cal.P6 = (int16_t)(tp[16] | (tp[17] << 8));
  s_cal.P7 = (int16_t)(tp[18] | (tp[19] << 8));
  s_cal.P8 = (int16_t)(tp[20] | (tp[21] << 8));
  s_cal.P9 = (int16_t)(tp[22] | (tp[23] << 8));
  s_cal.H1 = tp[25];                                   /* 0xA1 */

  s_cal.H2 = (int16_t)(h[0] | (h[1] << 8));            /* 0xE1/0xE2 */
  s_cal.H3 = h[2];                                     /* 0xE3 */
  s_cal.H4 = (int16_t)((h[3] << 4) | (h[4] & 0x0F));   /* 0xE4/0xE5[3:0] */
  s_cal.H5 = (int16_t)((h[5] << 4) | (h[4] >> 4));     /* 0xE6/0xE5[7:4] */
  s_cal.H6 = (int8_t)h[6];                             /* 0xE7 */
  return true;
}

/* Datasheet fixed-point compensation (32-bit integer forms). */
static int32_t compensate_temp(int32_t adc_T) {
  int32_t v1 = ((((adc_T >> 3) - ((int32_t)s_cal.T1 << 1))) * ((int32_t)s_cal.T2)) >> 11;
  int32_t v2 = (((((adc_T >> 4) - ((int32_t)s_cal.T1)) *
                 ((adc_T >> 4) - ((int32_t)s_cal.T1))) >> 12) * ((int32_t)s_cal.T3)) >> 14;
  s_t_fine = v1 + v2;
  return (s_t_fine * 5 + 128) >> 8;          /* 0.01 C */
}

static uint32_t compensate_press(int32_t adc_P) {
  int64_t v1 = ((int64_t)s_t_fine) - 128000;
  int64_t v2 = v1 * v1 * (int64_t)s_cal.P6;
  v2 += (v1 * (int64_t)s_cal.P5) << 17;
  v2 += ((int64_t)s_cal.P4) << 35;
  v1 = ((v1 * v1 * (int64_t)s_cal.P3) >> 8) + ((v1 * (int64_t)s_cal.P2) << 12);
  v1 = ((((int64_t)1 << 47) + v1) * (int64_t)s_cal.P1) >> 33;
  if (v1 == 0) return 0;                     /* avoid divide-by-zero */
  int64_t p = 1048576 - adc_P;
  p = (((p << 31) - v2) * 3125) / v1;
  v1 = (((int64_t)s_cal.P9) * (p >> 13) * (p >> 13)) >> 25;
  v2 = (((int64_t)s_cal.P8) * p) >> 19;
  p = ((p + v1 + v2) >> 8) + (((int64_t)s_cal.P7) << 4);
  return (uint32_t)(p >> 8);                 /* Pa */
}

static uint32_t compensate_hum(int32_t adc_H) {
  int32_t v = s_t_fine - 76800;
  v = ((((adc_H << 14) - (((int32_t)s_cal.H4) << 20) - (((int32_t)s_cal.H5) * v)) + 16384) >> 15) *
      (((((((v * ((int32_t)s_cal.H6)) >> 10) *
           (((v * ((int32_t)s_cal.H3)) >> 11) + 32768)) >> 10) + 2097152) *
         ((int32_t)s_cal.H2) + 8192) >> 14);
  v -= (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)s_cal.H1)) >> 4);
  if (v < 0) v = 0;
  if (v > 419430400) v = 419430400;
  return (uint32_t)(v >> 12);                /* %RH * 1024 (Q22.10) */
}

sensor_result_t bme280_init(const bme280_config_t *config) {
  if (config == NULL) return SENSOR_ERR_PARAM;
  s_config = *config;
  s_initialized = false;

  i2c_instance_t inst = (i2c_instance_t)s_config.i2c_instance;
  if (!i2c_init(inst, 100000U)) return SENSOR_ERR_BUS;

  uint8_t id = 0;
  if (!read_regs(BME280_REG_ID, &id, 1)) return SENSOR_ERR_BUS;
  if (id != BME280_CHIP_ID) return SENSOR_ERR_NOT_READY;

  if (!read_calibration()) return SENSOR_ERR_BUS;

  /* ctrl_hum must be written BEFORE ctrl_meas for it to take effect. */
  if (!write_reg(BME280_REG_CTRL_HUM, 0x01U)) return SENSOR_ERR_BUS;   /* hum x1 */
  if (!write_reg(BME280_REG_CTRL_MEAS, 0xB7U)) return SENSOR_ERR_BUS;  /* T x16, P x16, normal */

  s_initialized = true;
  return SENSOR_OK;
}

sensor_result_t bme280_read(bme280_data_t *out) {
  if (out == NULL) return SENSOR_ERR_PARAM;
  if (!s_initialized) return SENSOR_ERR_NOT_READY;

  uint8_t raw[BME280_RAW_LEN];
  if (!read_regs(BME280_REG_PRESS, raw, BME280_RAW_LEN)) return SENSOR_ERR_BUS;

  int32_t p_raw = (int32_t)(((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | (raw[2] >> 4));
  int32_t t_raw = (int32_t)(((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | (raw[5] >> 4));
  int32_t h_raw = (int32_t)(((uint32_t)raw[6] << 8) | raw[7]);

  out->temp_c_x100    = compensate_temp(t_raw);   /* sets s_t_fine first */
  out->pressure_pa    = compensate_press(p_raw);
  out->humidity_x1024 = compensate_hum(h_raw);
  return SENSOR_OK;
}
