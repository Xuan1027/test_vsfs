#include "../inc/vsfs.h"
#include "../inc/vsfs_bitmap.h"
#include "../inc/vsfs_stdinc.h"
#include "vsfsio_spdk.h"
#include "spdk.h"

/**
 * get free inode first, then
 * added an new entry of / dir, and
 * setting the info inside the / inode --> atime, mtime
 * setting the info about the new inode
 */
int main(int argc, char** argv) {
  init_spdk();

  char* fname = (char*)malloc(VSFS_FILENAME_LEN);
  // for (int i = 1; i <= 782; i++) {
  for (int i = 1; i <= 50; i++) {
    sprintf(fname, "%03d", i);
    int ret = vsfs_creat(fname, 0);
    if (ret == -1) {
      return -1;
    }
  }
  free(fname);

  exit_spdk();
  return 0;
}