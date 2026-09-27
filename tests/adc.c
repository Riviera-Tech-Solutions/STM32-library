#include "mock.h"
static bool stuck_calibration, no_conversion;
static unsigned calibrations, disables;
#if defined(STM32F4) || defined(STM32F7)
#define BASE 0x40012000U
#elif defined(STM32L4)
#define BASE 0x50040000U
#else
#define BASE 0x40012400U
#endif
static void before_access(uintptr_t a,unsigned w) {
  (void)w;
  uint32_t *cr=raw(BASE+8);
#if defined(STM32F4) || defined(STM32F7) || defined(STM32F1)
#if defined(STM32F1)
  if (a==BASE+8 && (*cr&12U)) { *cr&=~12U; calibrations++; }
#endif
  if (a==BASE && !no_conversion && (*cr&((1U<<30)|(1U<<22)))) *raw(BASE)=2;
  if (a==BASE+0x4C) *raw(a)=1234;
#else
  if (a==BASE+8) {
    if (*cr&(1U<<4)) *cr&=~((1U<<4)|(1U<<2));
    if (*cr&2U) { *cr&=~3U; disables++; }
    if (*cr&(1U<<31)) {
      CHECK((*cr&1U)==0); /* calibration must never run enabled */
      if (!stuck_calibration) { *cr&=~(1U<<31); calibrations++; }
    }
  }
  if (a==BASE) {
    *raw(a)=(*cr&1U)?1U:0U;
    if (*raw(BASE+0x28)) *raw(a)|=1U<<13;
    if ((*cr&4U) && !no_conversion) { *raw(a)|=4; *cr&=~4U; }
  }
  if (a==BASE+0x40) *raw(a)=1234;
#endif
}
#include DRIVER_SOURCE
int main(void) {
  CHECK(adc_init(0));
#if defined(STM32F4) || defined(STM32F7)
  CHECK((*raw(0x40012304)&(3U<<16))==(1U<<16));
#elif defined(STM32F1)
  CHECK((*raw(0x40021004)&(3U<<14))==(2U<<14));
#elif defined(STM32L4)
  CHECK((*raw(0x50040308)&(3U<<16))==(3U<<16));
#else
  CHECK((*raw(BASE+0x10)&(3U<<30))==(2U<<30));
#endif
  CHECK(adc_read(0,1)==1234);
  adc_stats_t stats;
  CHECK(adc_read(0,31)==0); CHECK(adc_get_stats(0,&stats)); CHECK(stats.last_error==ADC_ERR_INVALID_CHANNEL);
  CHECK(adc_init(0));
#if defined(STM32G0) || defined(STM32F0) || defined(STM32L4)
  CHECK(disables==1 && calibrations==2);
  stuck_calibration=true; CHECK(!adc_init(0));
  CHECK(adc_read(0,1)==0); CHECK(adc_get_stats(0,&stats)); CHECK(stats.last_error==ADC_ERR_NOT_INITIALIZED);
  stuck_calibration=false; CHECK(adc_init(0));
#endif
  no_conversion=true;
  CHECK(adc_read(0,1)==0); CHECK(adc_get_stats(0,&stats)); CHECK(stats.last_error==ADC_ERR_TIMEOUT);
  CHECK(adc_read(0,1)==0); CHECK(adc_get_stats(0,&stats)); CHECK(stats.last_error==ADC_ERR_NOT_INITIALIZED);
  return 0;
}
