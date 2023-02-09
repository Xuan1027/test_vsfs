#include <fcntl.h>
#include <linux/fs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "VSFS/vsfs.h"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    ret = EXIT_FAILURE;                                                        \
  } while (0)

struct superblock {
  struct vsfs_sb_info info;
  /* Padding to match block size */
  char padding[4096 - sizeof(struct vsfs_sb_info)];
};

int main(int argc, char *argv[]) {
  int fd = shm_open("test", O_CREAT | O_RDWR, 0666);

  struct stat fstats;
  int ret = fstat(fd, &fstats);
  if (ret) {
    handle_error("fstat():");
    return ret;
  }

  char *ptr;
  ptr = mmap(NULL, fstats.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  struct superblock *sb = (struct superblock *)ptr;
  printf("Superblock: (%ld)\n"
         "\tmagic=%#x\n"
         "\tnr_blocks=%u\n"
         "\tnr_ibitmap_blocks=%u\n"
         "\tnr_iregion_blocks=%u\n"
         "\tnr_dbitmap_blocks=%u\n"
         "\tnr_dregion_blocks=%u\n"
         "\tofs_ibitmap=%u\n"
         "\tofs_iregion=%u\n"
         "\tofs_dbitmap=%u\n"
         "\tofs_dregion=%u\n",
         sizeof(struct superblock), sb->info.magic, sb->info.nr_blocks,
         sb->info.nr_ibitmap_blocks, sb->info.nr_iregion_blocks,
         sb->info.nr_dbitmap_blocks, sb->info.nr_dregion_blocks,
         sb->info.ofs_ibitmap, sb->info.ofs_iregion, sb->info.ofs_dbitmap,
         sb->info.ofs_dregion);
  struct vsfs_inode *root_inode =
      (struct vsfs_inode *)&ptr[2 * VSFS_BLOCK_SIZE];

  printf("root inode:\n"
         "\tmode: %x\n"
         "\tblocks: %u\n"
         "\tsize: %u\n"
         "\tatime: %lu\n"
         "\tctime: %lu\n"
         "\tmtime: %lu\n",
         root_inode->mode, root_inode->blocks, root_inode->size,
         root_inode->atime, root_inode->ctime, root_inode->mtime);
  struct vsfs_dir_block *root_dir_info =
      (struct vsfs_dir_block
           *)&ptr[(sb->info.ofs_dregion + root_inode->block[0]) *
                  VSFS_BLOCK_SIZE];
  int i;
  printf("\\\n");
  for(i = 0;i<root_inode->entry;i++){
    printf("\t%s\tinode:%u\n",
            root_dir_info->files[i].filename,
            root_dir_info->files[i].inode
            );
  }
  printf("nr_inode/64=%ld\n",VSFS_NR_INODES/64);
  return 0;
}
