#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

int main(int argc, char **argv) {
  int fd, ret = 0;

  char *src = (char *)malloc(4096);

  FILE *fp = fopen("./src/test/testdata.txt", "r");

  vsfs_creat("test");

  fd = vsfs_open("test", O_RDWR);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  int i=0;
  int total_write = 0;
  int total_read = 0;
  while (fscanf(fp, "%[^\n]", src) != EOF) {
    total_read+=strlen(src);
    i++;
    printf("%s\n",src);
    ret = vsfs_write(fd, src, strlen(src));
    if (ret == -1) {
      printf("ERR at read file\n");
      return -1;
    }
    printf("write %d byte\n", ret);
    total_write+=ret;
    printf("total_write = %d\n", total_write);
    printf("total_read = %d\n", total_read);
    ret = fscanf(fp, "\n");
    printf("round %d end\n", i);
    // getchar();
  }
  // vsfs_print_block_nbr(fd);
  ret = vsfs_close(fd);
  if (ret == -1) {
    printf("ERR at close file\n");
    return -1;
  }

  free(src);

  fclose(fp);

  return 0;
}