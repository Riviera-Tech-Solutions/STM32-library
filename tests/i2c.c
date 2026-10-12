#include "mock.h"
static bool reading, nack;
static unsigned length, dr_accesses, addr_clears;
static void before_access(uintptr_t a,unsigned width) {
  (void)width;
  if (!reading) return;
#if defined(STM32F4)
  if (a==0x40005414U) *raw(a)=nack?(1U<<10):0xC7U;
  if (a==0x40005418U) {
    addr_clears++;
    CHECK(((*raw(0x40005400U)&(1U<<10))!=0)==(length>2));
    CHECK(((*raw(0x40005400U)&(1U<<11))!=0)==(length==2));
  }
  if (a==0x40005410U) {
    if (dr_accesses>0) {
      unsigned remaining=length-(dr_accesses-1);
      if (remaining<=3) CHECK((*raw(0x40005400U)&(1U<<10))==0);
      if (remaining<=2) CHECK((*raw(0x40005400U)&(1U<<9))!=0);
      *raw(a)=0x42;
    }
    dr_accesses++;
  }
#else
  if (a==0x40005418U) *raw(a)=nack?(1U<<4):0x66U;
  if (a==0x40005424U) { dr_accesses++; *raw(a)=0x42; }
#endif
}
#include DRIVER_SOURCE
int main(void) {
  uint8_t data[256];
  CHECK(!i2c_read(I2C_INSTANCE_1,0x50,data,1));
  CHECK(register_count==0); /* no implicit initialization/recovery */
  CHECK(i2c_init(I2C_INSTANCE_1,100000));
  CHECK(!i2c_read(I2C_INSTANCE_1,0x80,data,1));
  CHECK(!i2c_write(I2C_INSTANCE_1,0x50,NULL,1));
  for (length=1;length<=6;length++) {
    reading=true; dr_accesses=addr_clears=0;
    CHECK(i2c_read(I2C_INSTANCE_1,0x50,data,(uint16_t)length));
#if defined(STM32F4)
    CHECK(addr_clears==1 && dr_accesses==length+1);
#else
    CHECK(dr_accesses==length);
#endif
    for (unsigned i=0;i<length;i++) CHECK(data[i]==0x42);
    reading=false;
  }
#if defined(STM32G0)
  CHECK(!i2c_read(I2C_INSTANCE_1,0x50,data,256));
  CHECK(!i2c_write(I2C_INSTANCE_1,0x50,data,256));
  i2c_stats_t stats; CHECK(i2c_get_stats(I2C_INSTANCE_1,&stats));
  CHECK(stats.last_error==I2C_ERR_INVALID_PARAM);
#endif
  reading=true; nack=true;
  CHECK(!i2c_read(I2C_INSTANCE_1,0x50,data,1));
  return 0;
}
