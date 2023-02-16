#ifndef VSFSIO_H
#define VSFSIO_H

#include "vsfs.h"
#include "vsfs_stdinc.h"
#include "vsfs_shmfunc.h"
#include "vsfs_bitmap.h"

/**
 * create a new file 
 * \param shm_name the share memory name
*/
static int vsfs_creat(char* shm_name, char* file_name, int type){
  int fdc;
  char* shm_cache_name = strcat(shm_name, "_cached");

  struct vsfs_cached_data* ptr = (vsfs_cached_data*)shm_oandm(shm_cache_name, O_RDWR, PROT_READ | PROT_WRITE, &fdc);
  uint32_t ret = get_free_inode(ptr);

  int fd = shm_open(shm_name, O_RDWR, 0666);


  shm_close(ptr, &fdc);
}

// static int vsfs_open(char *pathname, int flags);

// static int vsfs_read(int fildes, void *buf, size_t nbyte);

// static int vsfs_write(int fildes, const void *buf, size_t nbyte);

// static int vsfs_close(int fildes);

// static off_t vsfs_lseek(int fd, off_t offset, int whence);

// static int vsfs_stat(const char *pathname, struct stat *st) {
//   memset(st, 0, sizeof(struct stat));

// }



#endif/*VSFSIO_H*/
