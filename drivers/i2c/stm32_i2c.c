/**
 * @file stm32_i2c.c
 * @brief STM32 I2C master – all families (blocking, polled).
 * Define one of STM32F0, F1, F2, F3, F4, F7, G0, G4, L0, L1, L4, L4+, H7, WB, WL. Timeout: I2C_TIMEOUT_MS (default 100).
 */

#include "stm32_i2c.h"
#include <stddef.h>
#include "../common/stm32_wait.h"

#ifndef I2C_TIMEOUT_MS
#define I2C_TIMEOUT_MS 100
#endif

#ifndef I2C_MAX_RETRIES
#define I2C_MAX_RETRIES 2
#endif

#define I2C_SPIN_DELAY_CYCLES 1000U

/* Legacy I2C (SR1/SR2/DR): F0, F1, F2, F3, F4, F7, G4, L1, L4, L4+. Same base and register layout. */
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32G4) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
#define I2C1_BASE   0x40005400U
#define I2C2_BASE   0x40005800U
#define I2C_CR1(b)    (*(volatile uint32_t *)((b) + 0x00U))
#define I2C_CR2(b)    (*(volatile uint32_t *)((b) + 0x04U))
#define I2C_SR1(b)    (*(volatile uint32_t *)((b) + 0x14U))
#define I2C_SR2(b)    (*(volatile uint32_t *)((b) + 0x18U))
#define I2C_DR(b)     (*(volatile uint32_t *)((b) + 0x10U))
#define CR1_PE       (1U << 0)
#define CR1_START    (1U << 8)
#define CR1_STOP     (1U << 9)
#define CR1_ACK      (1U << 10)
#define SR1_SB       (1U << 0)
#define SR1_ADDR     (1U << 1)
#define SR1_TXE      (1U << 7)
#define SR1_RXNE     (1U << 6)
#define SR1_BTF      (1U << 2)
#define SR1_STOPF    (1U << 4)
#define I2C_LEGACY_APB1_MHZ 42
#endif

/* New I2C (ISR/ICR/TDR/RDR): G0, L0 */
#if defined(STM32G0) || defined(STM32L0)
#define I2C1_BASE   0x40005400U
#define I2C2_BASE   0x40005800U
#define RCC_APBENR1 (*(volatile uint32_t *)(0x40021058U))
#define I2C_CR1(b)    (*(volatile uint32_t *)((b) + 0x00U))
#define I2C_CR2(b)    (*(volatile uint32_t *)((b) + 0x04U))
#define I2C_ISR(b)    (*(volatile uint32_t *)((b) + 0x1CU))
#define I2C_ICR(b)    (*(volatile uint32_t *)((b) + 0x20U))
#define I2C_RDR(b)    (*(volatile uint32_t *)((b) + 0x24U))
#define I2C_TDR(b)    (*(volatile uint32_t *)((b) + 0x28U))
#define CR1_PE       (1U << 0)
#define CR2_START    (1U << 13)
#define CR2_STOP     (1U << 12)
#define CR2_NACK     (1U << 15)
#define ISR_TXIS     (1U << 1)
#define ISR_RXNE     (1U << 2)
#define ISR_TC       (1U << 6)
#define ISR_STOPF    (1U << 5)
#define ICR_STOPCF   (1U << 5)
#endif

static uint32_t get_base(i2c_instance_t inst) {
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32G4) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS) || defined(STM32G0) || defined(STM32L0)
  return (inst == I2C_INSTANCE_1) ? I2C1_BASE : I2C2_BASE;
#else
  (void)inst;
  return 0;
#endif
}

static bool s_i2c_initialized[I2C_INSTANCE_COUNT];
static uint32_t s_i2c_clock_hz[I2C_INSTANCE_COUNT] = { 100000U, 100000U };
static i2c_stats_t s_i2c_stats[I2C_INSTANCE_COUNT];

static int wait_timeout_ms(void) {
  return (int)I2C_TIMEOUT_MS;
}

