#include <sys/time.h>

#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_func.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

int main(int argc, char **argv) {
  int fd, ret = 0;
  struct timeval starttime, endtime;

  char *src = (char *)malloc(3333);

  memset(src, 'a', 3333);

  gettimeofday(&starttime, 0);

  vsfs_creat("test");

  fd = vsfs_open("test", O_RDWR);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  int limit = 1024 * 1024;
  for (int i = 1; i <= limit; i++) {
    // printf("==================================================\n");
    // printf("the <%d> term:\n", i);
    ret = vsfs_write(fd, src, 3333);
    if (ret == -1) {
      printf("ERR at read file\n");
      return -1;
    }
    // progress_bar(i, limit);
  }
  // vsfs_print_block_nbr(fd);
  ret = vsfs_close(fd);
  if (ret == -1) {
    printf("ERR at close file\n");
    return -1;
  }

  gettimeofday(&endtime, 0);
  double timeuse = 1000000 * (endtime.tv_sec - starttime.tv_sec) +
                   endtime.tv_usec - starttime.tv_usec;
  timeuse /= 1000;
  printf("total need %.3lf ms\n", timeuse);

  free(src);

  return 0;
}