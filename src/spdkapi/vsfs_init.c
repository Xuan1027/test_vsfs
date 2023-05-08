#include "vsfs_init.h"

#include "../inc/vsfs.h"
#include "spdk.h"

static struct superblock *write_superblock(unsigned long *lba_cnt,
                                           unsigned block_size,
                                           unsigned long device_size) {
  struct superblock *sb = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  unsigned long st_size = block_size * device_size;
  if (!sb)
    return NULL;

  uint32_t nr_blocks = st_size / VSFS_BLOCK_SIZE;
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
  int ret = write_spdk(sb, *lba_cnt, 1, IO_QUEUE);
  (*lba_cnt) += (1);

  if (ret != 0) {
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

static int write_inode_region(unsigned long *lba_cnt, struct superblock *sb) {
  char *block = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  if (!block)
    return -1;
  memset(block, 0, VSFS_BLOCK_SIZE);
  struct vsfs_inode *inode = (struct vsfs_inode *)block;
  inode->mode = htole32(0x0f); // drwx
  inode->blocks = htole32(1);
  inode->atime = inode->ctime = inode->mtime = htole32(0);
  inode->entry = htole32(2);
  inode->l1[0] = htole32(0);

  int ret = write_spdk(block, *lba_cnt, 1, IO_QUEUE);
  (*lba_cnt) += (1);

  if (ret != 0) {
    goto end;
  }

  uint32_t count;
  memset(block, 0, VSFS_BLOCK_SIZE);
  for (count = 1; count < 2048; count++) {
    ret = write_spdk(block, *lba_cnt, 1, IO_QUEUE);
    (*lba_cnt) += (1);

    if (ret != 0)
      goto end;
  }

  printf(
      "Inode region:\n"
      "\tinode size = %ld B\n"
      "\twrote %u blocks\n",
      sizeof(struct vsfs_inode), count);
end:
  free_dma_buffer(block);
  return ret;
}

static int write_inode_bitmap(unsigned long *lba_cnt, struct superblock *sb) {
  char *block = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  if (!block)
    return -1;

  uint64_t *ibitmap = (uint64_t *)block;

  /* Set all bits to 1 */
  memset(ibitmap, 0xff, VSFS_BLOCK_SIZE);
  /* First inode */
  ibitmap[0] = htole64(0xfffffffffffffffe);

  int ret = write_spdk(block, *lba_cnt, 1, IO_QUEUE);
  (*lba_cnt) += (1);
  free_dma_buffer(block);
  return ret;
}

static int write_data_bitmap(unsigned long *lba_cnt, struct superblock *sb) {
  char *block = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  if (!block)
    return -1;

  uint64_t *dbitmap = (uint64_t *)block;

  /* Set all bits to 1 */
  memset(dbitmap, 0xff, VSFS_BLOCK_SIZE);
  /* First data block */
  uint32_t ret;
  dbitmap[0] = htole64(0xfffffffffffffffe);
  ret = write_spdk(dbitmap, *lba_cnt, 1, IO_QUEUE);
  (*lba_cnt) += (1);
  if (ret != 0) {
    goto end;
  }
  dbitmap[0] = htole64(0xffffffffffffffff);
  uint32_t i;
  for (i = 1; i < le32toh(sb->info.nr_dbitmap_blocks); i++) {
    ret = write_spdk(dbitmap, *lba_cnt, 1, IO_QUEUE);
    (*lba_cnt) += (1);
    if (ret != 0) {
      goto end;
    }
  }
  ret = 0;
  printf(
      "Data bitmap:\n"
      "\twrote %d blocks\n",
      i);

end:
  free_dma_buffer(block);

  return ret;
}

static int write_data_region(unsigned long *lba_cnt, struct superblock *sb) {
  char *block = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  if (!block)
    return -1;

  struct vsfs_dir_block *dblock = (struct vsfs_dir_block *)block;
  dblock->files[0].inode = 0;
  strncpy(dblock->files[0].filename, ".\0", 2);
  dblock->files[1].inode = 0;
  strncpy(dblock->files[1].filename, "..\0", 3);

  int ret =
      write_spdk(dblock, *lba_cnt, 1, IO_QUEUE);
  (*lba_cnt) += (1);

  if (ret != 0) {
    goto end;
  }

  printf(
      "Data region:\n"
      "\twrote 1 blocks\n");
end:
  free_dma_buffer(block);
  return ret;
}

int init_vsfs(char *name) {
  int ret;
  unsigned block_size;
  unsigned long device_size;
  unsigned long lba_cnt = 0;
  get_device_info(&block_size, &device_size);
  printf(
      "block_size=%d\n"
      "device_size=%ld\n",
      block_size, device_size);

  struct superblock *sb = write_superblock(&lba_cnt, block_size, device_size);
  if (!sb) {
    handle_error("write_superblock():");
    goto free_sb;
  }

  ret = write_inode_bitmap(&lba_cnt, sb);
  if (ret) {
    handle_error("write_inode_bitmap");
    goto free_sb;
  }

  ret = write_inode_region(&lba_cnt, sb);
  if (ret) {
    handle_error("write_inode_region");
    goto free_sb;
  }

  ret = write_data_bitmap(&lba_cnt, sb);
  if (ret) {
    handle_error("write_data_bitmap");
    goto free_sb;
  }

  ret = write_data_region(&lba_cnt, sb);
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
  free_dma_buffer(sb);

  return ret;
}

static int make_shm_cached(char *name, char *ptr) {
  struct superblock *sb = (struct superblock *)ptr;
  char *ibitmap =
      alloc_dma_buffer(VSFS_BLOCK_SIZE * sb->info.nr_ibitmap_blocks);
  char *dbitmap =
      alloc_dma_buffer(VSFS_BLOCK_SIZE * sb->info.nr_dbitmap_blocks);
  if (!ibitmap || !dbitmap)
    goto free_dma;

  int ret = 0;
  ret = read_spdk(ibitmap, sb->info.ofs_ibitmap,
                  sb->info.nr_ibitmap_blocks,
                  IO_QUEUE);
  if (ret != 0) {
    handle_error("read_spdk():");
    goto free_dma;
  }
  ret = read_spdk(dbitmap, sb->info.ofs_dbitmap,
                  sb->info.nr_dbitmap_blocks,
                  IO_QUEUE);
  if (ret != 0) {
    handle_error("read_spdk():");
    goto free_dma;
  }

  int s_len = strlen(name);
  char *cached = malloc(s_len + 8);
  if (!cached) {
    handle_error("malloc():");
    goto free_dma;
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
free_dma:
  free_dma_buffer(ibitmap);
  free_dma_buffer(dbitmap);
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

int mount_vsfs(char *name) {
  int ret = 0;
  char *ptr = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  read_spdk(ptr, 0, 1, IO_QUEUE);

  struct superblock *sb = (struct superblock *)ptr;
  // check magic number
  if (sb->info.magic != VSFS_MAGIC) {
    handle_error("magic number unmatehed.");
    goto free;
  }

  if (make_shm_cached(name, ptr) != 0) {
    goto free;
  }

  if (creat_open_file_table() < 0)
    handle_error("creat_open_file_table():");

  return 0;
free:
  free_dma_buffer(ptr);
  return ret;
}