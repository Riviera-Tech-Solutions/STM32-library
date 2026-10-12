/**
 * @file stm32_i2c.c
 * @brief STM32 I2C master – blocking, polled. Two hardware generations,
 *        family assignment verified against ST CMSIS device headers:
 *
 *   LEGACY (SR1/SR2/DR, CCR/TRISE):  F1, F2, F4, L1
 *   V2     (TIMINGR/ISR/RXDR/TXDR):  F0, F3, F7, G0, G4, L0, L4, L4+
 *
 * (A previous revision put F0/F3/F7/G4/L4 in the legacy group — those parts
 * have the v2 peripheral, so every register access was off. The v2 block also
 * polled ISR at +0x1C, which is ICR, a write-only clear register; real ISR is
 * at +0x18. v2 reads never set RD_WRN, so a "read" was a write on the wire,
 * and STOP used bit 12 — reserved — instead of 14.)
 *
 * Timing derives from I2C_KERNEL_HZ, defaulting to the clock THIS library's
 * SystemInit actually leaves for the I2C kernel on each family. Override it
 * (and/or I2C_V2_TIMINGR_100K/_400K) if you reconfigure the clock tree.
 *
 * Timeout: I2C_TIMEOUT_MS (default 100).
 */

#include "stm32_i2c.h"
#include "../common/stm32_family.h"
#include <stddef.h>
#include "../common/stm32_wait.h"
#include "../common/stm32_irq.h"

#ifndef I2C_TIMEOUT_MS
#define I2C_TIMEOUT_MS 100
#endif

#ifndef I2C_MAX_RETRIES
#define I2C_MAX_RETRIES 2
#endif

#define I2C_SPIN_DELAY_CYCLES 1000U

/* ---- Family config: generation, RCC enable, kernel clock ---------------- */
#if defined(STM32F1)
#define I2C_MAP_LEGACY
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x4002101CU))   /* APB1ENR */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 8000000U     /* PCLK1 = HSI 8 MHz */
#endif
#elif defined(STM32F2)
#define I2C_MAP_LEGACY
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x40023840U))   /* APB1ENR */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 16000000U    /* PCLK1 = HSI 16 MHz */
#endif
#elif defined(STM32F4)
#define I2C_MAP_LEGACY
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x40023840U))   /* APB1ENR */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 42000000U    /* PCLK1 after the 84 MHz PLL bring-up */
#endif
#elif defined(STM32L1)
#define I2C_MAP_LEGACY
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x40023824U))   /* APB1ENR */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 2097000U     /* PCLK1 = MSI default */
#endif

#elif defined(STM32F0)
#define I2C_MAP_V2
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x4002101CU))   /* APB1ENR */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 8000000U     /* I2C1 kernel defaults to HSI8 */
#endif
#elif defined(STM32F3)
#define I2C_MAP_V2
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x4002101CU))   /* APB1ENR */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 8000000U     /* I2C kernel defaults to HSI 8 MHz */
#endif
#elif defined(STM32F7)
#define I2C_MAP_V2
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x40023840U))   /* APB1ENR */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 16000000U    /* PCLK1 = HSI 16 MHz */
#endif
#elif defined(STM32G0)
#define I2C_MAP_V2
/* RM0444/CMSIS: APBENR1 @ RCC+0x3C (0x40021058 is reserved space) */
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x4002103CU))   /* APBENR1 */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 16000000U    /* PCLK = HSI16 */
#endif
#elif defined(STM32G4)
#define I2C_MAP_V2
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x40021058U))   /* APB1ENR1 */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 16000000U    /* PCLK1 = HSI 16 MHz */
#endif
#elif defined(STM32L0)
#define I2C_MAP_V2
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x40021038U))   /* APB1ENR */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 2097000U     /* PCLK1 = MSI default */
#endif
#elif defined(STM32L4) || defined(STM32L4PLUS)
#define I2C_MAP_V2
#define I2C_RCC_REG  (*(volatile uint32_t *)(0x40021058U))   /* APB1ENR1 */
#ifndef I2C_KERNEL_HZ
#define I2C_KERNEL_HZ 4000000U     /* PCLK1 = MSI default */
#endif
#endif

#define I2C_EN_BIT(inst)  (((inst) == I2C_INSTANCE_1) ? (1U << 21) : (1U << 22))

