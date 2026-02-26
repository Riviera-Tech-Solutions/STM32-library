/**
 * @file ssd1306.c
 * @brief SSD1306 128x64 OLED – I2C/SPI implementation. Senior design: no magic numbers.
 */
#include "ssd1306.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "../../drivers/i2c/stm32_i2c.h"
#include "../../drivers/spi/stm32_spi.h"

#define FB_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)
#define SSD1306_CMD_DATA_CONTINUE 0x40U
#define SSD1306_CMD_SET_COL_ADDR  0x21U
#define SSD1306_CMD_SET_PAGE_ADDR 0x22U
#define I2C_CHUNK 32

static ssd1306_config_t s_config;
static uint8_t s_fb[FB_SIZE];
static bool s_initialized;

sensor_result_t ssd1306_init(const ssd1306_config_t *config) {
  if (config == NULL) return SENSOR_ERR_PARAM;
  s_config = *config;
  memset(s_fb, 0, sizeof(s_fb));
  s_initialized = true;
  return SENSOR_OK;
}

sensor_result_t ssd1306_clear(void) {
  if (!s_initialized) return SENSOR_ERR_NOT_READY;
  memset(s_fb, 0, sizeof(s_fb));
  return SENSOR_OK;
}

sensor_result_t ssd1306_draw_pixel(uint8_t x, uint8_t y, uint8_t on) {
  if (!s_initialized || x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT) return SENSOR_ERR_PARAM;
  uint16_t idx = (uint16_t)(y / 8) * SSD1306_WIDTH + (uint16_t)x;
  if (on) s_fb[idx] |= (1U << (y % 8));
  else    s_fb[idx] &= ~(1U << (y % 8));
  return SENSOR_OK;
}

sensor_result_t ssd1306_refresh(void) {
  if (!s_initialized) return SENSOR_ERR_NOT_READY;

  if (s_config.backend == SSD1306_BACKEND_I2C) {
    i2c_instance_t inst = (i2c_instance_t)s_config.i2c_instance;
    uint8_t buf[1 + I2C_CHUNK];
    buf[0] = SSD1306_CMD_DATA_CONTINUE;
    for (uint16_t off = 0; off < FB_SIZE; off += I2C_CHUNK) {
      uint16_t n = (FB_SIZE - off) > I2C_CHUNK ? I2C_CHUNK : (uint16_t)(FB_SIZE - off);
      for (uint16_t i = 0; i < n; i++) buf[1 + i] = s_fb[off + i];
      if (!i2c_write(inst, s_config.i2c_addr, buf, (uint16_t)(1 + n)))
        return SENSOR_ERR_BUS;
    }
    return SENSOR_OK;
  }

  if (s_config.backend == SSD1306_BACKEND_SPI) {
    spi_instance_t inst = (spi_instance_t)s_config.spi_instance;
    uint8_t buf[1 + FB_SIZE];
    buf[0] = SSD1306_CMD_DATA_CONTINUE;
    for (uint16_t i = 0; i < FB_SIZE; i++) buf[1 + i] = s_fb[i];
    if (!spi_transfer(inst, buf, NULL, (uint16_t)(1 + FB_SIZE)))
      return SENSOR_ERR_BUS;
    return SENSOR_OK;
  }

  return SENSOR_ERR_PARAM;
}
