/** Blocking mode-0, 8-bit SPI master; chip select is owned by the caller. */
#include "stm32_spi.h"
#include "../common/stm32_family.h"
#include "../common/stm32_wait.h"
#include <stddef.h>
#if defined(STM32F0) || defined(STM32F1) || defined(STM32F3)
#define SPI_RCC_SPI1 (*(volatile uint32_t *)(0x40021018U))   /* APB2ENR */
#define SPI_RCC_SPI2 (*(volatile uint32_t *)(0x4002101CU))   /* APB1ENR */
#elif defined(STM32F2) || defined(STM32F4) || defined(STM32F7)
#define SPI_RCC_SPI1 (*(volatile uint32_t *)(0x40023844U))   /* APB2ENR */
#define SPI_RCC_SPI2 (*(volatile uint32_t *)(0x40023840U))   /* APB1ENR */
#elif defined(STM32G0)
#define SPI_RCC_SPI1 (*(volatile uint32_t *)(0x40021040U))   /* APBENR2 */
#define SPI_RCC_SPI2 (*(volatile uint32_t *)(0x4002103CU))   /* APBENR1 */
#elif defined(STM32L0)
#define SPI_RCC_SPI1 (*(volatile uint32_t *)(0x40021034U))   /* APB2ENR */
#define SPI_RCC_SPI2 (*(volatile uint32_t *)(0x40021038U))   /* APB1ENR */
#elif defined(STM32L1)
#define SPI_RCC_SPI1 (*(volatile uint32_t *)(0x40023820U))   /* APB2ENR */
#define SPI_RCC_SPI2 (*(volatile uint32_t *)(0x40023824U))   /* APB1ENR */
#elif defined(STM32G4) || defined(STM32L4) || defined(STM32L4PLUS)
#define SPI_RCC_SPI1 (*(volatile uint32_t *)(0x40021060U))   /* APB2ENR  */
#define SPI_RCC_SPI2 (*(volatile uint32_t *)(0x40021058U))   /* APB1ENR1 */
#endif
#define SPI1_EN_BIT (1U << 12)
#define SPI2_EN_BIT (1U << 14)


#if defined(SPI_RCC_SPI1)
#define SPI_CR1(b) (*(volatile uint32_t *)((b) + 0x00U))
#define SPI_CR2(b) (*(volatile uint32_t *)((b) + 0x04U))
#define SPI_SR(b)  (*(volatile uint32_t *)((b) + 0x08U))
/* Byte access prevents FIFO packing into a second frame. */
#define SPI_DR(b)  (*(volatile uint8_t *)((b) + 0x0CU))
#define CR1_SPE (1U << 6)
#define SR_RXNE (1U << 0)
#define SR_TXE (1U << 1)
#define SR_MODF (1U << 5)
#define SR_OVR (1U << 6)
#define SR_BSY (1U << 7)
#if defined(STM32F0) || defined(STM32F3) || defined(STM32F7) || defined(STM32G0) || defined(STM32G4) || defined(STM32L4) || defined(STM32L4PLUS)
#define SPI_FIFO 1
#endif
#if defined(STM32F4)
#define SPI_DEFAULT_PCLK1 42000000U
#define SPI_DEFAULT_PCLK2 84000000U
#elif defined(STM32F0) || defined(STM32F1) || defined(STM32F3)
#define SPI_DEFAULT_PCLK1 8000000U
#define SPI_DEFAULT_PCLK2 8000000U
#elif defined(STM32L0) || defined(STM32L1)
#define SPI_DEFAULT_PCLK1 2097000U
#define SPI_DEFAULT_PCLK2 2097000U
#elif defined(STM32L4) || defined(STM32L4PLUS)
#define SPI_DEFAULT_PCLK1 4000000U
#define SPI_DEFAULT_PCLK2 4000000U
#else
#define SPI_DEFAULT_PCLK1 16000000U
#define SPI_DEFAULT_PCLK2 16000000U
#endif
#ifndef SPI_PCLK1_HZ
#define SPI_PCLK1_HZ SPI_DEFAULT_PCLK1
#endif
#ifndef SPI_PCLK2_HZ
#define SPI_PCLK2_HZ SPI_DEFAULT_PCLK2
#endif
static bool initialized[SPI_INSTANCE_COUNT];
static bool valid(spi_instance_t i) {
  return (unsigned)i < SPI_INSTANCE_COUNT && (SPI_INSTANCE_MASK & (1U << (unsigned)i));
}
static uint32_t get_base(spi_instance_t i) {
  return i == SPI_INSTANCE_1 ? 0x40013000U : 0x40003800U;
}
static bool wait_status(uint32_t b, uint32_t mask, bool set) {
  uint32_t spins = SPI_TIMEOUT_LOOPS;
#if STM32_WAIT_USE_TICK
  uint32_t start = STM32_WAIT_GET_TICK_MS();
#endif
  for (;;) {
    uint32_t sr = SPI_SR(b);
    if (sr & (SR_MODF | SR_OVR)) return false;
    if (((sr & mask) != 0U) == set) return true;
    if (spins == 0U) return false;
    --spins;
#if STM32_WAIT_USE_TICK
    if ((uint32_t)(STM32_WAIT_GET_TICK_MS() - start) >= SPI_TIMEOUT_MS) return false;
#endif
  }
}
#endif

