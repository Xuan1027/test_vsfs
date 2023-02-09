#ifndef VSFS_H
#define VSFS_H

#include <stdint.h>
#include <time.h>

// Undecided
#define VSFS_MAGIC 0xF0000000

#define VSFS_SB_BLOCK_NR 0
/* 4KiB */
#define VSFS_BLOCK_SIZE (1 << 12)
#define VSFS_MAX_LEVEL1_ENTRY 50
#define VSFS_MAX_LEVEL2_ENTRY (5 << 10)
#define VSFS_MAX_LEVEL3_ENTRY                                                  \
  ((1 << 20) - VSFS_MAX_LEVEL1_ENTRY - VSFS_MAX_LEVEL2_ENTRY)
/* 4GiB */
#define VSFS_MAX_FILESIZE ((1 << 20) * VSFS_BLOCK_SIZE)
#define VSFS_NR_INODES ((1 << 11) * (VSFS_BLOCK_SIZE / sizeof(struct vsfs_inode)))
#define VSFS_FILENAME_LEN 254
#define VSFS_FILES_PER_BLOCK \
  (VSFS_BLOCK_SIZE / sizeof(struct vsfs_file_entry))

/*
 * vsfs partition layout
 * +---------------+
 * |  superblock   |  1 block
 * +---------------+
 * | inode bitmap  |  sb->nr_ibitmap_blocks blocks
 * +---------------+
 * | inode region  |  sb->nr_iregion_blocks blocks
 * +---------------+
 * | dblock bitmap |  sb->nr_dbitmap_blocks blocks
 * +---------------+
 * |     data      |  sb->nr_dregion_blocks blocks
 * |    blocks     |  rest of the blocks
 * +---------------+
 */
struct vsfs_sb_info {
  uint32_t magic; /* Magic number */

  uint32_t nr_blocks; /* Total number of blocks */

  uint32_t ofs_ibitmap;
  uint32_t ofs_iregion;
  uint32_t ofs_dbitmap;
  uint32_t ofs_dregion;

  uint32_t nr_ibitmap_blocks; /* Number of inode bitmap blocks (fixed 1)*/
  uint32_t nr_iregion_blocks; /* Number of inode region blocks (fixed 2048) */
  uint32_t nr_dbitmap_blocks; /* Number of data  bitmap blocks */
  uint32_t nr_dregion_blocks; /* Number of data  region blocks */

  uint32_t nr_free_inodes;
  uint32_t nr_free_dblock;

};

struct vsfs_level {
  unsigned int l1[50];
  unsigned int l2[5];
  unsigned int l3[1];
};

struct vsfs_inode {
  uint16_t mode;   /* File mode -> drwx */
  uint16_t blocks; /* Total number of data blocks count */
  union {
    uint32_t size;   /* File: size in byte || Dir: entry num */
    uint32_t entry;
  };
  time_t atime;    /* Access time */
  time_t ctime;    /* Inode change time */
  time_t mtime;    /* Modification time */
  union {
    unsigned int block[56];
    struct vsfs_level lv;
  };
};

struct vsfs_file_entry {
  uint16_t inode;
  char filename[VSFS_FILENAME_LEN];
};

struct vsfs_dir_block {
  struct vsfs_file_entry files[VSFS_FILES_PER_BLOCK];
};

typedef struct op_file_table_entry {
  uint16_t inode_nr;
  uint8_t ptr_counter;
} op_ftable_t;

/* superblock functions */
// int simplefs_fill_super(struct super_block *sb, void *data, int silent);

/* inode functions */
// int simplefs_init_inode_cache(void);
// void simplefs_destroy_inode_cache(void);
// struct inode *simplefs_iget(struct super_block *sb, unsigned long ino);

#endif /*VSFS_H*/