/* ---- Register maps ------------------------------------------------------ */
#if defined(I2C_MAP_LEGACY)
#define I2C1_BASE   0x40005400U
#define I2C2_BASE   0x40005800U
#define I2C_CR1(b)    (*(volatile uint32_t *)((b) + 0x00U))
#define I2C_CR2(b)    (*(volatile uint32_t *)((b) + 0x04U))
#define I2C_DR(b)     (*(volatile uint32_t *)((b) + 0x10U))
#define I2C_SR1(b)    (*(volatile uint32_t *)((b) + 0x14U))
#define I2C_SR2(b)    (*(volatile uint32_t *)((b) + 0x18U))
#define I2C_CCR(b)    (*(volatile uint32_t *)((b) + 0x1CU))  /* clock control (SCL) */
#define I2C_TRISE(b)  (*(volatile uint32_t *)((b) + 0x20U))  /* max rise time       */
#define CCR_FS       (1U << 15)   /* 0 = standard mode, 1 = fast mode */
#define CR1_PE       (1U << 0)
#define CR1_START    (1U << 8)
#define CR1_STOP     (1U << 9)
#define CR1_POS      (1U << 11)
#define CR1_ACK      (1U << 10)
#define SR1_SB       (1U << 0)
#define SR1_ADDR     (1U << 1)
#define SR1_TXE      (1U << 7)
#define SR1_RXNE     (1U << 6)
#define SR1_BTF      (1U << 2)
#define SR1_STOPF    (1U << 4)
#define I2C_STATUS_OFFSET 0x14U
#elif defined(I2C_MAP_V2)
#define I2C1_BASE   0x40005400U
#define I2C2_BASE   0x40005800U
#define I2C_CR1(b)    (*(volatile uint32_t *)((b) + 0x00U))
#define I2C_CR2(b)    (*(volatile uint32_t *)((b) + 0x04U))
#define I2C_TIMINGR(b) (*(volatile uint32_t *)((b) + 0x10U))  /* v2 SCL timing */
#define I2C_ISR(b)    (*(volatile uint32_t *)((b) + 0x18U))   /* status (NOT 0x1C — that is ICR) */
#define I2C_ICR(b)    (*(volatile uint32_t *)((b) + 0x1CU))
#define I2C_RDR(b)    (*(volatile uint32_t *)((b) + 0x24U))
#define I2C_TDR(b)    (*(volatile uint32_t *)((b) + 0x28U))
#define CR1_PE       (1U << 0)
#define CR2_RD_WRN   (1U << 10)
#define CR2_START    (1U << 13)
#define CR2_STOP     (1U << 14)
#define ISR_TXIS     (1U << 1)
#define ISR_RXNE     (1U << 2)
#define ISR_STOPF    (1U << 5)
#define ISR_TC       (1U << 6)
#define ICR_STOPCF   (1U << 5)
#define I2C_STATUS_OFFSET 0x18U
#endif

