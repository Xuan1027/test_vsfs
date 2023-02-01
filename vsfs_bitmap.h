#ifndef VSFS_BITMAP_H
#define VSFS_BITMAP_H

#include "vsfs.h"


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

/*
 * Return the number of bit we found and clear the the following `len` consecutive
 * free bit(s) (set to 1) in a given in-memory bitmap spanning over multiple
 * blocks. Return 0 if no enough free bit(s) were found (we assume that the
 * first bit is never free because of the superblock and the root inode, thus
 * allowing us to use 0 as an error value).
 */
static inline uint32_t get_first_free_bits(unsigned long *freemap,
                                           unsigned long size,
                                           uint32_t len)
{
    uint32_t bit, prev = 0, count = 0;
    for_each_set_bit (bit, freemap, size) {
        if (prev != bit - 1)
            count = 0;
        prev = bit;
        if (++count == len) {
            bitmap_clear(freemap, bit - len + 1, len);
            return bit - len + 1;
        }
    }
    return 0;
}

#endif/*VSFS_BITMAP_H*/
