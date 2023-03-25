#include "vsfs_stdinc.h"

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("bad arg num\n");
    exit(EXIT_FAILURE);
  }

  shm_unlink(argv[1]);
  shm_unlink(strcat(argv[1], "_cached"));
  shm_unlink("optab");

  return 0;
}