/* Return true if flag set before timeout. */
static bool wait_flag(uint32_t base, uint32_t mask, bool set, int timeout_ms) {
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
  volatile uint32_t *sr1 = (volatile uint32_t *)(base + 0x14U);
  uint32_t timeout_u32 = timeout_ms > 0 ? (uint32_t)timeout_ms : 0U;
  uint32_t spin_budget = timeout_u32 > 0U ? (timeout_u32 * I2C_SPIN_DELAY_CYCLES) : I2C_SPIN_DELAY_CYCLES;
  return stm32_wait_mask_eq32(sr1, mask, set, timeout_u32, spin_budget);
#elif defined(STM32G0) || defined(STM32L0)
  volatile uint32_t *isr = (volatile uint32_t *)(base + 0x1CU);
  uint32_t timeout_u32 = timeout_ms > 0 ? (uint32_t)timeout_ms : 0U;
  uint32_t spin_budget = timeout_u32 > 0U ? (timeout_u32 * I2C_SPIN_DELAY_CYCLES) : I2C_SPIN_DELAY_CYCLES;
  return stm32_wait_mask_eq32(isr, mask, set, timeout_u32, spin_budget);
#else
  (void)base;
  (void)mask;
  (void)set;
  (void)timeout_ms;
  return false;
#endif
}

static bool is_valid_inst(i2c_instance_t inst) {
  return (inst >= I2C_INSTANCE_1) && (inst < I2C_INSTANCE_COUNT);
}

static void i2c_set_error(i2c_instance_t inst, i2c_error_t err) {
  if (!is_valid_inst(inst)) return;
  s_i2c_stats[inst].last_error = err;
}

static void i2c_backoff_delay(uint8_t attempt) {
  uint32_t loops = (uint32_t)I2C_SPIN_DELAY_CYCLES << (attempt > 7U ? 7U : attempt);
  for (volatile uint32_t d = 0; d < loops; d++) { (void)d; }
}

static void i2c_soft_recover(uint32_t base) {
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
  I2C_CR1(base) |= CR1_STOP;
  I2C_CR1(base) &= ~CR1_PE;
  for (volatile int d = 0; d < I2C_SPIN_DELAY_CYCLES; d++) { (void)d; }
  I2C_CR1(base) |= CR1_PE;
#elif defined(STM32G0) || defined(STM32L0)
  I2C_CR1(base) &= ~CR1_PE;
  for (volatile int d = 0; d < I2C_SPIN_DELAY_CYCLES; d++) { (void)d; }
  I2C_CR1(base) |= CR1_PE;
  I2C_ICR(base) = ICR_STOPCF;
#else
  (void)base;
#endif
}

bool i2c_init(i2c_instance_t inst, uint32_t clock_speed_hz) {
  if (!is_valid_inst(inst)) return false;
  if (clock_speed_hz == 0U) clock_speed_hz = 100000U;
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3)
  volatile uint32_t *rcc = (volatile uint32_t *)(0x40021000U + 0x1CU);
  if (inst == I2C_INSTANCE_1) *rcc |= (1U << 21); else *rcc |= (1U << 22);
  uint32_t base = get_base(inst);
  if (base == 0U) {
    s_i2c_stats[inst].init_fail++;
    i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
    return false;
  }
  I2C_CR1(base) &= ~CR1_PE;
  I2C_CR2(base) = (I2C_LEGACY_APB1_MHZ & 0x3FU);
  I2C_CR1(base) |= CR1_PE;
  s_i2c_initialized[inst] = true;
  s_i2c_clock_hz[inst] = clock_speed_hz;
  s_i2c_stats[inst].init_ok++;
  i2c_set_error(inst, I2C_ERR_NONE);
  return true;
#elif defined(STM32F4) || defined(STM32F7)
  volatile uint32_t *rcc = (volatile uint32_t *)(0x40023840U);
  if (inst == I2C_INSTANCE_1) *rcc |= (1U << 21); else *rcc |= (1U << 22);
  uint32_t base = get_base(inst);
  if (base == 0U) {
    s_i2c_stats[inst].init_fail++;
    i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
    return false;
  }
  I2C_CR1(base) &= ~CR1_PE;
  I2C_CR2(base) = (I2C_LEGACY_APB1_MHZ & 0x3FU);
  I2C_CR1(base) |= CR1_PE;
  s_i2c_initialized[inst] = true;
  s_i2c_clock_hz[inst] = clock_speed_hz;
  s_i2c_stats[inst].init_ok++;
  i2c_set_error(inst, I2C_ERR_NONE);
  return true;
