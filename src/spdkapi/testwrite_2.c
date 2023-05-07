#include <stdio.h>
#include <sys/time.h>

#include "spdk.h"
#include "../inc/vsfs.h"
#include "../inc/vsfs_bitmap.h"
#include "../inc/vsfs_func.h"
#include "../inc/vsfs_stdinc.h"
#include "vsfsio_spdk.h"

int main(int argc, char **argv) {
  init_spdk();
  int fd, ret = 0;
  struct timeval starttime, endtime;

  char *src = (char *)malloc(4096);

  memset(src, 'a', 4096);

  gettimeofday(&starttime, 0);

  vsfs_creat("test", 0);

  fd = vsfs_open("test", O_RDWR);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  int limit = 6 * 1024;
  for (int i = 1; i <= limit; i++) {
    // printf("==================================================\n");
    // printf("the <%d> term:\n", i);
    ret = vsfs_write(fd, src, 4096);
    if (ret == -1) {
      printf("ERR at read file\n");
      return -1;
    }
    // progress_bar(i, limit);
  }
  vsfs_lseek(fd, 0, SEEK_SET);
  // vsfs_print_block_nbr(fd);
  uint64_t count = 0;
  while (vsfs_read(fd, src, 4096) != EOF) {
    for (int j = 0; j < 4096; j++) {
      if (src[j] == 'a')
        count++;
      else {
        printf("j = %d, char = <%c>\n", j, src[j]);
        PAUSE;
      }
    }
  }
  printf("count = %lu\n", count);
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
  
  exit_spdk();
  return 0;
}
