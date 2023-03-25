#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

int main(int argc, char** argv) {
  if (argc != 2) {
    printf("bad arg num\n");
    exit(EXIT_FAILURE);
  }
  int limit = atoi(argv[1]);
  char* fname = (char*)malloc(VSFS_FILENAME_LEN);
  int* fdtab = (int*)malloc(limit * sizeof(int));
  int ret = 0;

  for (int i = 1; i <= limit; i++) {
    sleep(2);
    sprintf(fname, "%03d", i);
    fdtab[i] = vsfs_open(fname, O_RDWR);
    printf("get the fd is: %d\n", fdtab[i]);
    if (fdtab[i] < 0) {
      printf("ERR!\n");
      return -1;
    }
  }

  for (int i = limit; i >= 1; i--) {
    ret = vsfs_close(fdtab[i]);
    if (ret != 0) {
      printf("ERR occur!\n");
      return -1;
    }
  }
  free(fname);
  free(fdtab);

  return 0;
}