bool spi_init(spi_instance_t instance, uint32_t clock_hz) {
#if defined(SPI_RCC_SPI1)
  if (!valid(instance) || clock_hz == 0U) return false;
  uint32_t pclk = instance == SPI_INSTANCE_1 ? SPI_PCLK2_HZ : SPI_PCLK1_HZ;
  if (pclk == 0U) return false;
  uint32_t br;
  for (br = 0U; br < 8U; ++br) {
    if ((uint64_t)clock_hz * (2U << br) >= pclk) break;
  }
  if (br == 8U) return false; /* Cannot meet the requested maximum SCK. */
  if (instance == SPI_INSTANCE_1) SPI_RCC_SPI1 |= SPI1_EN_BIT;
  else SPI_RCC_SPI2 |= SPI2_EN_BIT;
  if (instance == SPI_INSTANCE_1) (void)SPI_RCC_SPI1;
  else (void)SPI_RCC_SPI2;
  uint32_t b = get_base(instance);
  initialized[instance] = false;
  SPI_CR1(b) &= ~CR1_SPE;
  /* Master, software NSS high, CPOL=CPHA=0, MSB first, full duplex. */
  SPI_CR1(b) = (1U << 2) | (1U << 8) | (1U << 9) | (br << 3);
#if defined(SPI_FIFO)
  SPI_CR2(b) = (7U << 8) | (1U << 12); /* 8-bit DS, byte RX threshold */
#else
  SPI_CR2(b) = 0U;
#endif
  /* Drain stale RX data and clear OVR with the DR/SR sequence. */
  uint32_t budget = SPI_TIMEOUT_LOOPS;
  while (SPI_SR(b) & SR_RXNE) {
    (void)SPI_DR(b);
    if (budget == 0U) return false;
    --budget;
  }
  (void)SPI_DR(b);
  (void)SPI_SR(b);
  SPI_CR1(b) |= CR1_SPE;
  initialized[instance] = true;
  return true;
#else
  (void)instance; (void)clock_hz;
  return false;
#endif
}

void spi_deinit(spi_instance_t instance) {
#if defined(SPI_RCC_SPI1)
  if (!valid(instance)) return;
  SPI_CR1(get_base(instance)) &= ~CR1_SPE;
  if (instance == SPI_INSTANCE_1) SPI_RCC_SPI1 &= ~SPI1_EN_BIT;
  else SPI_RCC_SPI2 &= ~SPI2_EN_BIT;
  initialized[instance] = false;
#else
  (void)instance;
#endif
}

bool spi_transfer(spi_instance_t instance, const uint8_t *tx, uint8_t *rx, uint16_t len) {
#if defined(SPI_RCC_SPI1)
  if (!valid(instance) || !initialized[instance]) return false;
  uint32_t b = get_base(instance);
  for (uint16_t i = 0; i < len; ++i) {
    if (!wait_status(b, SR_TXE, true)) goto failed;
    SPI_DR(b) = tx ? tx[i] : 0U;
    if (!wait_status(b, SR_RXNE, true)) goto failed;
    uint8_t in = SPI_DR(b); /* Always drain, including transmit-only traffic. */
    if (rx) rx[i] = in;
  }
  if (!wait_status(b, SR_TXE, true) || !wait_status(b, SR_BSY, false)) goto failed;
  return true;
failed:
  /* A partial transaction is not retried: caller must release CS and reinit. */
  SPI_CR1(b) &= ~CR1_SPE;
  (void)SPI_DR(b);
  (void)SPI_SR(b);
  initialized[instance] = false;
  return false;
#else
  (void)instance; (void)tx; (void)rx; (void)len;
  return false;
#endif
}
