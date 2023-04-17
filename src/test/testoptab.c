#include "vsfs.h"
#include "vsfs_shmfunc.h"

int main(int argc, char** argv) {
  int opfd;
  unsigned short* op_counter = (unsigned short*)shm_oandm(
      "optab", O_RDWR, PROT_READ | PROT_WRITE, &opfd);
  if (!op_counter) {
    printf("ERR: in vsfs_read(): open file table faild!\n");
    return -1;
  }
  op_ftable_t* op_ftable = (op_ftable_t*)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_read(): open file table faild!\n");
    return -1;
  }
  printf("the open file table have <%d> entry\n", *op_counter);

  printf("printing the open file table contain:\n");
  for (int i = 0; i < *op_counter; i++) {
    printf(
        "the <%d> entry:\n"
        "\toffset: %lu\n"
        "\tinode_nr: %u\n"
        "\tptr_counter: %u\n"
        "\tlock: %u\n",
        i, op_ftable[i].offset, op_ftable[i].inode_nr, op_ftable[i].ptr_counter,
        op_ftable[i].lock);
  }

  shm_close(op_counter, opfd);

  return 0;
}