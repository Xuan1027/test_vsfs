#include "vsfs_init.h"

#include "spdk.h"
#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"

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

  free_dma_buffer(block);
  block = alloc_dma_buffer((sb->info.nr_iregion_blocks - 1) * VSFS_BLOCK_SIZE);
  memset(block, 0, (sb->info.nr_iregion_blocks - 1) * VSFS_BLOCK_SIZE);
  ret = write_spdk(block, *lba_cnt, sb->info.nr_iregion_blocks - 1, IO_QUEUE);
  (*lba_cnt) += (sb->info.nr_iregion_blocks - 1);

  if (ret != 0)
    goto end;

  printf(
      "Inode region:\n"
      "\tinode size = %ld B\n"
      "\twrote %u blocks\n",
      sizeof(struct vsfs_inode), sb->info.nr_iregion_blocks);
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
  free_dma_buffer(block);
  block = alloc_dma_buffer((sb->info.nr_dbitmap_blocks - 1) * VSFS_BLOCK_SIZE);
  dbitmap = (uint64_t *)block;
  memset(dbitmap, 0xff, (sb->info.nr_dbitmap_blocks - 1) * VSFS_BLOCK_SIZE);
  ret = write_spdk(dbitmap, *lba_cnt, sb->info.nr_dbitmap_blocks - 1, IO_QUEUE);
  (*lba_cnt) += (sb->info.nr_dbitmap_blocks - 1);
  if (ret != 0) {
    goto end;
  }
  ret = 0;
  printf(
      "Data bitmap:\n"
      "\twrote %d blocks\n",
      sb->info.nr_dbitmap_blocks);

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

  int ret = write_spdk(dblock, *lba_cnt, 1, IO_QUEUE);
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
  int ret = -1;
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
  int ret = -1;
  struct superblock *sb = (struct superblock *)ptr;
  char *ibitmap =
      alloc_dma_buffer(VSFS_BLOCK_SIZE * sb->info.nr_ibitmap_blocks);
  char *dbitmap =
      alloc_dma_buffer(VSFS_BLOCK_SIZE * sb->info.nr_dbitmap_blocks);
  struct vsfs_inode *inode_reg =
      alloc_dma_buffer(VSFS_BLOCK_SIZE * sb->info.nr_iregion_blocks);
  if (!ibitmap || !dbitmap || !inode_reg)
    goto free_dma;

  ret = read_spdk(ibitmap, sb->info.ofs_ibitmap, sb->info.nr_ibitmap_blocks,
                  IO_QUEUE);
  if (ret != 0) {
    handle_error("read_spdk():");
    goto free_dma;
  }
  ret = read_spdk(dbitmap, sb->info.ofs_dbitmap, sb->info.nr_dbitmap_blocks,
                  IO_QUEUE);
  if (ret != 0) {
    handle_error("read_spdk():");
    goto free_dma;
  }

  ret = read_spdk(inode_reg, sb->info.ofs_iregion, sb->info.nr_iregion_blocks,
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
  uint32_t total_size =
      (sb->info.nr_ibitmap_blocks + sb->info.nr_dbitmap_blocks +
       sb->info.nr_iregion_blocks) *
      VSFS_BLOCK_SIZE;

  if (ftruncate(cfd, sizeof(struct vsfs_sb_info) + total_size) == -1) {
    handle_error("ftruncate():");
    goto free_str;
  }
  char *cptr = mmap(NULL, sizeof(struct vsfs_sb_info) + total_size,
                    PROT_READ | PROT_WRITE, MAP_SHARED, cfd, 0);
  if (cptr == MAP_FAILED) {
    handle_error("mmap():");
    goto free_str;
  }

  struct vsfs_sb_info *cached_sb = (struct vsfs_sb_info *)cptr;
  char *cached_ibitmap = cptr + sizeof(struct vsfs_sb_info);
  char *cached_iregion = cptr + sizeof(struct vsfs_sb_info) +
                         (sb->info.nr_ibitmap_blocks * VSFS_BLOCK_SIZE);
  char *cached_dbitmap = cptr + sizeof(struct vsfs_sb_info) +
                         (sb->info.nr_ibitmap_blocks * VSFS_BLOCK_SIZE) +
                         (sb->info.nr_iregion_blocks * VSFS_BLOCK_SIZE);

  memcpy(cached_sb, sb, sizeof(struct vsfs_sb_info));
  memcpy(cached_ibitmap, ibitmap, sb->info.nr_ibitmap_blocks * VSFS_BLOCK_SIZE);
  memcpy(cached_iregion, inode_reg,
         sb->info.nr_iregion_blocks * VSFS_BLOCK_SIZE);
  memcpy(cached_dbitmap, dbitmap, sb->info.nr_dbitmap_blocks * VSFS_BLOCK_SIZE);
  munmap(cptr, sizeof(struct vsfs_sb_info) + total_size);
free_str:
  free(cached);
free_dma:
  free_dma_buffer(ibitmap);
  free_dma_buffer(dbitmap);
  free_dma_buffer(inode_reg);
  return ret;
}