static uint32_t get_base(i2c_instance_t inst) {
#if defined(I2C_MAP_LEGACY) || defined(I2C_MAP_V2)
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
static bool wait_flag(uint32_t base, uint32_t mask, bool set, int timeout_ms) __attribute__((unused));
static bool wait_flag(uint32_t base, uint32_t mask, bool set, int timeout_ms) {
#if defined(I2C_MAP_LEGACY) || defined(I2C_MAP_V2)
  volatile uint32_t *st = (volatile uint32_t *)(base + I2C_STATUS_OFFSET);
  uint32_t timeout_u32 = timeout_ms > 0 ? (uint32_t)timeout_ms : 0U;
  uint32_t spin_budget = timeout_u32 > 0U ? (timeout_u32 * I2C_SPIN_DELAY_CYCLES) : I2C_SPIN_DELAY_CYCLES;
#if STM32_WAIT_USE_TICK
  uint32_t start = STM32_WAIT_GET_TICK_MS();
#endif
  for (;;) {
    uint32_t status = *st;
#if defined(I2C_MAP_LEGACY)
    if (status & ((1U << 8) | (1U << 9) | (1U << 10) | (1U << 11))) return false;
#else
    if (status & ((1U << 4) | (1U << 8) | (1U << 9) | (1U << 10))) return false;
#endif
    if (((status & mask) != 0U) == set) return true;
    if (spin_budget == 0U) return false;
    --spin_budget;
#if STM32_WAIT_USE_TICK
    if ((uint32_t)(STM32_WAIT_GET_TICK_MS() - start) >= timeout_u32) return false;
#endif
  }
#else
  (void)base;
  (void)mask;
  (void)set;
  (void)timeout_ms;
  return false;
#endif
}

static bool is_valid_inst(i2c_instance_t inst) {
  return (unsigned)inst < I2C_INSTANCE_COUNT && (I2C_INSTANCE_MASK & (1U << (unsigned)inst));
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
#if defined(I2C_MAP_LEGACY)
  /* Single-master bus. Abort the transaction, including the final retry. */
  I2C_CR1(base) |= CR1_STOP;
  I2C_CR1(base) &= ~CR1_PE;
  I2C_SR1(base) &= ~((1U << 8) | (1U << 9) | (1U << 10) | (1U << 11));
  I2C_CR1(base) &= ~(CR1_POS | CR1_ACK);
  for (volatile uint32_t d = 0; d < I2C_SPIN_DELAY_CYCLES; d++) { (void)d; }
  I2C_CR1(base) |= CR1_PE;
#elif defined(I2C_MAP_V2)
  I2C_CR1(base) &= ~CR1_PE;
  for (volatile uint32_t d = 0; d < I2C_SPIN_DELAY_CYCLES; d++) { (void)d; }
  I2C_CR1(base) |= CR1_PE;
  I2C_ICR(base) = ICR_STOPCF | (1U << 4) | (1U << 8) | (1U << 9) | (1U << 10);
#else
  (void)base;
#endif
}

#if defined(I2C_MAP_LEGACY)
/*
 * Program the legacy I2C SCL timing. Without this the peripheral has CCR = 0 and
 * never drives SCL, so nothing on the bus responds. Must run with PE = 0.
 * CCR/TRISE per RM: Sm CCR = Fpclk1/(2*Fscl), TRISE = Fpclk1_MHz + 1.
 */
static void i2c_legacy_set_timing(uint32_t base, uint32_t pclk_hz, uint32_t scl_hz) {
  const uint32_t pclk_mhz = pclk_hz / 1000000U;
  uint32_t ccr;
  uint32_t trise;
  if (scl_hz <= 100000U) {              /* standard mode */
    ccr = pclk_hz / (2U * scl_hz);
    if (ccr < 4U) ccr = 4U;
    trise = pclk_mhz + 1U;
    I2C_CCR(base) = ccr & 0x0FFFU;      /* F/S = 0, DUTY = 0 */
  } else {                              /* fast mode, DUTY = 0 */
    ccr = pclk_hz / (3U * scl_hz);
    if (ccr < 1U) ccr = 1U;
    trise = ((pclk_mhz * 300U) / 1000U) + 1U;   /* 300 ns max rise for Fm */
    I2C_CCR(base) = CCR_FS | (ccr & 0x0FFFU);
  }
  I2C_TRISE(base) = trise & 0x3FU;
}
#endif

#if defined(I2C_MAP_V2)
/*
 * Compute a workable TIMINGR for the given kernel clock and target SCL.
 * Approximate (analog-filter and rise-time margins are estimated, not tuned
 * per board): with slow kernels the actual SCL lands below the target, which
 * is I2C-legal. For exact ST-tuned values on your clock, override
 * I2C_V2_TIMINGR_100K / _400K (see AN4235 / CubeMX).
 */
static uint32_t i2c_v2_timingr(uint32_t kernel_hz, uint32_t scl_hz) {
  uint32_t presc = 0U;
  uint32_t ticks = kernel_hz / scl_hz;              /* kernel ticks per SCL period */
  while (ticks > 256U && presc < 15U) {
    presc++;
    ticks = (kernel_hz / (presc + 1U)) / scl_hz;
  }
  const uint32_t sync = 4U;                          /* filter + sync overhead */
  uint32_t avail = (ticks > sync) ? (ticks - sync) : 4U;
  uint32_t scll = (avail * 55U) / 100U;              /* SCL low slightly longer */
  uint32_t sclh = avail - scll;
  if (scll < 1U) scll = 1U;
  if (scll > 256U) scll = 256U;
  if (sclh < 1U) sclh = 1U;
  if (sclh > 256U) sclh = 256U;
  uint32_t fi_hz = kernel_hz / (presc + 1U);
  uint32_t sdadel = fi_hz / 4000000U;                /* ~250 ns data hold */
  uint32_t scldel = sdadel + 1U;                     /* ~data setup, > hold */
  if (sdadel > 15U) sdadel = 15U;
  if (scldel > 15U) scldel = 15U;
  return (presc << 28) | (scldel << 20) | (sdadel << 16) |
         (((sclh - 1U) & 0xFFU) << 8) | ((scll - 1U) & 0xFFU);
}
#endif

bool i2c_init(i2c_instance_t inst, uint32_t clock_speed_hz) {
  if (!is_valid_inst(inst)) return false;
  if (clock_speed_hz == 0U) clock_speed_hz = 100000U;
  if (clock_speed_hz < 10000U || clock_speed_hz > 400000U) {
    s_i2c_stats[inst].init_fail++;
    i2c_set_error(inst, I2C_ERR_INVALID_PARAM);
    return false;
  }
#if defined(I2C_MAP_LEGACY)
  I2C_RCC_REG |= I2C_EN_BIT(inst);
  uint32_t base = get_base(inst);
  if (base == 0U) {
    s_i2c_stats[inst].init_fail++;
    i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
    return false;
  }
  I2C_CR1(base) &= ~CR1_PE;
  I2C_CR2(base) = ((I2C_KERNEL_HZ / 1000000U) & 0x3FU);   /* FREQ field = PCLK1 MHz */
  i2c_legacy_set_timing(base, I2C_KERNEL_HZ, clock_speed_hz);
  I2C_CR1(base) |= CR1_PE;
  s_i2c_initialized[inst] = true;
  s_i2c_clock_hz[inst] = clock_speed_hz;
  s_i2c_stats[inst].init_ok++;
  i2c_set_error(inst, I2C_ERR_NONE);
  return true;
#elif defined(I2C_MAP_V2)
  I2C_RCC_REG |= I2C_EN_BIT(inst);
  uint32_t base = get_base(inst);
  if (base == 0U) {
    s_i2c_stats[inst].init_fail++;
    i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
    return false;
  }
  I2C_CR1(base) &= ~CR1_PE;
#if defined(I2C_V2_TIMINGR_100K) && defined(I2C_V2_TIMINGR_400K)
  I2C_TIMINGR(base) = (clock_speed_hz > 100000U) ? I2C_V2_TIMINGR_400K : I2C_V2_TIMINGR_100K;
#else
  I2C_TIMINGR(base) = i2c_v2_timingr(I2C_KERNEL_HZ, clock_speed_hz);
#endif
  I2C_CR1(base) |= CR1_PE;
  s_i2c_initialized[inst] = true;
  s_i2c_clock_hz[inst] = clock_speed_hz;
  s_i2c_stats[inst].init_ok++;
  i2c_set_error(inst, I2C_ERR_NONE);
  return true;
#else
  (void)inst;
  (void)clock_speed_hz;
  s_i2c_stats[inst].init_fail++;
  i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
  return false;
#endif
}

void i2c_deinit(i2c_instance_t inst) {
  if (!is_valid_inst(inst)) return;
#if defined(I2C_MAP_LEGACY) || defined(I2C_MAP_V2)
  uint32_t base = get_base(inst);
  if (base == 0U) return;
  I2C_CR1(base) &= ~CR1_PE;
  I2C_RCC_REG &= ~I2C_EN_BIT(inst);
#endif
  s_i2c_initialized[inst] = false;
}

bool i2c_recover(i2c_instance_t inst) {
  /* Keep the array bound explicit across compiler interprocedural passes. */
  if ((unsigned)inst >= I2C_INSTANCE_COUNT || !is_valid_inst(inst)) return false;
  i2c_stats_t *stats = &s_i2c_stats[(unsigned)inst];
  stats->recover_attempts++;
  uint32_t clk = s_i2c_clock_hz[inst];
  i2c_deinit(inst);
  if (!i2c_init(inst, clk)) {
    stats->recover_fail++;
    i2c_set_error(inst, I2C_ERR_INIT_FAILED);
    return false;
  }
  uint32_t base = get_base(inst);
  if (base == 0U) {
    stats->recover_fail++;
    i2c_set_error(inst, I2C_ERR_BASE_UNAVAILABLE);
    return false;
  }
  i2c_soft_recover(base);
  stats->recover_ok++;
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
#if defined(I2C_MAP_LEGACY)
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
#elif defined(I2C_MAP_V2)
  I2C_CR2(base) = ((uint32_t)(addr << 1) & 0xFEu) | ((uint32_t)len << 16U);   /* write: RD_WRN = 0 */
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

static bool transaction_valid(i2c_instance_t inst, uint8_t addr, const void *data, uint16_t len) {
  if (!is_valid_inst(inst)) return false;
  if (!s_i2c_initialized[inst]) {
    i2c_set_error(inst, I2C_ERR_NOT_INITIALIZED);
    return false;
  }
  if (addr > 0x7FU || (len != 0U && data == NULL)
#if defined(I2C_MAP_V2)
      || len > 255U
#endif
  ) {
    i2c_set_error(inst, I2C_ERR_INVALID_PARAM);
    return false;
  }
  return true;
}

bool i2c_write(i2c_instance_t inst, uint8_t addr, const uint8_t *data, uint16_t len) {
  if (!is_valid_inst(inst)) return false;
  if (!transaction_valid(inst, addr, data, len)) return false;
  s_i2c_stats[inst].write_attempts++;
  for (uint32_t attempt = 0; attempt <= I2C_MAX_RETRIES; attempt++) {
    if (i2c_write_once(inst, addr, data, len)) {
      s_i2c_stats[inst].write_ok++;
      i2c_set_error(inst, I2C_ERR_NONE);
      return true;
    }
    i2c_soft_recover(get_base(inst));
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
#if defined(I2C_MAP_LEGACY)
  I2C_CR1(base) &= ~CR1_POS;
  I2C_CR1(base) |= CR1_ACK | CR1_START;
  if (!wait_flag(base, SR1_SB, true, to)) return false;
  I2C_DR(base) = (uint32_t)((addr << 1) | 1U);
  if (!wait_flag(base, SR1_ADDR, true, to)) return false;
  uint32_t irq;
  if (len == 1U) {
    irq = stm32_irq_save();
    I2C_CR1(base) &= ~CR1_ACK;
    (void)I2C_SR1(base);
    (void)I2C_SR2(base);
    I2C_CR1(base) |= CR1_STOP;
    stm32_irq_restore(irq);
    if (!wait_flag(base, SR1_RXNE, true, to)) return false;
    data[0] = (uint8_t)I2C_DR(base);
  } else if (len == 2U) {
    irq = stm32_irq_save();
    I2C_CR1(base) |= CR1_POS;
    I2C_CR1(base) &= ~CR1_ACK;
    (void)I2C_SR1(base);
    (void)I2C_SR2(base);
    stm32_irq_restore(irq);
    if (!wait_flag(base, SR1_BTF, true, to)) return false;
    irq = stm32_irq_save();
    I2C_CR1(base) |= CR1_STOP;
    data[0] = (uint8_t)I2C_DR(base);
    data[1] = (uint8_t)I2C_DR(base);
    stm32_irq_restore(irq);
  } else {
    (void)I2C_SR1(base);
    (void)I2C_SR2(base);
    uint16_t i = 0U;
    while ((uint16_t)(len - i) > 3U) {
      if (!wait_flag(base, SR1_RXNE, true, to)) return false;
      data[i++] = (uint8_t)I2C_DR(base);
    }
    if (!wait_flag(base, SR1_BTF, true, to)) return false;
    irq = stm32_irq_save();
    I2C_CR1(base) &= ~CR1_ACK;
    data[i++] = (uint8_t)I2C_DR(base);
    stm32_irq_restore(irq);
    if (!wait_flag(base, SR1_BTF, true, to)) return false;
    irq = stm32_irq_save();
    I2C_CR1(base) |= CR1_STOP;
    data[i++] = (uint8_t)I2C_DR(base);
    data[i] = (uint8_t)I2C_DR(base);
    stm32_irq_restore(irq);
  }
  I2C_CR1(base) &= ~CR1_POS;
  return true;
#elif defined(I2C_MAP_V2)
  /* Master read: RD_WRN = 1; NACK of the final byte is automatic when NBYTES
     is exhausted — no manual NACK needed. */
  I2C_CR2(base) = ((uint32_t)(addr << 1) & 0xFEu) | CR2_RD_WRN | ((uint32_t)len << 16U);
  I2C_CR2(base) |= CR2_START;
  for (uint16_t i = 0; i < len; i++) {
    if (!wait_flag(base, ISR_RXNE, true, to)) return false;
    data[i] = (uint8_t)I2C_RDR(base);
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
  if (!transaction_valid(inst, addr, data, len)) return false;
  s_i2c_stats[inst].read_attempts++;
  for (uint32_t attempt = 0; attempt <= I2C_MAX_RETRIES; attempt++) {
    if (i2c_read_once(inst, addr, data, len)) {
      s_i2c_stats[inst].read_ok++;
      i2c_set_error(inst, I2C_ERR_NONE);
      return true;
    }
    i2c_soft_recover(get_base(inst));
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
