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
  char* fname = (char*)malloc(VSFS_FILENAME_LEN);
  int ret;

  int limit = atoi(argv[1]);
  for (int i = 1; i <= limit; i++) {
    sprintf(fname, "%03d", i);
    ret = vsfs_stat(fname, re);
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
        fname, re->mode, re->size, re->blocks, re->ctime, re->atime, re->mtime);
  }

  free(fname);
  free(re);

  return 0;
}