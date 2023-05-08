#include "../inc/vsfs.h"
#include "../inc/vsfs_stdinc.h"
#include "spdk.h"

#define handle_error(msg) \
  do {                    \
    perror(msg);          \
    ret = EXIT_FAILURE;   \
  } while (0)

int main(int argc, char *argv[]) {
  init_spdk();

  printf("_MY_SSD_BLOCK_SIZE_=%d\n", _MY_SSD_BLOCK_SIZE_);
  printf("VSFS_BLOCK_SIZE=%d\n", VSFS_BLOCK_SIZE);
  printf("PER_DEV_BLOCKS=%d\n", PER_DEV_BLOCKS);

  struct superblock *sb = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  read_spdk(sb, 0, 1, IO_QUEUE);

  printf(
      "Superblock: (%ld)\n"
      "\tmagic=%#x\n"
      "\tnr_blocks=%u\n"
      "\tnr_ibitmap_blocks=%u\n"
      "\tnr_iregion_blocks=%u\n"
      "\tnr_dbitmap_blocks=%u\n"
      "\tnr_dregion_blocks=%u\n"
      "\tofs_ibitmap=%u\n"
      "\tofs_iregion=%u\n"
      "\tofs_dbitmap=%u\n"
      "\tofs_dregion=%u\n"
      "\tnr_free_inodes=%u\n"
      "\tnr_free_dblock=%u\n",
      sizeof(struct superblock), sb->info.magic, sb->info.nr_blocks,
      sb->info.nr_ibitmap_blocks, sb->info.nr_iregion_blocks,
      sb->info.nr_dbitmap_blocks, sb->info.nr_dregion_blocks,
      sb->info.ofs_ibitmap, sb->info.ofs_iregion, sb->info.ofs_dbitmap,
      sb->info.ofs_dregion, sb->info.nr_free_inodes, sb->info.nr_free_dblock);

  struct vsfs_inode *root_inode = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  read_spdk(root_inode, sb->info.ofs_iregion, 1, IO_QUEUE);

  printf(
      "root inode:\n"
      "\tmode: %x\n"
      "\tblocks: %u\n"
      "\tentry: %u\n"
      "\tatime: %lu\n"
      "\tctime: %lu\n"
      "\tmtime: %lu\n",
      root_inode->mode, root_inode->blocks, root_inode->entry,
      root_inode->atime, root_inode->ctime, root_inode->mtime);

  struct vsfs_dir_block *root_dir_info = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  read_spdk(root_dir_info,
            sb->info.ofs_dregion + root_inode->l1[0], 1,
            IO_QUEUE);

  int i;
  printf("/\n");
  for (i = 0; i < root_inode->entry; i++) {
    if (i % 16 == 0) {
      read_spdk(root_dir_info, sb->info.ofs_dregion + root_inode->l1[i / 16], 1,
                IO_QUEUE);
    }

    printf("\t%s\tinode:%u\n", root_dir_info->files[i % 16].filename,
           root_dir_info->files[i % 16].inode);
  }
  printf("nr_inode/64=%ld\n", VSFS_NR_INODES / 64);

  free_dma_buffer(sb);
  free_dma_buffer(root_inode);
  free_dma_buffer(root_dir_info);
  exit_spdk();
  return 0;
}
