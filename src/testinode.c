#include "vsfs.h"
#include "vsfs_stdinc.h"
#include "vsfs_bitmap.h"

int main(int argc, char **argv)
{

    int fd = shm_open("test_cached", O_CREAT | O_RDWR, 0666);

    struct stat fstats;
    int re = fstat(fd, &fstats);
    if(re == -1)
        return -1;
    struct vsfs_cached_data *ptr = mmap(NULL, fstats.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);


    printf("Superblock: (%d)\n"
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
           VSFS_BLOCK_SIZE, ptr->sbi.magic, ptr->sbi.nr_blocks,
           ptr->sbi.nr_ibitmap_blocks, ptr->sbi.nr_iregion_blocks,
           ptr->sbi.nr_dbitmap_blocks, ptr->sbi.nr_dregion_blocks,
           ptr->sbi.ofs_ibitmap, ptr->sbi.ofs_iregion, ptr->sbi.ofs_dbitmap,
           ptr->sbi.ofs_dregion);

    uint32_t ret = get_free_inode(ptr);

    printf("get return = %u\n", ret);

    put_inode(ptr, ret);

    munmap(ptr, fstats.st_size);

    close(fd);

    return 0;
}