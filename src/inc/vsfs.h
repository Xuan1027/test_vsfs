#ifndef VSFS_H
#define VSFS_H

#include "vsfs_stdinc.h"

// Undecided
#define VSFS_MAGIC 0xF0000000

#define VSFS_SB_BLOCK_NR 0
/* 4KiB */
#define VSFS_BLOCK_SIZE (1 << 12)
#define VSFS_MAX_LEVEL1_ENTRY 49
#define VSFS_MAX_LEVEL2_ENTRY (5 << 10)
#define VSFS_MAX_LEVEL3_ENTRY \
  ((1 << 20) - VSFS_MAX_LEVEL1_ENTRY - VSFS_MAX_LEVEL2_ENTRY)
/* 4GiB */
#define VSFS_MAX_FILESIZE ((1 << 20) * VSFS_BLOCK_SIZE)
#define VSFS_NR_INODES \
  ((1 << 11) * (VSFS_BLOCK_SIZE / sizeof(struct vsfs_inode)))
#define VSFS_FILENAME_LEN 254
#define VSFS_FILES_PER_BLOCK (VSFS_BLOCK_SIZE / sizeof(struct vsfs_file_entry))

/* define the open table size */
#define VSFS_OPTAB_SIZE 2 * VSFS_BLOCK_SIZE
#define VSFS_POINTER_PER_BLOCK (VSFS_BLOCK_SIZE / sizeof(int))
#define VSFS_LEVEL1_PTR 49
#define VSFS_LEVEL2_PTR 5 * VSFS_POINTER_PER_BLOCK
#define VSFS_LEVEL3_PTR VSFS_POINTER_PER_BLOCK * VSFS_POINTER_PER_BLOCK
#define VSFS_LEVEL1_LIMIT VSFS_LEVEL1_PTR
#define VSFS_LEVEL2_LIMIT VSFS_LEVEL1_PTR + VSFS_LEVEL2_PTR
#define VSFS_LEVEL3_LIMIT VSFS_LEVEL1_PTR + VSFS_LEVEL2_PTR + VSFS_LEVEL3_PTR

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

struct superblock {
    struct vsfs_sb_info info;
    /* Padding to match block size */
    char padding[4096 - sizeof(struct vsfs_sb_info)];
};

struct vsfs_inode {
    uint32_t mode;   /* File mode -> drwx */
    uint32_t blocks; /* Total number of data blocks count */
    union {
        uint32_t size; /* File: size in byte || Dir: entry num */
        uint32_t entry;
    };
    unsigned int l1[49];
    unsigned int l2[5];
    unsigned int l3[1];
    time_t atime; /* Access time */
    time_t ctime; /* Inode change time */
    time_t mtime; /* Modification time */
};

typedef struct file{
    uint64_t size;
    uint32_t mode;
    uint32_t blocks;
    uint32_t *block;
    uint32_t l2_block[1024];
    uint32_t l3_block;
    uint16_t inode;
    time_t atime; /* Access time */
    time_t ctime; /* Inode change time */
    time_t mtime; /* Modification time */
}file_t;

struct vsfs_file_entry {
    uint16_t inode;
    char filename[VSFS_FILENAME_LEN];
};

struct vsfs_dir_block {
    struct vsfs_file_entry files[VSFS_FILES_PER_BLOCK];
};

struct vsfs_data_block {
    char data[VSFS_BLOCK_SIZE];
};

struct vsfs_pointer_block {
    uint32_t __pointer[VSFS_POINTER_PER_BLOCK];
};

typedef struct op_file_table_entry {
    uint64_t offset;
    uint32_t inode_nr;
    uint16_t ptr_counter;
    uint16_t lock;
    file_t* file_ptr;
} op_ftable_t;

/* superblock functions */
// int vsfs_fill_super(struct super_block *sb, void *data, int silent);

/* inode functions */
// int vsfs_init_inode_cache(void);
// void vsfs_destroy_inode_cache(void);
// struct inode *vsfs_iget(struct vsfs_sb_info *sb, unsigned long ino);

#endif /*VSFS_H*/
