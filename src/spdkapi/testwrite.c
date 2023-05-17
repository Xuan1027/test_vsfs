#include "../inc/vsfs.h"
#include "../inc/vsfs_bitmap.h"
#include "../inc/vsfs_stdinc.h"
#include "../inc/vsfsio_spdk.h"
#include "spdk.h"
#include <stdio.h>
#include <stdlib.h>

int main(){
  init_spdk();
  
  vsfs_creat("test", 10);
  int fd = vsfs_open("test", O_RDWR);

  char *writebuf = alloc_dma_buffer(13), *readbuf = alloc_dma_buffer(13);

  sprintf(writebuf, "hello world!");
  // vsfs_lseek(fd, 0, SEEK_SET);
  printf("writting to spdk:\n%s\n", writebuf);
  vsfs_write(fd, writebuf, 13);

  vsfs_lseek(fd, 0, SEEK_SET);
  printf("readed:\n");
  vsfs_read(fd, readbuf, 13);
  printf("%s\n", readbuf);

  vsfs_close(fd);

  // printf("close\n");

  free_dma_buffer(readbuf);
  // printf("free readbuf end\n");
  free_dma_buffer(writebuf);
  // printf("free writebuf end\n");

  exit_spdk();
  return 0;
}
