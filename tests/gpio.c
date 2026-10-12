#include "mock.h"
static void before_access(uintptr_t a,unsigned w) { (void)a; (void)w; }
#include DRIVER_SOURCE
int main(void) {
  gpio_set_mode(0,0,GPIO_MODE_INPUT);
  gpio_set_pull(0,0,GPIO_PULL_NONE);
  CHECK((*raw(0x40010800U)&15U)==4U);
  gpio_set_pull(0,0,GPIO_PULL_UP);
  CHECK((*raw(0x40010800U)&15U)==8U);
  CHECK(*raw(0x40010810U)==1U);
  gpio_set_pull(0,0,GPIO_PULL_DOWN);
  CHECK(*raw(0x40010810U)==65536U);
  gpio_set_mode(0,0,GPIO_MODE_ANALOG);
  gpio_set_pull(0,0,GPIO_PULL_UP);
  CHECK((*raw(0x40010800U)&15U)==0);
  *raw(0x4001080CU)=0x55;
  gpio_write(0,3,true); CHECK(*raw(0x40010810U)==8U);
  gpio_write(0,3,false); CHECK(*raw(0x40010810U)==(8U<<16));
  CHECK(*raw(0x4001080CU)==0x55); /* no ODR RMW */
  return 0;
}
