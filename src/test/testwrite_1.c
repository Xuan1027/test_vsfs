#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

int main(int argc, char **argv) {
  int fd, ret = 0;

  char *src = (char *)malloc(555555);

  FILE *fp = fopen("./src/test/testdata.txt", "r");
  ret = fscanf(fp, "%s", src);

  // printf("src[5000] = %c\n", src[5000]);

  fd = vsfs_open("001", O_RDWR);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  for (int i = 1; i <= 30; i++) {
    printf("the <%d> term:\n", i);
    ret = vsfs_write(fd, src, 555555);
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