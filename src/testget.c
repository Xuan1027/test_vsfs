#include "vsfs.h"
#include "vsfs_stdinc.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"

int main(int argc, char** argv){

    int fd;
    struct vsfs_cached_data* sup = shm_oandm("test_cached", O_RDWR, PROT_READ | PROT_WRITE, &fd);

    printf("vsfs_sb_info: (%ld)\n"
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
        sizeof(struct superblock), sup->sbi.magic, sup->sbi.nr_blocks,
        sup->sbi.nr_ibitmap_blocks, sup->sbi.nr_iregion_blocks,
        sup->sbi.nr_dbitmap_blocks, sup->sbi.nr_dregion_blocks,
        sup->sbi.ofs_ibitmap, sup->sbi.ofs_iregion, sup->sbi.ofs_dbitmap,
        sup->sbi.ofs_dregion, sup->sbi.nr_free_inodes, sup->sbi.nr_free_dblock);


    // uint32_t free_inode = get_free_inode(sup);

    // printf("get first free inode = %u\n", free_inode);

    printf("sup_i_bitmap = %p\n", sup->ibitmap);
    printf("sup_d_bitmap = %p\n", sup->dbitmap);
    // uint32_t free_dblock = get_free_dblock(sup);

    // printf("get first free dblock = %u\n", free_dblock);

    shm_close(sup, &fd);

    return 0;
}