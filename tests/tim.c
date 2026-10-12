#include "mock.h"
static void before_access(uintptr_t a,unsigned w) { (void)a; (void)w; }
#include DRIVER_SOURCE
int main(void) {
  CHECK(!tim_pwm_init(0,4,1)); CHECK(register_count==0);
  CHECK(!tim_pwm_init(0,1,0));
  CHECK(tim_pwm_init(0,1,1));
  CHECK(*raw(0x40000014U)==1); CHECK(*raw(0x40000028U)>0);
  tim_pwm_set_duty(0,1,100);
  CHECK(*raw(0x40000034U)==*raw(0x4000002CU)+1);
  CHECK(*raw(0x40000034U)<=65535);
  tim_pwm_set_duty(0,4,50); CHECK(*raw(0x40000034U)==*raw(0x4000002CU)+1);
  tim_pwm_set_duty(0,1,0); CHECK(*raw(0x40000034U)==0);
  return 0;
}