#elif defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS) || defined(STM32G4)
  volatile uint32_t *rcc = (volatile uint32_t *)(0x40021000U + 0x58U);
  if (inst == I2C_INSTANCE_1) *rcc |= (1U << 21); else *rcc |= (1U << 22);
  uint32_t base = get_base(inst);
  if (base == 0U) {
    s_i2c_stats[inst].init_fail++;
    i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
    return false;
  }
  I2C_CR1(base) &= ~CR1_PE;
  I2C_CR2(base) = (I2C_LEGACY_APB1_MHZ & 0x3FU);
  I2C_CR1(base) |= CR1_PE;
  s_i2c_initialized[inst] = true;
  s_i2c_clock_hz[inst] = clock_speed_hz;
  s_i2c_stats[inst].init_ok++;
  i2c_set_error(inst, I2C_ERR_NONE);
  return true;
#elif defined(STM32G0) || defined(STM32L0)
  if (inst == I2C_INSTANCE_1)
    RCC_APBENR1 |= (1U << 21);
  else
    RCC_APBENR1 |= (1U << 22);
  uint32_t base = get_base(inst);
  if (base == 0U) {
    s_i2c_stats[inst].init_fail++;
    i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
    return false;
  }
  I2C_CR1(base) &= ~CR1_PE;
  /* G0 uses TIMINGR or analog filter; minimal: just enable PE for stub */
  I2C_CR1(base) |= CR1_PE;
  s_i2c_initialized[inst] = true;
  s_i2c_clock_hz[inst] = clock_speed_hz;
  s_i2c_stats[inst].init_ok++;
  i2c_set_error(inst, I2C_ERR_NONE);
  return true;
#else
  (void)inst;
  (void)clock_speed_hz;
  s_i2c_stats[I2C_INSTANCE_1].init_fail++;
  return false;
#endif
}

void i2c_deinit(i2c_instance_t inst) {
  if (!is_valid_inst(inst)) return;
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3)
  uint32_t base = get_base(inst);
  if (base == 0U) return;
  I2C_CR1(base) &= ~CR1_PE;
  volatile uint32_t *rcc = (volatile uint32_t *)(0x40021000U + 0x1CU);
  if (inst == I2C_INSTANCE_1) *rcc &= ~(1U << 21); else *rcc &= ~(1U << 22);
#elif defined(STM32F4) || defined(STM32F7)
  uint32_t base = get_base(inst);
  if (base == 0U) return;
  I2C_CR1(base) &= ~CR1_PE;
  volatile uint32_t *rcc = (volatile uint32_t *)(0x40023840U);
  if (inst == I2C_INSTANCE_1) *rcc &= ~(1U << 21); else *rcc &= ~(1U << 22);
#elif defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS) || defined(STM32G4)
  uint32_t base = get_base(inst);
  if (base == 0U) return;
  I2C_CR1(base) &= ~CR1_PE;
  volatile uint32_t *rcc = (volatile uint32_t *)(0x40021000U + 0x58U);
  if (inst == I2C_INSTANCE_1) *rcc &= ~(1U << 21); else *rcc &= ~(1U << 22);
#elif defined(STM32G0) || defined(STM32L0)
  uint32_t base = get_base(inst);
  if (base == 0U) return;
  I2C_CR1(base) &= ~CR1_PE;
  if (inst == I2C_INSTANCE_1) RCC_APBENR1 &= ~(1U << 21); else RCC_APBENR1 &= ~(1U << 22);
#else
  (void)inst;
#endif
  s_i2c_initialized[inst] = false;
}

