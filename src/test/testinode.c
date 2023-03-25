#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("bad arg num\n");
    exit(EXIT_FAILURE);
  }
  file_stat_t* re = (file_stat_t*)malloc(sizeof(file_stat_t));
  int ret;

  ret = vsfs_stat(argv[1], re);
  if (ret != 0) {
    printf("err!\n");
    return -1;
  }
  printf(
      "inode of <%s>:\n"
      "\tmode = %s\n"
      "\tsize = %d\n"
      "\tblocks = %d\n"
      "\tctime = %s"
      "\tatime = %s"
      "\tmtime = %s",
      argv[1], re->mode, re->size, re->blocks, re->ctime, re->atime, re->mtime);

  free(re);

  return 0;
}