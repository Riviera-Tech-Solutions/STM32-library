/**
 * @file stm32_spi.c
 * @brief STM32 SPI master – all families (polled). Define one of STM32F0, F1, F2, F3, F4, F7, G0, G4, L0, L1, L4, L4+, etc.
 */
#include "stm32_spi.h"
#include <stddef.h>

/* Legacy SPI (CR1/SR/DR): F0, F1, F2, F3, F4, F7, G4, L1, L4, L4+. Same layout. */
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32G4) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
#define SPI1_BASE   0x40013000U
#define SPI2_BASE   0x40003800U
#define SPI_CR1(b)   (*(volatile uint32_t *)((b) + 0x00U))
#define SPI_SR(b)    (*(volatile uint32_t *)((b) + 0x08U))
#define SPI_DR(b)    (*(volatile uint32_t *)((b) + 0x0CU))
#define CR1_SPE      (1U << 6)
#define CR1_MSTR     (1U << 2)
#define SR_TXE       (1U << 1)
#define SR_RXNE      (1U << 0)
#define SR_BSY       (1U << 7)
#endif

/* G0 / L0: SPI1 0x40013000, SPI2 0x40003800. Same CR1/SR/DR offsets. */
#if defined(STM32G0) || defined(STM32L0)
#define SPI1_BASE   0x40013000U
#define SPI2_BASE   0x40003800U
#define SPI_CR1(b)   (*(volatile uint32_t *)((b) + 0x00U))
#define SPI_SR(b)    (*(volatile uint32_t *)((b) + 0x08U))
#define SPI_DR(b)    (*(volatile uint32_t *)((b) + 0x0CU))
#define CR1_SPE      (1U << 6)
#define CR1_MSTR     (1U << 2)
#define SR_TXE       (1U << 1)
#define SR_RXNE      (1U << 0)
#define SR_BSY       (1U << 7)
#endif

static uint32_t get_base(spi_instance_t inst) {
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32G0) || defined(STM32G4) || defined(STM32L0) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
  return (inst == SPI_INSTANCE_1) ? SPI1_BASE : SPI2_BASE;
#else
  (void)inst;
  return 0;
#endif
}

static bool wait_txe(uint32_t base, int timeout) {
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32G0) || defined(STM32G4) || defined(STM32L0) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
  while ((SPI_SR(base) & SR_TXE) == 0 && timeout-- > 0) { }
  return (SPI_SR(base) & SR_TXE) != 0;
#else
  (void)base;
  (void)timeout;
  return true;
#endif
}

static bool wait_rxne(uint32_t base, int timeout) {
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32G0) || defined(STM32G4) || defined(STM32L0) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
  while ((SPI_SR(base) & SR_RXNE) == 0 && timeout-- > 0) { }
  return (SPI_SR(base) & SR_RXNE) != 0;
#else
  (void)base;
  (void)timeout;
  return true;
#endif
}

bool spi_init(spi_instance_t instance, uint32_t clock_hz) {
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3)
  volatile uint32_t *rcc_apb2 = (volatile uint32_t *)(0x40021000U + 0x18U);
  volatile uint32_t *rcc_apb1 = (volatile uint32_t *)(0x40021000U + 0x1CU);
  if (instance == SPI_INSTANCE_1) *rcc_apb2 |= (1U << 12); else *rcc_apb1 |= (1U << 14);
  uint32_t base = get_base(instance);
  SPI_CR1(base) &= ~CR1_SPE;
  SPI_CR1(base) = CR1_MSTR | (2U << 3);
  SPI_CR1(base) |= CR1_SPE;
  (void)clock_hz;
  return true;
#elif defined(STM32F4) || defined(STM32F7)
  volatile uint32_t *rcc_apb2 = (volatile uint32_t *)(0x40023800U + 0x44U);
  volatile uint32_t *rcc_apb1 = (volatile uint32_t *)(0x40023800U + 0x40U);
  if (instance == SPI_INSTANCE_1) *rcc_apb2 |= (1U << 12); else *rcc_apb1 |= (1U << 14);
  uint32_t base = get_base(instance);
  SPI_CR1(base) &= ~CR1_SPE;
  SPI_CR1(base) = CR1_MSTR | (2U << 3);
  SPI_CR1(base) |= CR1_SPE;
  (void)clock_hz;
  return true;
