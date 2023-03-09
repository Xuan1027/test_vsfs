#include "vsfs.h"
#include "vsfsio.h"
#include "vsfs_stdinc.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"


int main(int argc, char **argv)
{
    int fd, ret = 0;

    char* src = (char*)malloc(53248);

    fd = vsfs_open("test", "001", O_RDWR);
    if(fd == -1){
        printf("ERR at open file\n");
        return -1;
    }
    ret = vsfs_read("test", fd, src, 53248);
    if(ret == -1){
        printf("ERR at read file\n");
        return -1;
    }
    ret = vsfs_close(fd);
    if(ret == -1){
        printf("ERR at close file\n");
        return -1;
    }

    int count = 0;
    for(int i=0;i<53248;i++)
        if(src[i]=='a')
            count++;

    printf("Theres have %d of char <a>\n", count);

    free(src);

    return 0;
}