bool i2c_recover(i2c_instance_t inst) {
  if (!is_valid_inst(inst)) return false;
  s_i2c_stats[inst].recover_attempts++;
  uint32_t clk = s_i2c_clock_hz[inst];
  i2c_deinit(inst);
  if (!i2c_init(inst, clk)) {
    s_i2c_stats[inst].recover_fail++;
    i2c_set_error(inst, I2C_ERR_INIT_FAILED);
    return false;
  }
  uint32_t base = get_base(inst);
  if (base == 0U) {
    s_i2c_stats[inst].recover_fail++;
    i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
    return false;
  }
  i2c_soft_recover(base);
  s_i2c_stats[inst].recover_ok++;
  i2c_set_error(inst, I2C_ERR_NONE);
  return true;
}

static bool i2c_write_once(i2c_instance_t inst, uint8_t addr, const uint8_t *data, uint16_t len) {
  if (!is_valid_inst(inst)) return false;
  if (!s_i2c_initialized[inst]) {
    i2c_set_error(inst, I2C_ERR_NOT_INITIALIZED);
    return false;
  }
  if (len == 0U) return true;
  if (data == NULL || addr > 0x7FU) {
    i2c_set_error(inst, I2C_ERR_INVALID_PARAM);
    return false;
  }
  uint32_t base = get_base(inst);
  if (base == 0U) {
    i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
    return false;
  }
  int to = wait_timeout_ms();
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
  I2C_CR1(base) |= CR1_START;
  if (!wait_flag(base, SR1_SB, true, to)) return false;
  I2C_DR(base) = (uint32_t)((addr << 1) & 0xFEu);
  if (!wait_flag(base, SR1_ADDR, true, to)) return false;
  (void)I2C_SR2(base);
  for (uint16_t i = 0; i < len; i++) {
    if (!wait_flag(base, SR1_TXE, true, to)) return false;
    I2C_DR(base) = (uint32_t)data[i];
  }
  if (!wait_flag(base, SR1_BTF, true, to)) return false;
  I2C_CR1(base) |= CR1_STOP;
  return true;
#elif defined(STM32G0) || defined(STM32L0)
  I2C_CR2(base) = ((uint32_t)(addr << 1) & 0xFFu) | ((uint32_t)len << 16U);
  I2C_CR2(base) |= CR2_START;
  if (!wait_flag(base, ISR_TXIS, true, to)) return false;
  for (uint16_t i = 0; i < len; i++) {
    I2C_TDR(base) = (uint32_t)data[i];
    if (i + 1 < len && !wait_flag(base, ISR_TXIS, true, to)) return false;
  }
  if (!wait_flag(base, ISR_TC, true, to)) return false;
  I2C_CR2(base) |= CR2_STOP;
  if (!wait_flag(base, ISR_STOPF, true, to)) return false;
  I2C_ICR(base) = ICR_STOPCF;
  return true;
#else
  (void)addr;
  (void)data;
  (void)len;
  (void)base;
  (void)to;
  return false;
#endif
}

bool i2c_write(i2c_instance_t inst, uint8_t addr, const uint8_t *data, uint16_t len) {
  if (!is_valid_inst(inst)) return false;
  s_i2c_stats[inst].write_attempts++;
  for (uint8_t attempt = 0; attempt <= I2C_MAX_RETRIES; attempt++) {
    if (i2c_write_once(inst, addr, data, len)) {
      s_i2c_stats[inst].write_ok++;
      i2c_set_error(inst, I2C_ERR_NONE);
      return true;
    }
    i2c_set_error(inst, I2C_ERR_TX_FAILED);
    if (attempt < I2C_MAX_RETRIES) {
      s_i2c_stats[inst].retries_used++;
      i2c_backoff_delay(attempt);
      if (!i2c_recover(inst)) {
        s_i2c_stats[inst].write_fail++;
        i2c_set_error(inst, I2C_ERR_RECOVER_FAILED);
        return false;
      }
    }
  }
  s_i2c_stats[inst].write_fail++;
  return false;
}

