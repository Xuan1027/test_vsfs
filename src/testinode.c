#include "vsfs.h"
// #include "vsfsio.h"
#include "vsfs_stdinc.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"


/**
 * get free inode first, then
 * added an new entry of / dir, and
 * setting the info inside the / inode --> atime, mtime
 * setting the info about the new inode
*/
int main(int argc, char **argv)
{
    int fd, fdc;
    
    char* name = malloc(10);
    strncpy(name, argv[1], 4);
    strcat(name, "\0");

    struct superblock* sup = (struct superblock*)shm_oandm("test", O_RDWR, PROT_READ | PROT_WRITE, &fd);
    struct vsfs_cached_data* sup_cached = (struct vsfs_cached_data*)shm_oandm("test_cached", O_RDWR, PROT_READ | PROT_WRITE, &fdc);
    struct vsfs_inode* inode_reg = (struct vsfs_inode*)((char*)sup + sup->info.ofs_iregion*VSFS_BLOCK_SIZE);
    struct vsfs_dir_block* data_reg = (struct vsfs_dir_block*)((char*)sup + sup->info.ofs_dregion*VSFS_BLOCK_SIZE);

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
           sizeof(struct superblock), sup->info.magic, sup->info.nr_blocks,
           sup->info.nr_ibitmap_blocks, sup->info.nr_iregion_blocks,
           sup->info.nr_dbitmap_blocks, sup->info.nr_dregion_blocks,
           sup->info.ofs_ibitmap, sup->info.ofs_iregion, sup->info.ofs_dbitmap,
           sup->info.ofs_dregion, sup->info.nr_free_inodes, sup->info.nr_free_dblock);
        
    printf("super_blo position is %p\n"
            "inode_reg position is %p\n"
            , sup, inode_reg);

    printf("vsfs_inode_region: %p\n"
            "\tmode=%hu\n"
            "\tblocks=%hu\n"
            "\tentry=%u\n"
            "\tatime=%lu\n"
            "\tctime=%lu\n"
            "\tmtime=%lu\n"
            ,  inode_reg, inode_reg->mode, inode_reg->blocks, inode_reg->entry, inode_reg->atime, inode_reg->ctime, inode_reg->mtime);

    uint32_t ret = get_free_inode(sup_cached);
    printf("get return = %u\n", ret);
    
    if(inode_reg->entry >= inode_reg->blocks*16){
        printf("added new entry faild, need to add a block for / dir\n");
        goto err_ext;
    }
    name = strcat(name, "\0");
    data_reg->files[inode_reg->entry].inode = ret;
    strncpy(data_reg->files[inode_reg->entry++].filename , name, (int)(strlen(name))+1);

    time_t now;
    time(&now);
    inode_reg->atime = now;
    inode_reg->mtime = now;

    inode_reg[ret].mode = htole32(0x0f);
    inode_reg[ret].blocks = htole32(ret);
    inode_reg[ret].atime = inode_reg[ret].ctime = inode_reg[ret].mtime = now;
    inode_reg[ret].size = htole32(ret);

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
        printf("%2hu\t\t%s\n", data_reg->files[i].inode, data_reg->files[i].filename);
    }


    shm_close(sup, &fd);
    shm_close(sup_cached, &fdc);

    return 0;
err_ext:
    shm_close(sup, &fd);
    shm_close(sup_cached, &fdc);
    return -1;
}