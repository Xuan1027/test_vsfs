#ifndef VSFS_INIT_H
#define VSFS_INIT_H
#include "../inc/vsfs_stdinc.h"

#define handle_error(msg) \
  do {                    \
    perror(msg);          \
    ret = EXIT_FAILURE;   \
  } while (0)

/* Returns ceil(a/b) */
static inline uint32_t idiv_ceil(uint32_t a, uint32_t b) {
  return (a / b) + !!(a % b);
}

int init_vsfs(char *name);

int mount_vsfs(char *name);

#endif