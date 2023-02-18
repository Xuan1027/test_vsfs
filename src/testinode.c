#include "vsfs.h"
#include "vsfsio.h"
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
    if (argc != 2) {
        printf("bad arg num\n");
        exit(EXIT_FAILURE);
    }
    char* fname = (char*)malloc(VSFS_FILENAME_LEN);
    for(int i=1;i<=20;i++){
        sprintf(fname, "%03d", i);
        int ret = vsfs_creat(argv[1], fname);
        if(ret == -1){
            return -1;
        }
    }
    free(fname);
    
    return 0;
}