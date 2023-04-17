#include "vsfs.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

#define handle_error(msg) \
  do {                    \
    perror(msg);          \
    ret = EXIT_FAILURE;   \
  } while (0)

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
  printf(
      "Superblock: (%ld)\n"
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
  inode->mode = htole32(0x0f); // drwx
  inode->blocks = htole32(1);
  inode->atime = inode->ctime = inode->mtime = htole32(0);
  inode->entry = htole32(2);
  inode->l1[0] = htole32(0);

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

  printf(
      "Inode region:\n"
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

  /* Set all bits to 1 */
  memset(ibitmap, 0xff, VSFS_BLOCK_SIZE);
  /* First inode */
  ibitmap[0] = htole64(0xfffffffffffffffe);

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

  /* Set all bits to 1 */
  memset(dbitmap, 0xff, VSFS_BLOCK_SIZE);
  /* First data block */
  uint32_t ret;
  dbitmap[0] = htole64(0xfffffffffffffffe);
  ret = write(fd, dbitmap, VSFS_BLOCK_SIZE);
  if (ret != VSFS_BLOCK_SIZE) {
    ret = -1;
    goto end;
  }
  dbitmap[0] = htole64(0xffffffffffffffff);
  uint32_t i;
  for (i = 1; i < le32toh(sb->info.nr_dbitmap_blocks); i++) {
    ret = write(fd, dbitmap, VSFS_BLOCK_SIZE);
    if (ret != VSFS_BLOCK_SIZE) {
      ret = -1;
      goto end;
    }
  }
  ret = 0;
  printf(
      "Data bitmap:\n"
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

  printf(
      "Data region:\n"
      "\twrote 1 blocks\n");
  free(block);
  return 0;
}

static int __attribute__((unused)) init_vsfs(char *name) {
  int fd = shm_open(name, O_CREAT | O_RDWR, 0666);
  int ret;
  if (fd == -1) {
    handle_error("shm_open():");
    return ret;
  }

  if (ftruncate(fd, 0x140000000) == -1) {
    handle_error("ftruncate():");
    goto fclose;
  }
  struct stat fstats;
  ret = fstat(fd, &fstats);
  if (ret) {
    handle_error("fstat():");
    goto fclose;
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

  printf(
      "sizeof(superblock) = %ld\n"
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

static int make_shm_cached(char *name, char *ptr) {
  struct superblock *sb = (struct superblock *)ptr;
  char *ibitmap = ptr + (sb->info.ofs_ibitmap * VSFS_BLOCK_SIZE);
  char *dbitmap = ptr + (sb->info.ofs_dbitmap * VSFS_BLOCK_SIZE);

  int ret = 0;
  int s_len = strlen(name);
  char *cached = malloc(s_len + 8);
  if (!cached) {
    handle_error("malloc():");
    goto end;
  }
  memcpy(cached, name, s_len);
  strncpy(cached + s_len, "_cached", 8);

  int cfd = shm_open(cached, O_CREAT | O_RDWR, 0666);
  if (cfd == -1) {
    handle_error("shm_open():");
    goto free_str;
  }

  // sizeof(inode bitmap + data bitmap)
  uint32_t bitmap_total_size =
      (sb->info.nr_ibitmap_blocks + sb->info.nr_dbitmap_blocks) *
      VSFS_BLOCK_SIZE;

  if (ftruncate(cfd, sizeof(struct vsfs_sb_info) + bitmap_total_size) == -1) {
    handle_error("ftruncate():");
    goto free_str;
  }
  char *cptr = mmap(NULL, sizeof(struct vsfs_sb_info) + bitmap_total_size,
                    PROT_READ | PROT_WRITE, MAP_SHARED, cfd, 0);
  if (cptr == MAP_FAILED) {
    handle_error("mmap():");
    goto free_str;
  }

  struct vsfs_sb_info *cached_sb = (struct vsfs_sb_info *)cptr;
  char *cached_ibitmap = cptr + sizeof(struct vsfs_sb_info);
  char *cached_dbitmap = cptr + sizeof(struct vsfs_sb_info) +
                         (sb->info.nr_ibitmap_blocks * VSFS_BLOCK_SIZE);

  memcpy(cached_sb, sb, sizeof(struct vsfs_sb_info));
  memcpy(cached_ibitmap, ibitmap, sb->info.nr_ibitmap_blocks * VSFS_BLOCK_SIZE);
  memcpy(cached_dbitmap, dbitmap, sb->info.nr_dbitmap_blocks * VSFS_BLOCK_SIZE);
  munmap(cptr, sizeof(struct vsfs_sb_info) + bitmap_total_size);
free_str:
  free(cached);
end:
  return ret;
}

static int init_spdk_daemon(void) {
  int ret = 0;
  // todo:
  return ret;
}

static int creat_open_file_table(void) {
  // open file table test
  int opfd = shm_open("optab", O_CREAT | O_RDWR, 0666);
  if (opfd < 0)
    return opfd;

  if (ftruncate(opfd, VSFS_OPTAB_SIZE + sizeof(unsigned short)) == -1) {
    close(opfd);
    printf("ftruncate open table size ERR!\n");
    return -1;
  }

  op_ftable_t *init = (op_ftable_t *)malloc(VSFS_OPTAB_SIZE);
  memset(init, 0, VSFS_OPTAB_SIZE);

  unsigned short count = 0;

  int ret = write(opfd, &count, sizeof(unsigned short));
  if (ret != sizeof(unsigned short)) {
    free(init);
    close(opfd);
    printf("init the op_ftable entry counter ERR!\n");
    return -1;
  }
  ret = write(opfd, init, VSFS_OPTAB_SIZE);
  if (ret != VSFS_OPTAB_SIZE) {
    free(init);
    close(opfd);
    printf("init the op_ftable entry ERR!\n");
    return -1;
  }

  close(opfd);

  return 0;
}

static int __attribute__((unused)) mount_vsfs(char *name) {
  int fd = shm_open(name, O_CREAT | O_RDWR, 0666);

  struct stat fstats;
  int ret = fstat(fd, &fstats);
  if (ret) {
    handle_error("fstat():");
    goto fclose;
  }

  char *ptr;
  ptr = mmap(NULL, fstats.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  struct superblock *sb = (struct superblock *)ptr;
  // check magic number
  if (sb->info.magic != VSFS_MAGIC) {
    handle_error("magic number unmatehed.");
    goto unmap;
  }

  if (make_shm_cached(name, ptr) != 0) {
    goto unmap;
  }

  if (creat_open_file_table() < 0)
    handle_error("creat_open_file_table():");

  ret = init_spdk_daemon();
  if (ret == -1) {
    handle_error("init_spdk_daemon():");
    return ret;
  }

  return 0;
unmap:
  munmap(ptr, fstats.st_size);
fclose:
  close(fd);
  return ret;
}

static int __attribute__((unused)) vsfs_prewrite(void) {
  int fd, ret = 0;
  struct timeval starttime, endtime;

  char *src = (char *)malloc(4096);

  memset(src, 'a', 4096);

  gettimeofday(&starttime, 0);

  vsfs_creat("test");

  fd = vsfs_open("test", O_RDWR);
  if (fd == -1) {
    printf("ERR at open file\n");
    return -1;
  }
  int limit = 1024 * 1024;
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
  // vsfs_print_block_nbr(fd);
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

  return 0;
}

