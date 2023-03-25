#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

int main(int argc, char** argv) {
  int fd, ret = 0;

  char* src = (char*)malloc(555555);

  fd = vsfs_open("001", O_RDONLY);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  ret = vsfs_read(fd, src, 555555);
  if (ret == -1) {
    printf("ERR at read file\n");
    return -1;
  }
  printf("%s\n", src);

  ret = vsfs_close(fd);
  if (ret == -1) {
    printf("ERR at close file\n");
    return -1;
  }

  free(src);

  return 0;
}