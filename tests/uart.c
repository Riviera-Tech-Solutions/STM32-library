#include "mock.h"
static void before_access(uintptr_t a,unsigned w) { (void)a; (void)w; }
#include DRIVER_SOURCE
int main(void) {
  CHECK(!uart_init(0,0)); CHECK(!uart_init(0,100000000));
  CHECK(!uart_init(0,1)); CHECK(register_count==0);
  CHECK(uart_init(0,115200)); CHECK(*raw(0x40011008U)==729);
  uart_putc(0,42);
  uart_stats_t stats; CHECK(uart_get_stats(0,&stats)); CHECK(stats.last_error==UART_ERR_TX_TIMEOUT);
  *raw(0x40011000U)=128;
  uart_putc(0,42); CHECK(*raw(0x40011004U)==42);
  CHECK(uart_get_stats(0,&stats)); CHECK(stats.tx_ok==1);
  return 0;
}