static bool i2c_read_once(i2c_instance_t inst, uint8_t addr, uint8_t *data, uint16_t len) {
  if (!is_valid_inst(inst)) return false;
  if (!s_i2c_initialized[inst]) {
    i2c_set_error(inst, I2C_ERR_NOT_INITIALIZED);
    return false;
  }
  if (len == 0U) return true;
  if (data == NULL || addr > 0x7FU) {
    i2c_set_error(inst, I2C_ERR_INVALID_PARAM);
    return false;
  }
  uint32_t base = get_base(inst);
  if (base == 0U) {
    i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
    return false;
  }
  int to = wait_timeout_ms();
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
  I2C_CR1(base) |= CR1_ACK;
  I2C_CR1(base) |= CR1_START;
  if (!wait_flag(base, SR1_SB, true, to)) return false;
  I2C_DR(base) = (uint32_t)((addr << 1) | 1u);
  if (!wait_flag(base, SR1_ADDR, true, to)) return false;
  (void)I2C_SR2(base);
  for (uint16_t i = 0; i < len; i++) {
    if (i == len - 1) I2C_CR1(base) &= ~CR1_ACK;
    if (!wait_flag(base, SR1_RXNE, true, to)) return false;
    data[i] = (uint8_t)I2C_DR(base);
  }
  I2C_CR1(base) |= CR1_STOP;
  return true;
#elif defined(STM32G0) || defined(STM32L0)
  I2C_CR2(base) = ((uint32_t)(addr << 1) | 1u) | ((uint32_t)len << 16U);
  I2C_CR2(base) |= CR2_START;
  for (uint16_t i = 0; i < len; i++) {
    if (!wait_flag(base, ISR_RXNE, true, to)) return false;
    data[i] = (uint8_t)I2C_RDR(base);
    if (i == len - 1) I2C_CR2(base) |= CR2_NACK;
  }
  if (!wait_flag(base, ISR_TC, true, to)) return false;
  I2C_CR2(base) |= CR2_STOP;
  if (!wait_flag(base, ISR_STOPF, true, to)) return false;
  I2C_ICR(base) = ICR_STOPCF;
  return true;
#else
  (void)addr;
  (void)base;
  (void)to;
  (void)data;
  (void)len;
  return false;
#endif
}

bool i2c_read(i2c_instance_t inst, uint8_t addr, uint8_t *data, uint16_t len) {
  if (!is_valid_inst(inst)) return false;
  s_i2c_stats[inst].read_attempts++;
  for (uint8_t attempt = 0; attempt <= I2C_MAX_RETRIES; attempt++) {
    if (i2c_read_once(inst, addr, data, len)) {
      s_i2c_stats[inst].read_ok++;
      i2c_set_error(inst, I2C_ERR_NONE);
      return true;
    }
    i2c_set_error(inst, I2C_ERR_RX_FAILED);
    if (attempt < I2C_MAX_RETRIES) {
      s_i2c_stats[inst].retries_used++;
      i2c_backoff_delay(attempt);
      if (!i2c_recover(inst)) {
        s_i2c_stats[inst].read_fail++;
        i2c_set_error(inst, I2C_ERR_RECOVER_FAILED);
        return false;
      }
    }
  }
  s_i2c_stats[inst].read_fail++;
  return false;
}

bool i2c_get_stats(i2c_instance_t inst, i2c_stats_t *out_stats) {
  if (!is_valid_inst(inst) || out_stats == NULL) return false;
  *out_stats = s_i2c_stats[inst];
  return true;
}

bool i2c_reset_stats(i2c_instance_t inst) {
  if (!is_valid_inst(inst)) return false;
  s_i2c_stats[inst].init_ok = 0U;
  s_i2c_stats[inst].init_fail = 0U;
  s_i2c_stats[inst].recover_attempts = 0U;
  s_i2c_stats[inst].recover_ok = 0U;
  s_i2c_stats[inst].recover_fail = 0U;
  s_i2c_stats[inst].write_attempts = 0U;
  s_i2c_stats[inst].write_ok = 0U;
  s_i2c_stats[inst].write_fail = 0U;
  s_i2c_stats[inst].read_attempts = 0U;
  s_i2c_stats[inst].read_ok = 0U;
  s_i2c_stats[inst].read_fail = 0U;
  s_i2c_stats[inst].retries_used = 0U;
  s_i2c_stats[inst].last_error = I2C_ERR_NONE;
  return true;
}
