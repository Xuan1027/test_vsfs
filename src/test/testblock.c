#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

int main(int argc, char **argv) {
  int fd, ret = 0;

  fd = vsfs_open("001", O_RDWR);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  vsfs_print_block_nbr(fd);
  
  ret = vsfs_close(fd);
  if (ret == -1) {
    printf("ERR at close file\n");
    return -1;
  }

  return 0;
}