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

#include "vsfs.h"

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

/* Returns ceil(a/b) */
static inline uint32_t idiv_ceil(uint32_t a, uint32_t b) {
  return (a / b) + !!(a % b);
}

static struct superblock *write_superblock(int fd, struct stat *fstats) {
  struct superblock *sb = malloc(sizeof(struct superblock));
  if (!sb)
    return NULL;
  uint32_t nr_blocks = fstats->st_size / VSFS_BLOCK_SIZE;
  uint32_t nr_ibitmap_blocks = 1;
  uint32_t nr_iregion_blocks =
      VSFS_NR_INODES / (VSFS_BLOCK_SIZE / sizeof(struct vsfs_inode));
  uint32_t remain = nr_blocks - nr_ibitmap_blocks - nr_iregion_blocks - 1;
  uint32_t mod = idiv_ceil(remain, 4096 << 3);
  uint32_t nr_dbitmap_blocks = mod;
  uint32_t nr_dregion_blocks = remain - mod;
  uint32_t ofs_ibitmap = 1;
  uint32_t ofs_iregion = 2;
  uint32_t ofs_dbitmap = ofs_iregion + nr_iregion_blocks;
  uint32_t ofs_dregion = ofs_dbitmap + nr_dbitmap_blocks;
  memset(sb, 0, sizeof(struct superblock));
  sb->info =
      (struct vsfs_sb_info){.magic = htole32(VSFS_MAGIC),
                            .nr_blocks = htole32(nr_blocks),
                            .ofs_ibitmap = htole32(ofs_ibitmap),
                            .ofs_iregion = htole32(ofs_iregion),
                            .ofs_dbitmap = htole32(ofs_dbitmap),
                            .ofs_dregion = htole32(ofs_dregion),
                            .nr_ibitmap_blocks = htole32(nr_ibitmap_blocks),
                            .nr_iregion_blocks = htole32(nr_iregion_blocks),
                            .nr_dbitmap_blocks = htole32(nr_dbitmap_blocks),
                            .nr_dregion_blocks = htole32(nr_dregion_blocks),
                            .nr_free_inodes = VSFS_NR_INODES - 1,
                            .nr_free_dblock = nr_dregion_blocks - 1};
  int ret = write(fd, sb, sizeof(struct superblock));
  if (ret != sizeof(struct superblock)) {
    free(sb);
    return NULL;
  }
  printf("Superblock: (%ld)\n"
         "\tmagic=%#x\n"
         "\tnr_blocks=%u\n"
         "\tnr_ibitmap_blocks=%u\n"
         "\tnr_iregion_blocks=%u\n"
         "\tnr_dbitmap_blocks=%u\n"
         "\tnr_dregion_blocks=%u\n",
         sizeof(struct superblock), sb->info.magic, sb->info.nr_blocks,
         sb->info.nr_ibitmap_blocks, sb->info.nr_iregion_blocks,
         sb->info.nr_dbitmap_blocks, sb->info.nr_dregion_blocks);
  return sb;
}

static int write_inode_region(int fd, struct superblock *sb) {
  char *block = malloc(VSFS_BLOCK_SIZE);
  if (!block)
    return -1;
  memset(block, 0, VSFS_BLOCK_SIZE);
  struct vsfs_inode *inode = (struct vsfs_inode *)block;
  uint32_t first_data_block = le32toh(sb->info.ofs_dregion);
  inode->mode = htole32(0x0f); // drwx
  inode->blocks = htole32(1);
  inode->atime = inode->ctime = inode->mtime = htole32(0);
  inode->entry = htole32(2);
  inode->block[0] = htole32(0);

  int ret = write(fd, block, VSFS_BLOCK_SIZE);
  if (ret != VSFS_BLOCK_SIZE) {
    goto end;
  }

  uint32_t count;
  memset(block, 0, VSFS_BLOCK_SIZE);
  for (count = 1; count < 2048; count++) {
    ret = write(fd, block, VSFS_BLOCK_SIZE);
    if (ret != VSFS_BLOCK_SIZE)
      goto end;
  }

  printf("Inode region:\n"
         "\tinode size = %ld B\n"
         "\twrote %u blocks\n",
         sizeof(struct vsfs_inode), count);
  free(block);
  return 0;
end:
  free(block);
  return -1;
}

