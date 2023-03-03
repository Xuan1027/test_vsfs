#include "vsfs.h"
#include "vsfsio.h"
#include "vsfs_stdinc.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"

int main(int argc, char** argv){
    if (argc != 3) {
        printf("bad arg num\n");
        exit(EXIT_FAILURE);
    }
    file_stat_t* re = (file_stat_t*)malloc(sizeof(file_stat_t));
    int ret = vsfs_stat(argv[1], argv[2], re);

    if(ret != 0){
        printf("err!\n");
        return -1;
    }
    printf("inode of <%s>:\n"
            "\tmode = %s\n"
            "\tsize = %d\n"
            "\tblocks = %d\n"
            "\tctime = %s"
            "\tatime = %s"
            "\tmtime = %s",
            argv[2], re->mode, re->size, re->blocks, re->ctime, re->atime, re->mtime);

    free(re);

    return 0;
}