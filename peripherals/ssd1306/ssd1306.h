/**
 * @file ssd1306.h
 * @brief SSD1306 128x64 OLED – I2C or 4-wire SPI (D/C pin via GPIO driver).
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
  /* 4-wire SPI only: the D/C (data/command) GPIO. port 0 = GPIOA. */
  uint8_t spi_dc_port;
  uint8_t spi_dc_pin;
} ssd1306_config_t;

/* Brings the panel out of reset state: full SSD1306 command sequence
 * (charge pump, addressing mode, mux, display on). Returns SENSOR_ERR_BUS
 * if the display does not ACK. */
sensor_result_t ssd1306_init(const ssd1306_config_t *config);
sensor_result_t ssd1306_clear(void);
sensor_result_t ssd1306_draw_pixel(uint8_t x, uint8_t y, uint8_t on);
/* 5x7 font (ASCII 32..126), 6 px advance: 21 columns x 8 rows of text.
 * col in pixels (0..127), page in 8-pixel rows (0..7). */
sensor_result_t ssd1306_draw_char(uint8_t col, uint8_t page, char c);
sensor_result_t ssd1306_draw_text(uint8_t col, uint8_t page, const char *text);
/* Pushes the framebuffer to the panel (sets the full address window first). */
sensor_result_t ssd1306_refresh(void);

#endif /* SSD1306_H */
