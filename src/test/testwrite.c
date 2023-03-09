#include "vsfs.h"
#include "vsfsio.h"
#include "vsfs_stdinc.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"


int main(int argc, char **argv)
{
    int fd, ret = 0;

    char* src = (char*)malloc(1);

    *src = 'a';

    fd = vsfs_open("test", "001", O_RDWR);
    if(fd == -1){
        printf("ERR at open file\n");
        return -1;
    }
    for(int i=0;i<53248;i++){
        ret = vsfs_write("test", fd, src, 1);
        if(ret == -1){
            printf("ERR at read file\n");
            return -1;
        }
    }
    ret = vsfs_close(fd);
    if(ret == -1){
        printf("ERR at close file\n");
        return -1;
    }

    free(src);

    return 0;
}