static int write_inode_bitmap(int fd, struct superblock *sb) {
  char *block = malloc(VSFS_BLOCK_SIZE);
  if (!block)
    return -1;

  uint64_t *ibitmap = (uint64_t *)block;

  /* First inode */
  ibitmap[0] = htole64(0xfe);

  int ret = write(fd, ibitmap, VSFS_BLOCK_SIZE);
  if (ret != VSFS_BLOCK_SIZE) {
    free(block);
    return -1;
  }
  free(block);
  return 0;
}

static int write_data_bitmap(int fd, struct superblock *sb) {
  char *block = malloc(VSFS_BLOCK_SIZE);
  if (!block)
    return -1;

  uint64_t *dbitmap = (uint64_t *)block;

  /* Set all bits to 0 */
  memset(dbitmap, 0xff, VSFS_BLOCK_SIZE);

  uint32_t i, ret = 0;
  for (i = 0; i < le32toh(sb->info.nr_dbitmap_blocks); i++) {
    ret = write(fd, dbitmap, VSFS_BLOCK_SIZE);
    if (ret != VSFS_BLOCK_SIZE) {
      ret = -1;
      goto end;
    }
  }
  ret = 0;
  printf("Data bitmap:\n"
         "\twrote %d blocks\n",
         i);

end:
  free(block);

  return ret;
}

static int write_data_region(int fd, struct superblock *sb) {
  char *block = malloc(VSFS_BLOCK_SIZE);
  if (!block)
    return -1;

  struct vsfs_dir_block *dblock = (struct vsfs_dir_block *)block;
  dblock->files[0].inode = 0;
  strncpy(dblock->files[0].filename, ".\0", 2);
  dblock->files[1].inode = 0;
  strncpy(dblock->files[1].filename, "..\0", 3);

  int ret = write(fd, dblock, VSFS_BLOCK_SIZE);

  if (ret != VSFS_BLOCK_SIZE) {
    free(block);
    return -1;
  }

  printf("Data region:\n"
         "\twrote 1 blocks\n");
  free(block);
  return 0;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("bad arg num\n");
    exit(EXIT_FAILURE);
  }
  char *name = argv[1];
  int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
  int ret;
  if (fd == -1) {
    handle_error("shm_open():");
    return ret;
  }

  ftruncate(fd, 0x100000 << 10);
  struct stat fstats;
  ret = fstat(fd, &fstats);
  if (ret) {
    handle_error("fstat():");
    return ret;
  }

  printf("fstats->st_size=0x%lx\n", fstats.st_size);

  struct superblock *sb = write_superblock(fd, &fstats);
  if (!sb) {
    handle_error("write_superblock():");
    goto fclose;
  }

  ret = write_inode_bitmap(fd, sb);
  if (ret) {
    handle_error("write_inode_bitmap");
    goto free_sb;
  }

  ret = write_inode_region(fd, sb);
  if (ret) {
    handle_error("write_inode_region");
    goto free_sb;
  }

  ret = write_data_bitmap(fd, sb);
  if (ret) {
    handle_error("write_data_bitmap");
    goto free_sb;
  }

  ret = write_data_region(fd, sb);
  if (ret) {
    handle_error("write_data_region");
    goto free_sb;
  }

  printf("sizeof(superblock) = %ld\n"
         "sizeof(vsfs_sb_info) = %ld\n"
         "sizeof(vsfs_inode) = %ld\n"
         "sizeof(vsfs_dir_block) = %ld\n",
         sizeof(struct superblock), sizeof(struct vsfs_sb_info),
         sizeof(struct vsfs_inode), sizeof(struct vsfs_dir_block));

free_sb:
  free(sb);
fclose:
  close(fd);

  return ret;
}
