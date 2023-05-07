#include "../inc/vsfs.h"
#include "../inc/vsfs_bitmap.h"
#include "../inc/vsfs_stdinc.h"
#include "vsfsio_spdk.h"
#include "spdk.h"
#include <stdio.h>
#include <stdlib.h>

int main(){
  init_spdk();

  int fd = vsfs_open("test", O_RDWR);

  char *writebuf = alloc_dma_buffer(13), *readbuf = alloc_dma_buffer(13);

  sprintf(writebuf, "hello world!");
  vsfs_lseek(fd, 0, SEEK_SET);
  vsfs_write(fd, writebuf, 13);

  vsfs_lseek(fd, 0, SEEK_SET);
  vsfs_read(fd, readbuf, 13);
  printf("%s\n", readbuf);

  vsfs_close(fd);

  free_dma_buffer(writebuf);
  free_dma_buffer(readbuf);

  exit_spdk();
  return 0;
}
