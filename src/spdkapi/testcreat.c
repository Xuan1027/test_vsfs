#include "../inc/vsfs.h"
#include "../inc/vsfs_bitmap.h"
#include "../inc/vsfs_stdinc.h"
#include "vsfsio_spdk.h"
#include "spdk.h"

/**
 * get free inode first, then
 * added an new entry of / dir, and
 * setting the info inside the / inode --> atime, mtime
 * setting the info about the new inode
 */
int main(int argc, char** argv) {
  init_spdk();

  // char* fname = (char*)malloc(VSFS_FILENAME_LEN);
  // sprintf(fname,"test");
  int ret = vsfs_creat("test", 1024*1024);
  if(ret == -1){
    return ret;
    printf("already exist\n");
  }

  // for (int i = 1; i <= 782; i++) {
  // for (int i = 1; i <= 782; i++) {
  //   sprintf(fname, "%03d", i);
  //   int ret = vsfs_creat(fname, 0);
  //   if (ret == -1) {
  //     return -1;
  //   }
  // }
  int fd = vsfs_open("test", O_RDWR);

  // printf("fd = %d\n",fd);
  vsfs_print_block_nbr(fd);
  vsfs_close(fd);
  // free(fname);

  exit_spdk();
  return 0;
}
