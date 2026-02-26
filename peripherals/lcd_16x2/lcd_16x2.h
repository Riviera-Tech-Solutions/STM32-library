/**
 * @file lcd_16x2.h
 * @brief HD44780 16x2 LCD – I2C (PCF8574) or 4-bit GPIO. Senior design: config struct, result type.
 */
#ifndef LCD_16X2_H
#define LCD_16X2_H

#include <stdint.h>
#include <stdbool.h>
#include "../common/sensor_result.h"

#ifndef LCD_COLS
#define LCD_COLS 16
#endif
#ifndef LCD_ROWS
#define LCD_ROWS 2
#endif
#ifndef LCD_I2C_TIMEOUT_MS
#define LCD_I2C_TIMEOUT_MS 10
#endif

typedef enum {
  LCD_BACKEND_I2C,
  LCD_BACKEND_4BIT_GPIO
} lcd_backend_t;

typedef struct {
  lcd_backend_t backend;
  uint8_t       i2c_instance;  /* 0 = I2C1, 1 = I2C2 */
  uint8_t       i2c_addr;      /* 7-bit, e.g. 0x27 for PCF8574 */
  uint8_t       gpio_rs_port;
  uint8_t       gpio_rs_pin;
  uint8_t       gpio_en_port;
  uint8_t       gpio_en_pin;
  uint8_t       gpio_d4_port;
  uint8_t       gpio_d4_pin;
  uint8_t       gpio_d5_port;
  uint8_t       gpio_d5_pin;
  uint8_t       gpio_d6_port;
  uint8_t       gpio_d6_pin;
  uint8_t       gpio_d7_port;
  uint8_t       gpio_d7_pin;
} lcd_16x2_config_t;

sensor_result_t lcd_16x2_init(const lcd_16x2_config_t *config);
sensor_result_t lcd_16x2_clear(void);
sensor_result_t lcd_16x2_set_cursor(uint8_t row, uint8_t col);
sensor_result_t lcd_16x2_write_string(const char *s);
sensor_result_t lcd_16x2_write_char(char c);

#endif /* LCD_16X2_H */
