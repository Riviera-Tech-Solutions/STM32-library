/**
 * @file hcsr04.h
 * @brief HC-SR04 ultrasonic distance. Senior design: config struct, result + mm.
 */
#ifndef HCSR04_H
#define HCSR04_H

#include <stdint.h>
#include "../common/sensor_result.h"

#define HCSR04_TIMEOUT_MS 30

typedef struct {
  uint8_t trig_port;
  uint8_t trig_pin;
  uint8_t echo_port;
  uint8_t echo_pin;
} hcsr04_config_t;

sensor_result_t hcsr04_init(const hcsr04_config_t *config);
sensor_result_t hcsr04_read_mm(uint32_t *distance_mm);

#endif /* HCSR04_H */
