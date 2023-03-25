#include "vsfs_stdinc.h"

#define print_binary(target, size) _print_binary((int8_t *)target, size)
static void _print_binary(int8_t *byte, size_t size) { // little-endian version
  byte += (size - 1);
  for (size_t i = 0; i < size; i++, byte--) {
    for (int j = 7; j >= 0; j--) {
      printf("%d", ((*byte >> j) & 0b00000001));
    }
    printf(" ");
    if ((i + 1) % 8 == 0)
      printf("\n");
  }
}
