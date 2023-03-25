#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

/**
 * get free inode first, then
 * added an new entry of / dir, and
 * setting the info inside the / inode --> atime, mtime
 * setting the info about the new inode
 */
int main(int argc, char** argv) {
  char* fname = (char*)malloc(VSFS_FILENAME_LEN);
  for (int i = 1; i <= 798; i++) {
    sprintf(fname, "%03d", i);
    int ret = vsfs_creat(fname);
    if (ret == -1) {
      return -1;
    }
  }
  free(fname);

  return 0;
}