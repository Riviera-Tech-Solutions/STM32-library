#include "mock.h"
static bool saw_start, stuck;
static void before_access(uintptr_t a,unsigned w) {
  (void)w;
  if (*raw(0x40003000U)==0xCCCCU) saw_start=true;
  if (a==0x40003004U || a==0x40003008U) CHECK(saw_start);
  if (a==0x4000300CU) *raw(a)=stuck?3:0;
}
#include DRIVER_SOURCE
int main(void) {
  CHECK(wdg_init(2000)==2000); CHECK(saw_start);
  CHECK(wdg_init(60000)==32768); CHECK(*raw(0x40003004U)==6);
  stuck=true; CHECK(wdg_init(1000)==0);
  return 0;
}
