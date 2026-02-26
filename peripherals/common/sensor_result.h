/**
 * @file sensor_result.h
 * @brief Shared result type for sensor kit – senior design: single error domain.
 * Include from all peripheral drivers for consistent API.
 */
#ifndef SENSOR_RESULT_H
#define SENSOR_RESULT_H

#include <stdint.h>

typedef enum {
  SENSOR_OK = 0,
  SENSOR_ERR_PARAM,
  SENSOR_ERR_TIMEOUT,
  SENSOR_ERR_BUS,
  SENSOR_ERR_CHECKSUM,
  SENSOR_ERR_NOT_READY,
  SENSOR_ERR_UNKNOWN
} sensor_result_t;

#endif /* SENSOR_RESULT_H */
