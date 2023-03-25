#ifndef VSFS_SHMFUNC_H
#define VSFS_SHMFUNC_H

#include "vsfs_stdinc.h"

/**
 * open exist shm and mmap it,
 * the default of the shm_open mode is 0666,
 * the default of the mmap addr is NULL,
 * the default of the mmap flag is MAP_SHARED,
 * the default of the mmap offset is 0
 *
 * \param name is the name of shm
 * \param oflag is the Oflag of shm_open
 * \param msize the size of the mmap
 * \param mprot is the mmap prot
 * \param fd
 *
 * \return NULL for faild
 */
static void* shm_oandm(char* name, int oflag, int mprot, int* fd) {
  *fd = shm_open(name, oflag, 0666);
  void* ret = NULL;
  int re = 0;
  struct stat fstats;
  if (*fd < 0) {
    printf("open shm <%s> failed: %s\n", name, strerror(errno));
    goto faild_ret;
  }
  re = fstat(*fd, &fstats);
  if (re < 0) {
    printf("fstat shm <%s> failed: %s\n", name, strerror(errno));
    goto close_ret;
  }
  ret = (void*)mmap(NULL, fstats.st_size, mprot, MAP_SHARED, *fd, 0);
  if (ret == MAP_FAILED) {
    printf("mmap shm <%s> failed: %s\n", name, strerror(errno));
    ret = NULL;
    goto close_ret;
  }

  return ret;
close_ret:
  close(*fd);
faild_ret:
  return ret;
}

static void shm_close(void* ptr, int* fd) {
  int re;
  struct stat fstats;
  re = fstat(*fd, &fstats);
  if (re < 0) {
    printf("fstat shm in shm_close failed: %s\n", strerror(errno));
  }
  re = munmap(ptr, fstats.st_size);
  if (re < 0) {
    printf("munmap shm in shm_close failed: %s\n", strerror(errno));
  }
  re = close(*fd);
  if (re < 0) {
    printf("close shm in shm_close failed: %s\n", strerror(errno));
  }

  return;
}

#endif