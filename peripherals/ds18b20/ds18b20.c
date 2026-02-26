/**
 * @file ds18b20.c
 * @brief DS18B20 1-Wire – reset, skip ROM, convert, read scratchpad. Temp in 1/16 °C.
 */
#include "ds18b20.h"
#include <stddef.h>
#include <stdbool.h>
#include "../../drivers/gpio/stm32_gpio.h"

#ifndef DS18B20_LOOPS_PER_US
#define DS18B20_LOOPS_PER_US 72
#endif

#define CMD_SKIP_ROM  0xCCU
#define CMD_CONVERT   0x44U
#define CMD_READ_SP   0xBEU
#define RESET_LOW_US  480
#define PRESENCE_WAIT_US 100
#define SLOT_LOW_US   6
#define SLOT_RECOVERY_US 64

static ds18b20_config_t s_config;
static bool s_initialized;

static void delay_us(uint32_t us) {
  for (volatile uint32_t i = 0; i < us * (uint32_t)DS18B20_LOOPS_PER_US; i++) { (void)i; }
}

static void delay_ms(uint32_t ms) {
  for (uint32_t j = 0; j < ms; j++) delay_us(1000);
}

static void line_low(void) {
  gpio_set_mode(s_config.gpio_port, s_config.gpio_pin, GPIO_MODE_OUTPUT);
  gpio_write(s_config.gpio_port, s_config.gpio_pin, false);
}

static void line_release(void) {
  gpio_set_mode(s_config.gpio_port, s_config.gpio_pin, GPIO_MODE_INPUT);
  gpio_set_pull(s_config.gpio_port, s_config.gpio_pin, GPIO_PULL_UP);
}

static bool line_read(void) {
  return gpio_read(s_config.gpio_port, s_config.gpio_pin);
}

static bool onewire_reset(void) {
  line_low();
  delay_us(RESET_LOW_US);
  line_release();
  delay_us(PRESENCE_WAIT_US);
  bool presence = !line_read();
  delay_us(400 - PRESENCE_WAIT_US);
  return presence;
}

static void onewire_write_byte(uint8_t byte) {
  for (int i = 0; i < 8; i++) {
    line_low();
    if (byte & 1U) {
      delay_us(SLOT_LOW_US);
      line_release();
      delay_us(SLOT_RECOVERY_US - SLOT_LOW_US);
    } else {
      delay_us(60);
      line_release();
      delay_us(SLOT_RECOVERY_US - 60);
    }
    byte >>= 1;
  }
}

static uint8_t onewire_read_byte(void) {
  uint8_t byte = 0;
  for (int i = 0; i < 8; i++) {
    line_low();
    delay_us(SLOT_LOW_US);
    line_release();
    delay_us(10);
    if (line_read()) byte |= (1U << (unsigned)i);
    delay_us(55);
  }
  return byte;
}

sensor_result_t ds18b20_init(const ds18b20_config_t *config) {
  if (config == NULL) return SENSOR_ERR_PARAM;
  s_config = *config;
  line_release();
  s_initialized = true;
  return SENSOR_OK;
}

sensor_result_t ds18b20_read_temp_c_x16(int16_t *temp_c_x16) {
  if (temp_c_x16 == NULL || !s_initialized) return SENSOR_ERR_PARAM;

  if (!onewire_reset()) return SENSOR_ERR_TIMEOUT;

  onewire_write_byte(CMD_SKIP_ROM);
  onewire_write_byte(CMD_CONVERT);
  line_release();
  delay_ms(750);

  if (!onewire_reset()) return SENSOR_ERR_TIMEOUT;

  onewire_write_byte(CMD_SKIP_ROM);
  onewire_write_byte(CMD_READ_SP);
  uint8_t sp[9];
  for (int i = 0; i < 9; i++) sp[i] = onewire_read_byte();

  int16_t raw = (int16_t)((int16_t)sp[1] << 8 | sp[0]);
  *temp_c_x16 = raw;
  return SENSOR_OK;
}
