#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

int main(int argc, char** argv) {
  int fd, ret = 0;

  char* src = (char*)malloc(4097);

  fd = vsfs_open("001", O_RDWR);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  int count = 0;
  for (int i = 0; i < 5173; i++) {
    ret = vsfs_read(fd, src, 4096);
    if (ret == -1) {
      printf("ERR at read file\n");
      return -1;
    }
    for (int j = 0; j < 4096; j++) {
      if (src[j] == 'a')
        count++;
      else
        printf("i = %d, j = %d, char = <%c>\n", i, j, src[j]);
    }
  }
  ret = vsfs_close(fd);
  if (ret == -1) {
    printf("ERR at close file\n");
    return -1;
  }

  printf("Theres have %d of char <a>\n", count);

  free(src);

  return 0;
}