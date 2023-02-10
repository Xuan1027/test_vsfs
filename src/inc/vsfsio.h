#ifndef VSFSIO_H
#define VSFSIO_H

#include "vsfs_stdinc.h"
#include "vsfs.h"

static int vsfs_creat();

static int vsfs_open(char *pathname, int flags);

static int vsfs_read(int fildes, void *buf, size_t nbyte);

static int vsfs_write(int fildes, const void *buf, size_t nbyte);

static int vsfs_close(int fildes);

static off_t vsfs_lseek(int fd, off_t offset, int whence);

static int vsfs_stat(const char *pathname, struct stat *st) {
  memset(st, 0, sizeof(struct stat));

}



#endif/*VSFSIO_H*/