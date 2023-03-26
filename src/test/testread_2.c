#include <sys/time.h>

#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

int main(int argc, char** argv) {
  int fd, ret = 0;

  char* src = (char*)malloc(4096);

  struct timeval starttime, endtime;
  gettimeofday(&starttime, 0);

  fd = vsfs_open("001", O_RDWR);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  // long oft;
  // // oft = vsfs_lseek(fd, 0x1000000, SEEK_SET);
  // if(oft == -1){
  //   printf("ERR at open file\n");
  //   goto err;
  // }

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

  printf("Theres have %lu of char <a>\n", count);

  free(src);

  return 0;
}