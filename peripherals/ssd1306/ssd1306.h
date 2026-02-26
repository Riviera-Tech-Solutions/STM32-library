/**
 * @file ssd1306.h
 * @brief SSD1306 128x64 OLED – I2C/SPI. Senior design: config struct, result type.
 */
#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>
#include "../common/sensor_result.h"

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64
#define SSD1306_I2C_ADDR_DEFAULT 0x3CU

typedef enum { SSD1306_BACKEND_I2C, SSD1306_BACKEND_SPI } ssd1306_backend_t;

typedef struct {
  ssd1306_backend_t backend;
  uint8_t i2c_instance;
  uint8_t i2c_addr;
  uint8_t spi_instance;
} ssd1306_config_t;

sensor_result_t ssd1306_init(const ssd1306_config_t *config);
sensor_result_t ssd1306_clear(void);
sensor_result_t ssd1306_draw_pixel(uint8_t x, uint8_t y, uint8_t on);
sensor_result_t ssd1306_refresh(void);

#endif /* SSD1306_H */
