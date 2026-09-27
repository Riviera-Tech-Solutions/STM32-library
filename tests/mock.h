#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
static struct { uintptr_t address; uint32_t value; } registers[256];
static unsigned register_count;
static uint32_t *raw(uintptr_t address) {
  for (unsigned i=0;i<register_count;i++) if (registers[i].address==address) return &registers[i].value;
  CHECK(register_count < 256);
  registers[register_count].address=address;
  return &registers[register_count++].value;
}
static void before_access(uintptr_t address, unsigned width);
static volatile uint32_t *mock32(uintptr_t address) { before_access(address,32); return raw(address); }
static volatile uint8_t *mock8(uintptr_t address) { before_access(address,8); return (uint8_t *)raw(address); }
