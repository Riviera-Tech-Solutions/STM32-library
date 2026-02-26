/**
 * @file lcd_16x2.c
 * @brief HD44780 16x2 – I2C (PCF8574) or 4-bit GPIO. Senior design: no magic numbers.
 * Requires I2C driver for LCD_BACKEND_I2C. Provide delay_ms() for init timing (e.g. in sys/delay.c).
 */
#include "lcd_16x2.h"
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "../../drivers/i2c/stm32_i2c.h"

/* HD44780 command bytes */
#define LCD_CMD_CLEAR       0x01U
#define LCD_CMD_HOME        0x02U
#define LCD_CMD_ENTRY       (0x04U | 0x02U)  /* increment, no shift */
#define LCD_CMD_DISPLAY     (0x08U | 0x04U)  /* display on, cursor off */
#define LCD_CMD_FUNCTION    (0x20U | 0x08U)  /* 4-bit, 2 lines, 5x8 */
#define LCD_RS_DATA         1
#define LCD_RS_CMD          0
#define LCD_PCF8574_BL      0x08U  /* backlight bit on PCF8574 expander */

static lcd_16x2_config_t s_config;
static bool s_initialized;

/* Busy-wait delay; override with SysTick-based delay in project if needed. */
static void lcd_delay_ms(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i++)
    for (volatile uint32_t j = 0; j < 10000; j++) { }
}

static sensor_result_t write_nibble(uint8_t nibble, uint8_t rs) {
  uint8_t byte = (nibble << 4) | (rs ? LCD_RS_DATA : 0) | LCD_PCF8574_BL;
  i2c_instance_t inst = (i2c_instance_t)s_config.i2c_instance;
  if (!i2c_write(inst, s_config.i2c_addr, &byte, 1)) return SENSOR_ERR_BUS;
  lcd_delay_ms(1);
  return SENSOR_OK;
}

sensor_result_t lcd_16x2_init(const lcd_16x2_config_t *config) {
  if (config == NULL) return SENSOR_ERR_PARAM;
  memcpy(&s_config, config, sizeof(lcd_16x2_config_t));
  s_initialized = true;
  if (s_config.backend == LCD_BACKEND_I2C) {
    /* 4-bit init sequence per HD44780 */
    lcd_delay_ms(40);
    write_nibble(0x03, LCD_RS_CMD);
    lcd_delay_ms(5);
    write_nibble(0x03, LCD_RS_CMD);
    write_nibble(0x03, LCD_RS_CMD);
    write_nibble(0x02, LCD_RS_CMD);
    write_nibble(LCD_CMD_FUNCTION >> 4, LCD_RS_CMD);
    write_nibble(LCD_CMD_DISPLAY >> 4, LCD_RS_CMD);
    write_nibble(LCD_CMD_ENTRY >> 4, LCD_RS_CMD);
    write_nibble(LCD_CMD_CLEAR >> 4, LCD_RS_CMD);
  }
  return SENSOR_OK;
}

sensor_result_t lcd_16x2_clear(void) {
  if (!s_initialized) return SENSOR_ERR_NOT_READY;
  write_nibble(LCD_CMD_CLEAR >> 4, LCD_RS_CMD);
  write_nibble(LCD_CMD_CLEAR & 0x0F, LCD_RS_CMD);
  return SENSOR_OK;
}

sensor_result_t lcd_16x2_set_cursor(uint8_t row, uint8_t col) {
  if (!s_initialized || row >= LCD_ROWS || col >= LCD_COLS) return SENSOR_ERR_PARAM;
  uint8_t addr = col + (row == 1 ? 0x40U : 0x00U);
  uint8_t cmd = 0x80U | addr;
  write_nibble(cmd >> 4, LCD_RS_CMD);
  write_nibble(cmd & 0x0F, LCD_RS_CMD);
  return SENSOR_OK;
}

sensor_result_t lcd_16x2_write_char(char c) {
  if (!s_initialized) return SENSOR_ERR_NOT_READY;
  uint8_t b = (uint8_t)c;
  write_nibble(b >> 4, LCD_RS_DATA);
  write_nibble(b & 0x0F, LCD_RS_DATA);
  return SENSOR_OK;
}

sensor_result_t lcd_16x2_write_string(const char *s) {
  if (s == NULL) return SENSOR_ERR_PARAM;
  while (*s) {
    sensor_result_t r = lcd_16x2_write_char(*s++);
    if (r != SENSOR_OK) return r;
  }
  return SENSOR_OK;
}
