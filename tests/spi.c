#include "mock.h"
static bool transferring, stuck_busy, overrun;
static unsigned data_accesses;
static void before_access(uintptr_t a,unsigned width) {
  if (transferring && a==0x40013008U) *raw(a)=3U | (stuck_busy?128U:0U) | (overrun?64U:0U);
  if (transferring && a==0x4001300CU) { CHECK(width==8); data_accesses++; }
}
#include DRIVER_SOURCE
int main(void) {
  uint8_t tx[]={1,2,3},rx[3];
  CHECK(!spi_transfer(SPI_INSTANCE_1,tx,rx,3));
  CHECK(!spi_init((spi_instance_t)99,1000000));
  CHECK(!spi_init(SPI_INSTANCE_1,0));
  CHECK(!spi_init(SPI_INSTANCE_1,1));
  CHECK(spi_init(SPI_INSTANCE_1,1000000));
  unsigned br1=(*raw(0x40013000U)>>3)&7U;
  CHECK(spi_init(SPI_INSTANCE_1,10000000));
  CHECK(((*raw(0x40013000U)>>3)&7U)<br1);
  CHECK((*raw(0x40013000U)&0x300U)==0x300U);
#if defined(STM32G0)
  CHECK((*raw(0x40013004U)&0x1F00U)==0x1700U);
#endif
  transferring=true;
  CHECK(spi_transfer(SPI_INSTANCE_1,tx,NULL,3));
  CHECK(data_accesses==6); /* one byte write AND read for every frame */
  CHECK(spi_transfer(SPI_INSTANCE_1,tx,rx,3));
  CHECK(memcmp(tx,rx,3)==0);
  stuck_busy=true;
  CHECK(!spi_transfer(SPI_INSTANCE_1,tx,rx,1));
  stuck_busy=false;
  CHECK(!spi_transfer(SPI_INSTANCE_1,tx,rx,1)); /* fault invalidates state */
  transferring=false; *raw(0x40013008U)=0;
  CHECK(spi_init(SPI_INSTANCE_1,1000000));
  transferring=true; overrun=true;
  CHECK(!spi_transfer(SPI_INSTANCE_1,tx,rx,1));
  return 0;
}
