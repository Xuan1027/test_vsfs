#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

int main(int argc, char **argv) {
  int fd, ret = 0;

  char *src = (char *)malloc(4097);

  FILE *fp = fopen("./src/test/testdata2.txt", "r");
  ret = fscanf(fp, "%s", src);

  int count = 0;
  for (int j = 0; j < 4096; j++) {
    if (src[j] == 'a')
      count++;
    else
      printf("j = %d, char = <%c>\n", j, src[j]);
  }

  printf("count = %d\n", count);

  fd = vsfs_open("001", O_RDWR);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  for (int i = 1; i <= 5173; i++) {
    printf("the <%d> term:\n", i);
    ret = vsfs_write(fd, src, 4096);
    if (ret == -1) {
      printf("ERR at read file\n");
      return -1;
    }
  }
  vsfs_print_block_nbr(fd);
  ret = vsfs_close(fd);
  if (ret == -1) {
    printf("ERR at close file\n");
    return -1;
  }

  free(src);

  fclose(fp);

  return 0;
}