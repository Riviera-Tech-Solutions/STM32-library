/**
 * @file dht22.c
 * @brief DHT22 – 1-wire protocol. Host pulls low 1–10 ms, then reads 40 bits (hum hi/lo, temp hi/lo, checksum).
 */
#include "dht22.h"
#include <stddef.h>
#include <stdbool.h>
#include "../../drivers/gpio/stm32_gpio.h"

#ifndef DHT22_LOOPS_PER_US
#define DHT22_LOOPS_PER_US 72
#endif

static dht22_config_t s_config;
static bool s_initialized;

static void delay_us(uint32_t us) {
  for (volatile uint32_t i = 0; i < us * (uint32_t)DHT22_LOOPS_PER_US; i++) { (void)i; }
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

static bool wait_level(bool high, uint32_t timeout_us) {
  for (uint32_t i = 0; i < timeout_us * (uint32_t)DHT22_LOOPS_PER_US; i++) {
    if (line_read() == high) return true;
  }
  return false;
}

sensor_result_t dht22_init(const dht22_config_t *config) {
  if (config == NULL) return SENSOR_ERR_PARAM;
  s_config = *config;
  line_release();
  s_initialized = true;
  return SENSOR_OK;
}

sensor_result_t dht22_read(dht22_data_t *out) {
  if (out == NULL || !s_initialized) return SENSOR_ERR_PARAM;

  line_low();
  delay_ms(2);
  line_release();
  delay_us(30);

  if (!wait_level(true, 200)) return SENSOR_ERR_TIMEOUT;
  if (!wait_level(false, 200)) return SENSOR_ERR_TIMEOUT;
  if (!wait_level(true, 200)) return SENSOR_ERR_TIMEOUT;

  uint8_t buf[5];
  for (int byte = 0; byte < 5; byte++) {
    uint8_t v = 0;
    for (int bit = 7; bit >= 0; bit--) {
      if (!wait_level(false, (uint32_t)DHT22_TIMEOUT_US)) return SENSOR_ERR_TIMEOUT;
      if (!wait_level(true, (uint32_t)DHT22_TIMEOUT_US)) return SENSOR_ERR_TIMEOUT;
      delay_us(35);
      if (line_read()) v |= (1U << (unsigned)bit);
    }
    buf[byte] = v;
  }

  uint8_t sum = (uint8_t)((uint16_t)buf[0] + (uint16_t)buf[1] + (uint16_t)buf[2] + (uint16_t)buf[3]);
  if (sum != buf[4]) return SENSOR_ERR_CHECKSUM;

  out->humidity_x10 = (uint16_t)((uint16_t)buf[0] << 8 | buf[1]);
  int16_t raw_temp = (int16_t)((int16_t)buf[2] << 8 | buf[3]);
  out->temp_c_x10 = raw_temp;
  return SENSOR_OK;
}
