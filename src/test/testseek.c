#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

uint64_t rand_uint64(void) {
  uint64_t r = 0;

  for (int i=0; i<64; i += 15 /*30*/) {
    r = r*((uint64_t)RAND_MAX + 1) + rand();
  }
  return r;
}


int main(int argc, char **argv) {
  int fd;
  int ret;

  file_stat_t* re = (file_stat_t*)malloc(sizeof(file_stat_t)); 
  vsfs_creat("test", 1048576);
  fd = vsfs_open("test", O_RDWR);

  for(int i=0;i<100;i++){
    uint64_t oft = rand_uint64();
    oft %= 0x100000000;
    vsfs_lseek(fd, oft, SEEK_SET);
    ret = vsfs_stat("test", re);
    if (ret != 0) {
      printf("err!\n");
      return -1;
    }
    printf(
        "inode of <%s>:\n"
        "\tmode = %s\n"
        "\tsize = %lu\n"
        "\tblocks = %u\n"
        "\tctime = %s"
        "\tatime = %s"
        "\tmtime = %s",
        "test", re->mode, re->size, re->blocks, re->ctime, re->atime, re->mtime);
  }

  free(re);
  vsfs_close(fd);

  return 0;
}