static int write_sb_cached_to_SSD(char *name) {
  int fdc, ret = 0;
  char *ibitmap = NULL, *dbitmap = NULL;
  struct vsfs_inode *inode_reg = NULL;

  int s_len = strlen(name);
  char *cached = malloc(s_len + 8);
  if (!cached) {
    handle_error("malloc():");
    goto free_dma;
  }
  memcpy(cached, name, s_len);
  strncpy(cached + s_len, "_cached", 8);

  printf("%s\n", cached);
  struct vsfs_cached_data *sb_cached = (struct vsfs_cached_data *)shm_oandm(
      cached, O_RDWR, PROT_READ | PROT_WRITE, &fdc);
  if (!sb_cached) {
    printf("Error: open super block cached faild!\n");
  }

  ibitmap =
      alloc_dma_buffer(VSFS_BLOCK_SIZE * sb_cached->sbi.nr_ibitmap_blocks);
  dbitmap =
      alloc_dma_buffer(VSFS_BLOCK_SIZE * sb_cached->sbi.nr_dbitmap_blocks);
  inode_reg =
      alloc_dma_buffer(VSFS_BLOCK_SIZE * sb_cached->sbi.nr_iregion_blocks);
  if (!ibitmap || !dbitmap || !inode_reg)
    goto free_dma;

  struct vsfs_sb_info *cached_sb = (struct vsfs_sb_info *)sb_cached;
  char *cached_ibitmap = (char *)sb_cached + sizeof(struct vsfs_sb_info);
  char *cached_iregion = (char *)sb_cached + sizeof(struct vsfs_sb_info) +
                         (sb_cached->sbi.nr_ibitmap_blocks * VSFS_BLOCK_SIZE);
  char *cached_dbitmap = (char *)sb_cached + sizeof(struct vsfs_sb_info) +
                         (sb_cached->sbi.nr_ibitmap_blocks * VSFS_BLOCK_SIZE) +
                         (sb_cached->sbi.nr_iregion_blocks * VSFS_BLOCK_SIZE);

  memcpy(sb_cached, cached_sb, sizeof(struct vsfs_sb_info));
  memcpy(ibitmap, cached_ibitmap,
         sb_cached->sbi.nr_ibitmap_blocks * VSFS_BLOCK_SIZE);
  memcpy(inode_reg, cached_iregion,
         sb_cached->sbi.nr_iregion_blocks * VSFS_BLOCK_SIZE);
  memcpy(dbitmap, cached_dbitmap,
         sb_cached->sbi.nr_dbitmap_blocks * VSFS_BLOCK_SIZE);

  ret = write_spdk(ibitmap, sb_cached->sbi.ofs_ibitmap,
                   sb_cached->sbi.nr_ibitmap_blocks, IO_QUEUE);
  if (ret != 0) {
    handle_error("read_spdk():");
    goto free_dma;
  }
  ret = write_spdk(dbitmap, sb_cached->sbi.ofs_dbitmap,
                   sb_cached->sbi.nr_dbitmap_blocks, IO_QUEUE);
  if (ret != 0) {
    handle_error("read_spdk():");
    goto free_dma;
  }

  ret = write_spdk(inode_reg, sb_cached->sbi.ofs_iregion,
                   sb_cached->sbi.nr_iregion_blocks, IO_QUEUE);
  if (ret != 0) {
    handle_error("read_spdk():");
    goto free_dma;
  }

  free(cached);
  shm_close((void **)&sb_cached, fdc);

free_dma:
  // free_dma_buffer(ptr);
  free_dma_buffer(ibitmap);
  free_dma_buffer(dbitmap);
  free_dma_buffer(inode_reg);

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
  int ret = -1;
  char *ptr = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  read_spdk(ptr, 0, 1, IO_QUEUE);

  struct superblock *sb = (struct superblock *)ptr;
  // check magic number
  if (sb->info.magic != VSFS_MAGIC) {
    handle_error("magic number unmatehed.");
    goto free;
  }

  ret = make_shm_cached(name, ptr);
  if (ret != 0) {
    goto free;
  }

  if (creat_open_file_table() < 0)
    handle_error("creat_open_file_table():");

  free_dma_buffer(ptr);
  return 0;
free:
  free_dma_buffer(ptr);
  return ret;
}

int unmount_vsfs(char *name) {
  int ret = -1;

  ret = write_sb_cached_to_SSD(name);
  if (ret != 0) {
    goto free;
  }

  // shm_unlink(name);
  // printf("unlink %s\n", name);
  // shm_unlink("optab");
  // printf("unlink optab\n");

  return 0;
free:
  return ret;
}