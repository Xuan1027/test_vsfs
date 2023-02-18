#ifndef VSFSIO_H
#define VSFSIO_H

#include "vsfs.h"
#include "vsfs_stdinc.h"
#include "vsfs_shmfunc.h"
#include "vsfs_bitmap.h"

/**
 * create a new file 
 * \param shm_name the share memory name
*/
static int vsfs_creat(char* shm_name, char* file_name){


    int fd, fdc;
    char* name = (char*)malloc(VSFS_FILENAME_LEN);
    char* shm_cache_name = (char*)malloc(strlen(shm_name)+7);
    
    strncpy(shm_cache_name, shm_name, strlen(shm_name));
    strncpy(shm_cache_name+strlen(shm_name), "_cached", 7);
    // printf("shm_cached_name = <%s>\n", shm_cache_name);

    strncpy(name, file_name, strlen(file_name));
    strcat(name, "\0");

    struct superblock* sup = (struct superblock*)shm_oandm(shm_name, O_RDWR, PROT_READ | PROT_WRITE, &fd);
    if(!sup)
      goto shm_err_ext;
    struct vsfs_cached_data* sup_cached = (struct vsfs_cached_data*)shm_oandm(shm_cache_name, O_RDWR, PROT_READ | PROT_WRITE, &fdc);
    if(!sup_cached)
      goto sup_err_ext;
    struct vsfs_inode* inode_reg = (struct vsfs_inode*)((char*)sup + sup->info.ofs_iregion*VSFS_BLOCK_SIZE);
    struct vsfs_dir_block* data_reg = (struct vsfs_dir_block*)((char*)sup + sup->info.ofs_dregion*VSFS_BLOCK_SIZE);

    // printf("vsfs_sb_info: (%ld)\n"
    //        "\tmagic=%#x\n"
    //        "\tnr_blocks=%u\n"
    //        "\tnr_ibitmap_blocks=%u\n"
    //        "\tnr_iregion_blocks=%u\n"
    //        "\tnr_dbitmap_blocks=%u\n"
    //        "\tnr_dregion_blocks=%u\n"
    //        "\tofs_ibitmap=%u\n"
    //        "\tofs_iregion=%u\n"
    //        "\tofs_dbitmap=%u\n"
    //        "\tofs_dregion=%u\n"
    //        "\tnr_free_inodes=%u\n"
    //        "\tnr_free_dblock=%u\n",
    //        sizeof(struct superblock), sup->info.magic, sup->info.nr_blocks,
    //        sup->info.nr_ibitmap_blocks, sup->info.nr_iregion_blocks,
    //        sup->info.nr_dbitmap_blocks, sup->info.nr_dregion_blocks,
    //        sup->info.ofs_ibitmap, sup->info.ofs_iregion, sup->info.ofs_dbitmap,
    //        sup->info.ofs_dregion, sup->info.nr_free_inodes, sup->info.nr_free_dblock);
        
    // printf("super_blo position is %p\n"
    //         "inode_reg position is %p\n"
    //         , sup, inode_reg);

    // printf("vsfs_root_inode: %p\n"
    //         "\tmode=%hu\n"
    //         "\tblocks=%hu\n"
    //         "\tentry=%u\n"
    //         "\tatime=%lu\n"
    //         "\tctime=%lu\n"
    //         "\tmtime=%lu\n"
    //         ,  inode_reg, inode_reg->mode, inode_reg->blocks, inode_reg->entry, inode_reg->atime, inode_reg->ctime, inode_reg->mtime);

    uint32_t f_inode = get_free_inode(sup_cached), f_dblock = (uint32_t)0;
    uint32_t d_entry = inode_reg->entry;
    printf("get free inode = %u\n", f_inode);
    
    /**
     * the step of added an new block to / dir
     * 1.get a free data region
     * 2.setting the new block to / inode --> blocks, pointer
    */
    if(inode_reg->entry >= inode_reg->blocks*16){
        printf("added new entry faild, need to add a block for / dir\n");
        f_dblock = get_free_dblock(sup_cached);
        printf("get free dblock = %u\n", f_dblock);
        d_entry = inode_reg->entry - inode_reg->blocks*16;
        // if inode pointer <= level 1
        if(inode_reg->blocks>=57){
            printf("the inode dir entry bigger than 56 blocks, need to turn to level 2 pointer\n");
            goto sup_cached_err_ext;
        }
        inode_reg->block[inode_reg->blocks] = htole32(f_dblock);
        inode_reg->blocks++;
    }

    // writing the data block entry
    data_reg[f_dblock].files[d_entry].inode = f_inode;
    strncpy(data_reg[f_dblock].files[d_entry].filename , name, strlen(name)+1);
    inode_reg->entry++;
    free(name);

    // getting the time at this moment
    time_t now;
    time(&now);
    inode_reg->atime = now;
    inode_reg->mtime = now;

    // setting the new inode
    inode_reg[f_inode].mode = htole32(0x07);
    inode_reg[f_inode].blocks = htole32(0);
    inode_reg[f_inode].atime = inode_reg[f_inode].ctime = inode_reg[f_inode].mtime = now;
    inode_reg[f_inode].size = htole32(0);

    printf("\nAfter the creat the file :\n"
            "vsfs_root_inode: %p\n"
            "\tmode=%hu\n"
            "\tblocks=%hu\n"
            "\tentry=%u\n"
            "\tatime=%s"
            ,  inode_reg, inode_reg->mode, inode_reg->blocks, inode_reg->entry, ctime(&(inode_reg->atime)));
    printf("\tctime=%s"
            , ctime(&(inode_reg->ctime)));
    printf("\tmtime=%s"
            , ctime(&(inode_reg->mtime)));

    printf("-----------------------------------\n");

    // stop printf the inode
    if(inode_reg->entry != 16)
        goto end;

    for(int i=0;i<inode_reg->entry-1;i++){
        printf("\nthe inode <%d>:\n"
                "vsfs_inode_region: %p\n"
                "\tmode=%hu\n"
                "\tblocks=%hu\n"
                "\tsize=%u\n"
                "\tatime=%s"
                , i,  &inode_reg[i], inode_reg[i].mode, inode_reg[i].blocks, inode_reg[i].size
                , ctime(&(inode_reg[i].atime)));
        printf("\tctime=%s"
                , ctime(&(inode_reg[i].ctime)));
        printf("\tmtime=%s"
                , ctime(&(inode_reg[i].mtime)));
    }

    printf("the contain of the / dir is:\n");
    printf("inode_num\tfilename\n");
    for(int i=0;i<inode_reg->entry;i++){
        printf("%2hu\t\t%s\n", data_reg[f_dblock].files[i].inode, data_reg[f_dblock].files[i].filename);
    }

end:
    shm_close(sup, &fd);
    shm_close(sup_cached, &fdc);
    free(shm_cache_name);

    return 0;
sup_cached_err_ext:
    shm_close(sup_cached, &fdc);
sup_err_ext:
    shm_close(sup, &fd);
    free(shm_cache_name);
shm_err_ext:
    return -1;
}

// static int vsfs_open(char *pathname, int flags);

// static int vsfs_read(int fildes, void *buf, size_t nbyte);

// static int vsfs_write(int fildes, const void *buf, size_t nbyte);

// static int vsfs_close(int fildes);

// static off_t vsfs_lseek(int fd, off_t offset, int whence);

// static int vsfs_stat(const char *pathname, struct stat *st) {
//   memset(st, 0, sizeof(struct stat));

// }



#endif/*VSFSIO_H*/
