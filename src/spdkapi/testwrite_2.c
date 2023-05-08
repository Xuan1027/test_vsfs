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
  double timeuse;
  struct timeval starttime, endtime;

  char *src = (char *)alloc_dma_buffer(VSFS_BLOCK_SIZE);

  memset(src, 'a', 4096);

  gettimeofday(&starttime, 0);
  // int limit = 1 * 1024 * 1024 / 8;
  int limit = 50*1024;

  vsfs_creat("test", limit);

  fd = vsfs_open("test", O_RDWR);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  for (int i = 1; i <= limit; i++) {
    if(i%1000==0)
      printf("round: %d\n", i);
    // switch(i){
    //   case 49:
    //   printf("L1 write finish\n");
    //   gettimeofday(&endtime, 0);
    //   timeuse = 1000000 * (endtime.tv_sec - starttime.tv_sec) +
    //                endtime.tv_usec - starttime.tv_usec;
    //   timeuse /= 1000;
    //   printf("total need %.3lf ms\n", timeuse);
    //   break;
    //   case 5*1024+49:
    //   printf("L2 write finish\n");
    //   gettimeofday(&endtime, 0);
    //   timeuse = 1000000 * (endtime.tv_sec - starttime.tv_sec) +
    //                   endtime.tv_usec - starttime.tv_usec;
    //   timeuse /= 1000;
    //   printf("total need %.3lf ms\n", timeuse);
    //   break;
    //   default:
    //   printf("\rwrite progress = %.2f%%",(float)i*100/limit);
    // }
    // printf("==================================================\n");
    // printf("the <%d> term:\n", i);
    ret = vsfs_write(fd, src, 4096);
    if (ret == -1) {
      printf("ERR at read file\n");
      return -1;
    }
    // progress_bar(i, limit);
  }
  printf("write finish\n");
  gettimeofday(&endtime, 0);
  timeuse = 1000000 * (endtime.tv_sec - starttime.tv_sec) +
                  endtime.tv_usec - starttime.tv_usec;
  timeuse /= 1000;
  printf("total need %.3lf ms\n", timeuse);

  printf("lseek!\n");
  vsfs_lseek(fd, 0, SEEK_SET);
  // vsfs_print_block_nbr(fd);
  uint64_t count = 0;
  while (vsfs_read(fd, src, 4096) != EOF) {
    // int i = count/4096;
    // switch(i){
    //   case 49:
    //   printf("L1 write finish\n");
    //   gettimeofday(&endtime, 0);
    //   timeuse = 1000000 * (endtime.tv_sec - starttime.tv_sec) +
    //                endtime.tv_usec - starttime.tv_usec;
    //   timeuse /= 1000;
    //   printf("total need %.3lf ms\n", timeuse);
    //   break;
    //   case 5*1024+49:
    //   printf("L2 write finish\n");
    //   gettimeofday(&endtime, 0);
    //   timeuse = 1000000 * (endtime.tv_sec - starttime.tv_sec) +
    //                endtime.tv_usec - starttime.tv_usec;
    //   timeuse /= 1000;
    //   printf("total need %.3lf ms\n", timeuse);
    //   break;
    //   default:
    //   printf("\rread progress = %.2f%%",(float)i*100/limit);
    // }

    for (int j = 0; j < 4096; j++) {
      if (src[j] == 'a')
        count++;
      else {
        printf("j = %d, char = <%c>(%d)\n", j, src[j],src[j]);
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
  timeuse = 1000000 * (endtime.tv_sec - starttime.tv_sec) +
                   endtime.tv_usec - starttime.tv_usec;
  timeuse /= 1000;
  printf("total need %.3lf ms\n", timeuse);

  free_dma_buffer(src);
  
  exit_spdk();
  return 0;
}
