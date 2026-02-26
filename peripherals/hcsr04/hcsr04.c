/**
 * @file hcsr04.c
 * @brief HC-SR04 – trigger GPIO, measure echo pulse with busy-wait. distance_mm = pulse_us / 5.8.
 */
#include "hcsr04.h"
#include <stddef.h>
#include <stdbool.h>
#include "../../drivers/gpio/stm32_gpio.h"

/* Approximate loops per microsecond at 72 MHz (tune for your SYSCLK). */
#ifndef HCSR04_LOOPS_PER_US
#define HCSR04_LOOPS_PER_US 72
#endif

#define TRIG_PULSE_US 10
#define US_PER_MM_NUM 10
#define US_PER_MM_DEN 58

static hcsr04_config_t s_config;
static bool s_initialized;

static void delay_us(uint32_t us) {
  for (volatile uint32_t i = 0; i < us * (uint32_t)HCSR04_LOOPS_PER_US; i++) { (void)i; }
}

sensor_result_t hcsr04_init(const hcsr04_config_t *config) {
  if (config == NULL) return SENSOR_ERR_PARAM;
  s_config = *config;
  gpio_set_mode(s_config.trig_port, s_config.trig_pin, GPIO_MODE_OUTPUT);
  gpio_set_mode(s_config.echo_port, s_config.echo_pin, GPIO_MODE_INPUT);
  gpio_set_pull(s_config.echo_port, s_config.echo_pin, GPIO_PULL_NONE);
  gpio_write(s_config.trig_port, s_config.trig_pin, false);
  s_initialized = true;
  return SENSOR_OK;
}

sensor_result_t hcsr04_read_mm(uint32_t *distance_mm) {
  if (distance_mm == NULL || !s_initialized) return SENSOR_ERR_PARAM;

  gpio_write(s_config.trig_port, s_config.trig_pin, true);
  delay_us(TRIG_PULSE_US);
  gpio_write(s_config.trig_port, s_config.trig_pin, false);

  uint32_t timeout = (uint32_t)HCSR04_TIMEOUT_MS * 1000U * (uint32_t)HCSR04_LOOPS_PER_US;
  while (!gpio_read(s_config.echo_port, s_config.echo_pin) && timeout--) { }
  if (timeout == 0) return SENSOR_ERR_TIMEOUT;

  uint32_t count = 0;
  const uint32_t max_count = (uint32_t)HCSR04_LOOPS_PER_US * 30000U;
  while (gpio_read(s_config.echo_port, s_config.echo_pin) && count < max_count) count++;
  if (count >= max_count) return SENSOR_ERR_TIMEOUT;

  uint32_t pulse_us = count / (uint32_t)HCSR04_LOOPS_PER_US;
  *distance_mm = (pulse_us * US_PER_MM_NUM) / US_PER_MM_DEN;
  return SENSOR_OK;
}
