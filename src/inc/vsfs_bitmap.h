#ifndef VSFS_BITMAP_H
#define VSFS_BITMAP_H

#include "vsfs.h"
#include "vsfs_stdinc.h"

/*
 * vsfs cache in memory struct
 * +---------------+
 * |  superblock   |  sizeof(struct vsfs_sb_info)
 * +---------------+
 * | inode bitmap  |  sb->nr_ibitmap_blocks blocks
 * +---------------+
 * | dblock bitmap |  sb->nr_dbitmap_blocks blocks
 * +---------------+
 */
struct vsfs_cached_data {
  struct vsfs_sb_info sbi;
  uint64_t ibitmap[VSFS_NR_INODES / 64];
  uint64_t *dbitmap;
};

/*
 * Returns the index of the least significant 1-bit of x, or if x is zero,
 * returns -1.
 */
static inline int _ffs(uint64_t x) {
  if (x == 0)
    return -1;
  return (x&0xffffffff)?__builtin_ffs(x)-1:__builtin_ffs(x>>32)+31;
}

/*
 * Return the position of the first set bit we found after we clear it. (we
 * assume that the first bit is never free because of the superblock and the
 * root inode, thus allowing us to use 0 as an error value).
 */
static inline uint32_t get_first_free_bit(unsigned long *freemap,
                                          unsigned long size) {
  uint32_t count = 0;
  while (++count <= size) {
    if (freemap[count]) {
      int n = _ffs(freemap[count]);
      freemap[count] &= ~(1 << n);
      return (count * 64) + _ffs(freemap[count]);
    }
  }
  return 0;
}

static inline uint32_t get_free_inode(struct vsfs_cached_data *cd) {
  uint32_t ret =
      get_first_free_bit(cd->ibitmap, VSFS_NR_INODES / sizeof(uint64_t));
  if (ret)
    cd->sbi.nr_free_inodes--;
  return ret;
}

static inline uint32_t get_free_dblock(struct vsfs_cached_data *cd) {
  uint32_t ret = get_first_free_bit(cd->dbitmap, cd->sbi.nr_dregion_blocks /
                                                      sizeof(uint64_t));
  if (ret)
    cd->sbi.nr_free_dblock--;
  return ret;
}

/* Mark the i-th bit in freemap as free (i.e. 1) */
static inline int put_free_bit(unsigned long *freemap, unsigned long size,
                                    uint32_t i) {
  freemap[i/64]|=1<<(i%64);
  return 0;
}

static inline void put_inode(struct vsfs_cached_data *cd,uint32_t ino){
if(put_free_bit(cd->ibitmap, VSFS_NR_INODES/sizeof(uint64_t), ino))
    return;
  cd->sbi.nr_free_inodes++;
}

static inline void put_dblock(struct vsfs_cached_data *cd, uint32_t bno){
  if(put_free_bit(cd->dbitmap, cd->sbi.nr_dregion_blocks/sizeof(uint64_t), bno))
    return;
  cd->sbi.nr_free_dblock++;
}

#endif /*VSFS_BITMAP_H*/