#elif defined(STM32G0) || defined(STM32L0)
  volatile uint32_t *rcc = (volatile uint32_t *)(0x40021000U + 0x58U);
  if (instance == SPI_INSTANCE_1) *rcc |= (1U << 12); else *rcc |= (1U << 14);
  uint32_t base = get_base(instance);
  SPI_CR1(base) &= ~CR1_SPE;
  SPI_CR1(base) = CR1_MSTR | (2U << 3);
  SPI_CR1(base) |= CR1_SPE;
  (void)clock_hz;
  return true;
#elif defined(STM32G4) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
  volatile uint32_t *rcc = (volatile uint32_t *)(0x40021000U + 0x58U);
  if (instance == SPI_INSTANCE_1) *rcc |= (1U << 12); else *rcc |= (1U << 14);
  uint32_t base = get_base(instance);
  SPI_CR1(base) &= ~CR1_SPE;
  SPI_CR1(base) = CR1_MSTR | (2U << 3);
  SPI_CR1(base) |= CR1_SPE;
  (void)clock_hz;
  return true;
#else
  (void)instance;
  (void)clock_hz;
  return true;
#endif
}

void spi_deinit(spi_instance_t instance) {
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3)
  uint32_t base = get_base(instance);
  SPI_CR1(base) &= ~CR1_SPE;
  volatile uint32_t *rcc_apb2 = (volatile uint32_t *)(0x40021000U + 0x18U);
  volatile uint32_t *rcc_apb1 = (volatile uint32_t *)(0x40021000U + 0x1CU);
  if (instance == SPI_INSTANCE_1) *rcc_apb2 &= ~(1U << 12); else *rcc_apb1 &= ~(1U << 14);
#elif defined(STM32F4) || defined(STM32F7)
  uint32_t base = get_base(instance);
  SPI_CR1(base) &= ~CR1_SPE;
  volatile uint32_t *rcc_apb2 = (volatile uint32_t *)(0x40023800U + 0x44U);
  volatile uint32_t *rcc_apb1 = (volatile uint32_t *)(0x40023800U + 0x40U);
  if (instance == SPI_INSTANCE_1) *rcc_apb2 &= ~(1U << 12); else *rcc_apb1 &= ~(1U << 14);
#elif defined(STM32G0) || defined(STM32L0) || defined(STM32G4) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
  uint32_t base = get_base(instance);
  SPI_CR1(base) &= ~CR1_SPE;
  volatile uint32_t *rcc = (volatile uint32_t *)(0x40021000U + 0x58U);
  if (instance == SPI_INSTANCE_1) *rcc &= ~(1U << 12); else *rcc &= ~(1U << 14);
#else
  (void)instance;
#endif
}

bool spi_transfer(spi_instance_t instance, const uint8_t *tx, uint8_t *rx, uint16_t len) {
  uint32_t base = get_base(instance);
  int to = (int)SPI_TIMEOUT_MS * 1000;
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F2) || defined(STM32F3) || defined(STM32F4) || defined(STM32F7) || defined(STM32G0) || defined(STM32G4) || defined(STM32L0) || defined(STM32L1) || defined(STM32L4) || defined(STM32L4PLUS)
  for (uint16_t i = 0; i < len; i++) {
    uint8_t out = tx ? tx[i] : 0;
    if (!wait_txe(base, to)) return false;
    SPI_DR(base) = (uint32_t)out;
    if (!wait_rxne(base, to)) return false;
    if (rx) rx[i] = (uint8_t)SPI_DR(base);
  }
  while ((SPI_SR(base) & SR_BSY) != 0 && to-- > 0) { }
  return true;
#else
  (void)tx;
  (void)len;
  if (rx) for (uint16_t i = 0; i < len; i++) rx[i] = 0;
  return true;
#endif
}
