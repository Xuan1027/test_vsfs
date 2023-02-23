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
int vsfs_creat(char *shm_name, char *file_name)
{

    int fd, fdc;
    char *name = (char *)malloc(VSFS_FILENAME_LEN);
    char *shm_cache_name = (char *)malloc(strlen(shm_name) + 7);

    strncpy(shm_cache_name, shm_name, strlen(shm_name));
    strncpy(shm_cache_name + strlen(shm_name), "_cached", 7);
    // printf("shm_cached_name = <%s>\n", shm_cache_name);

    strncpy(name, file_name, strlen(file_name));

    struct superblock *sb = (struct superblock *)shm_oandm(shm_name, O_RDWR, PROT_READ | PROT_WRITE, &fd);
    if (!sb)
        goto shm_err_ext;
    struct vsfs_cached_data *sb_cached = (struct vsfs_cached_data *)shm_oandm(shm_cache_name, O_RDWR, PROT_READ | PROT_WRITE, &fdc);
    if (!sb_cached)
        goto sb_err_ext;
    struct vsfs_inode *inode_reg = (struct vsfs_inode *)((char *)sb + sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
    struct vsfs_dir_block *data_reg = (struct vsfs_dir_block *)((char *)sb + sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

    uint32_t f_inode = get_free_inode(sb_cached), f_dblock = (uint32_t)0;
    uint32_t d_entry = inode_reg->entry;
    // printf("get free inode = %u\n", f_inode);

    /**
     * the step of added an new block to / dir
     * 1.get a free data region
     * 2.setting the new block to / inode --> blocks, pointer
     */
    if (inode_reg->entry >= inode_reg->blocks * 16)
    {
        // printf("added new entry faild, adding a new block for / dir\n");
        if (inode_reg->blocks >= 56)
        {
            printf("the / inode dentry bigger than 56 blocks, need to turn to level 2 pointer\n");
            put_inode(sb_cached, f_inode);
            goto sup_cached_err_ext;
        }
        f_dblock = get_free_dblock(sb_cached);
        // printf("get free dblock = %u\n", f_dblock);
        d_entry = inode_reg->entry - inode_reg->blocks * 16;
        // if inode pointer <= level 1
        inode_reg->block[inode_reg->blocks] = htole32(f_dblock);
        inode_reg->blocks++;
    }

    // writing the data block entry
    data_reg[f_dblock].files[d_entry].inode = f_inode;
    strncpy(data_reg[f_dblock].files[d_entry].filename, name, strlen(name) + 1);
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

    shm_close(sb, &fd);
    shm_close(sb_cached, &fdc);
    free(shm_cache_name);

    return 0;
sup_cached_err_ext:
    shm_close(sb_cached, &fdc);
sb_err_ext:
    shm_close(sb, &fd);
    free(shm_cache_name);
shm_err_ext:
    return -1;
}

// static int vsfs_open(char *pathname, int flags);

// static int vsfs_read(int fildes, void *buf, size_t nbyte);

// static int vsfs_write(int fildes, const void *buf, size_t nbyte);

// static int vsfs_close(int fildes);

// static off_t vsfs_lseek(int fd, off_t offset, int whence);

// struct file_stat total size is 128B
typedef struct file_stat
{
    char mode[5];   /* File mode -> drwx */
    uint16_t blocks; /* Total number of data blocks count */
    union
    {
        uint32_t size; /* File: size in byte || Dir: entry num */
        uint32_t entry;
    };
    char atime[39]; /* Access time */
    char ctime[39]; /* Inode change time */
    char mtime[39]; /* Modification time */
}file_stat_t;

/**
 * looking the status of an inode
 * \param shm_name the share memory name
 * \param pathename the file wantted to see for the inode
 * \param ret the status of an inode
 * \return 0 for success, -1 for error
*/
int vsfs_stat(char *shm_name, char *pathname, file_stat_t* fre) {

    int fd;
    struct superblock *sb = (struct superblock *)shm_oandm(shm_name, O_RDWR, PROT_READ | PROT_WRITE, &fd);
    if(!sb)
        goto op_shm_err;
    struct vsfs_inode *inode_reg = (struct vsfs_inode *)((char *)sb + sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
    struct vsfs_dir_block *data_reg = (struct vsfs_dir_block *)((char *)sb + sb->info.ofs_dregion * VSFS_BLOCK_SIZE);
    
    uint16_t target_inode = 0;
    short find = 0;
    
    for(int i=0;i<inode_reg->entry;i++){
        int offset = inode_reg->block[i/16];
        if(strcmp(pathname, data_reg[offset].files[i-16*offset].filename)==0){
            target_inode = data_reg[offset].files[i-16*offset].inode;
            find = 1;
            break;
        }
    }
    if(!find){
        fre = NULL;
        goto not_find;
    }
    
    // printf("\nthe inode <%d>:\n"
    //         "vsfs_inode_region: %p\n"
    //         "\tmode=%hu\n"
    //         "\tblocks=%hu\n"
    //         "\tsize=%u\n"
    //         "\tatime=%s"
    //         , target_inode,  &inode_reg[target_inode], inode_reg[target_inode].mode, inode_reg[target_inode].blocks, inode_reg[target_inode].size
    //         , ctime(&(inode_reg[target_inode].atime)));
    // printf("\tctime=%s"
    //         , ctime(&(inode_reg[target_inode].ctime)));
    // printf("\tmtime=%s"
    //         , ctime(&(inode_reg[target_inode].mtime)));


    // setting the ret
    if((inode_reg[target_inode].mode & (1<<3))){
        fre->mode[0] = 'd';
        fre->entry = inode_reg[target_inode].entry;
    }
    else{
        fre->mode[0] = '-';
        fre->size = inode_reg[target_inode].size;
    }
    if((inode_reg[target_inode].mode & (1<<2)))
        fre->mode[1] = 'r';
    else
        fre->mode[1] = '-';
    if((inode_reg[target_inode].mode & (1<<1)))
        fre->mode[2] = 'w';
    else
        fre->mode[2] = '-';
    if((inode_reg[target_inode].mode & (1<<0)))
        fre->mode[3] = 'x';
    else
        fre->mode[3] = '-';
    fre->mode[4] = '\0';
    
    fre->blocks = inode_reg[target_inode].blocks;
    strcpy(fre->ctime, ctime(&(inode_reg[target_inode].ctime)));
    strcpy(fre->atime, ctime(&(inode_reg[target_inode].atime)));
    strcpy(fre->mtime, ctime(&(inode_reg[target_inode].mtime)));

    // print all of the / dir
    // printf("the contain of the / dir is:\n");
    // printf("inode_num\tfilename\n");
    // for(int i=0;i<inode_reg->entry;i++){
    //     int offset = inode_reg->block[i/16];
    //     printf("%2hu\t\t%s\n", data_reg[offset].files[i-16*offset].inode, data_reg[offset].files[i-16*offset].filename);
    // }

not_find:
    shm_close(sb, &fd);
    return 0;
op_shm_err:
    fre = NULL;
    return -1;
}

#endif /*VSFSIO_H*/
