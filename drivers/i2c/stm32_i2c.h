/**
 * @file stm32_i2c.h
 * @brief STM32 I2C master driver – public API
 *
 * Optimized embedded C; no HAL bloat. Config via defines below.
 * Supports multiple families via conditional compilation.
 */

#ifndef STM32_I2C_H
#define STM32_I2C_H

#include <stdint.h>
#include <stdbool.h>

/* Define STM32F4 or STM32G0 for family-specific implementation (see stm32_i2c.c). */
/* Timeout in milliseconds for blocking operations */
#ifndef I2C_TIMEOUT_MS
#define I2C_TIMEOUT_MS 100
#endif

/* Single-master bus only. Callers must serialize access. A retry can repeat
 * already-transmitted bytes; use 0 retries for non-idempotent commands.
 * Automatic retry attempts after a failed transaction (with bus/peripheral recovery). */
#ifndef I2C_MAX_RETRIES
#define I2C_MAX_RETRIES 2
#endif

#if I2C_MAX_RETRIES < 0 || I2C_MAX_RETRIES > 10
#error "I2C_MAX_RETRIES must be between 0 and 10"
#endif
#ifndef I2C_INSTANCE_MASK
#define I2C_INSTANCE_MASK 3U
#endif

/* I2C instance index (0 = I2C1, 1 = I2C2, etc.) */
typedef enum {
  I2C_INSTANCE_1 = 0,
  I2C_INSTANCE_2,
  I2C_INSTANCE_COUNT
} i2c_instance_t;

typedef enum {
  I2C_ERR_NONE = 0,
  I2C_ERR_INVALID_INSTANCE,
  I2C_ERR_NOT_INITIALIZED,
  I2C_ERR_INVALID_PARAM,
  I2C_ERR_BASE_UNAVAILABLE,
  I2C_ERR_INIT_FAILED,
  I2C_ERR_TX_FAILED,
  I2C_ERR_RX_FAILED,
  I2C_ERR_RECOVER_FAILED
} i2c_error_t;

typedef struct {
  uint32_t init_ok;
  uint32_t init_fail;
  uint32_t recover_attempts;
  uint32_t recover_ok;
  uint32_t recover_fail;
  uint32_t write_attempts;
  uint32_t write_ok;
  uint32_t write_fail;
  uint32_t read_attempts;
  uint32_t read_ok;
  uint32_t read_fail;
  uint32_t retries_used;
  i2c_error_t last_error;
} i2c_stats_t;

/**
 * @brief Initialize I2C peripheral (clock and timing). Caller configures AF/open-drain pins.
 * @param inst Instance index
 * @param clock_speed_hz Target bus speed in Hz (e.g. 100000, 400000)
 * @return true on success, false on error
 */
bool i2c_init(i2c_instance_t inst, uint32_t clock_speed_hz);

/**
 * @brief De-initialize I2C peripheral.
 */
void i2c_deinit(i2c_instance_t inst);

/**
 * @brief Attempt software recovery after bus/peripheral faults.
 * @param inst Instance index
 * @return true if recovery completed and peripheral is usable again
 */
bool i2c_recover(i2c_instance_t inst);

/**
 * @brief Write bytes to device (blocking).
 * @param inst Instance index
 * @param addr 7-bit slave address
 * @param data Pointer to data
 * @param len Number of bytes; v2 families accept 0..255 per transaction.
 * @return true on success
 */
bool i2c_write(i2c_instance_t inst, uint8_t addr, const uint8_t *data, uint16_t len);

/**
 * @brief Read bytes from device (blocking).
 * @param inst Instance index
 * @param addr 7-bit slave address
 * @param data Buffer to store read data
 * @param len Number of bytes; v2 families accept 0..255 per transaction.
 * @return true on success
 */
bool i2c_read(i2c_instance_t inst, uint8_t addr, uint8_t *data, uint16_t len);

/**
 * @brief Get reliability/health counters for an I2C instance.
 * @param inst Instance index
 * @param out_stats Destination pointer
 * @return true on success
 */
bool i2c_get_stats(i2c_instance_t inst, i2c_stats_t *out_stats);

/**
 * @brief Reset reliability/health counters for an I2C instance.
 * @param inst Instance index
 * @return true on success
 */
bool i2c_reset_stats(i2c_instance_t inst);

#endif /* STM32_I2C_H */
