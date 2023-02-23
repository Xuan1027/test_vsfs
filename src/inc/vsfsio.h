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

typedef struct file_descriptor_table{
    uint16_t index;
    op_ftable_t* ptr;
    struct file_descriptor_table* next;
}fd_table_t;

struct fd_table_list{
    fd_table_t* head;
    fd_table_t* tail;
};

typedef struct path{
    uint16_t inode;
    struct path* next;
}path_t;

struct path_list{
    path_t* head;
    path_t* tail;
};

// the process's file descriptor talbe
struct fd_table_list* fd_table = NULL;

// the path where the file pass
struct path_list* walk_through = NULL;

unsigned short op_entry = 0;

int vsfs_open(char *shm_name, char *pathname, int flags){
    /**
     * need to create a file descriptor table
     * the table need to point to the openfile table
     * the open file table's counter need to +1
     * need to update the file's inode and the root's inode
     * list the path of the dir and prepare to modify the a,c,mtime
    */

    int ret = -1;

    if(!walk_through){
        walk_through = (struct path_list*)malloc(sizeof(struct path_list));
        if(!walk_through){
            printf("ERR: malloc walk_through <%s>\n", strerror(errno));
            goto wt_list_ext;
        }
        walk_through->head = NULL;
        walk_through->tail = NULL;
    }

    // list the pass through inode prepare to modify the a,c,m_time;
    if(!walk_through->head){
        walk_through->head = (path_t*)malloc(sizeof(path_t));
        if(!walk_through->head){
            printf("ERR: malloc walk_through->head <%s>\n", strerror(errno));
            goto wt_ext;
        }
        walk_through->head->inode = 0;
        walk_through->head->next = NULL;
        walk_through->tail = walk_through->head;
    }
    else{
        printf("ERR: the walk_through are not <NULL>\n");
        goto wt_ext;
    }
    /**
     * first the open file table
     * second the file descriptor table
     * then modify the inode
    */
    int opfd, fd;
    op_ftable_t* op_ftable = (op_ftable_t*)shm_oandm(shm_name, O_RDWR, PROT_READ | PROT_WRITE, &opfd);
    if(!op_ftable){
        printf("ERR: open file table faild!\n");
        goto wt_ext;
    }
    // TODO: find the pathname of the file
    struct superblock *sb = (struct superblock *)shm_oandm(shm_name, O_RDWR, PROT_READ | PROT_WRITE, &fd);
    if(!sb){
        printf("ERR: open disk(memory) faild!\n");
        goto sb_ext;
    }
    struct vsfs_inode *inode_reg = (struct vsfs_inode *)((char *)sb + sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
    struct vsfs_dir_block *data_reg = (struct vsfs_dir_block *)((char *)sb + sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

    // check for the pathname in disk
    uint16_t target_inode;
    short inode_find = 0;
    for(int i=0;i<inode_reg->entry;i++){
        int offset = inode_reg->block[i/16];
        if(!strcmp(pathname, data_reg[offset].files[i-16*offset].filename)){
            target_inode = data_reg[offset].files[i-16*offset].inode;
            inode_find = 1;
            break;
        }
    }
    if(!inode_find){
        printf("ERR: file or dir not find!\n");
        goto not_find;
    }

    // adding fd_table entry
    if(!fd_table){
        fd_table = (struct fd_table_list*)malloc(sizeof(struct fd_table_list));
        if(!fd_table){
            printf("ERR: malloc fd_table <%s>\n", strerror(errno));
            goto not_find;
        }
        fd_table->head = NULL;
        fd_table->tail = NULL;
    }

    // TODO: build the fd_table
    if(!fd_table->head){
        fd_table->head = (fd_table_t*)malloc(sizeof(fd_table_t));
        if(!fd_table->head){
            printf("ERR: malloc fd_table->head <%s>\n", strerror(errno));
            goto fd_ext;
        }
        fd_table->head->index = ret = 1;
        fd_table->head->next = NULL;
        fd_table->tail = fd_table->head;
    }
    else{
        fd_table->tail->next = (fd_table_t*)malloc(sizeof(fd_table_t));
        if(!fd_table->tail->next){
            printf("ERR: malloc fd_table->tail->next <%s>\n", strerror(errno));
            goto fd_ext;
        }
        ret = fd_table->tail->index+1;
        fd_table->tail = fd_table->tail->next;
        fd_table->tail->index = ret;
        fd_table->tail->next = NULL;
    }

    // find the inode in open file table
    // and set the fd_table ptr to open file table
    short optab_find = 0;
    for(unsigned short i=0;i<op_entry;i++){
        if(op_ftable[i].inode_nr == target_inode){
            optab_find = 1;
            op_ftable[i].ptr_counter++;
            fd_table->tail->ptr = &(op_ftable[i]);
            break;
        }
    }
    if(op_entry>=64 && !optab_find){
        printf("ERR: open file table full, need to allocate a new block\n");
        ret = -1;
        goto fd_ext;
    }
    if(!optab_find){
        op_ftable[op_entry].inode_nr = target_inode;
        op_ftable[op_entry].ptr_counter = 1;
        op_ftable[op_entry].offset = 0;
        op_ftable[op_entry].lock = 0;
        op_entry++;
    }

    // modify the inode in the walk_through
    time_t now;
    time(&now);
    if(flags & O_RDONLY){
        for(path_t* tmp=walk_through->head;tmp;tmp=tmp->next)
            inode_reg[tmp->inode].atime = now;
    }
    if(flags & O_WRONLY){
        for(path_t* tmp=walk_through->head;tmp;tmp=tmp->next){
            inode_reg[tmp->inode].atime = now;
            inode_reg[tmp->inode].mtime = now;
        }
    }

    shm_close(sb, &fd);
    shm_close(op_ftable, &opfd);

    return ret;
fd_ext:
    if(fd_table->head){
        for(fd_table_t* tmp=fd_table->head->next;tmp;tmp=tmp->next){
            free(fd_table->head);
            fd_table->head = tmp;
        }
        if(fd_table->head)
            free(fd_table->head);
        fd_table->head = NULL;
    }

    free(fd_table);
not_find:
    shm_close(sb, &fd);
sb_ext:
    shm_close(op_ftable, &opfd);
wt_ext:
    if(walk_through->head){
        for(path_t* tmp=walk_through->head->next;tmp;tmp=tmp->next){
            free(walk_through->head);
            walk_through->head = tmp;
        }
        if(walk_through->head)
            free(walk_through->head);
        walk_through->head = NULL;
    }

    free(walk_through);
wt_list_ext:
    return ret;
}

// static int vsfs_read(int fildes, void *buf, size_t nbyte);

// static int vsfs_write(int fildes, const void *buf, size_t nbyte);

// Remember to free the table
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
        if(!strcmp(pathname, data_reg[offset].files[i-16*offset].filename)){
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
