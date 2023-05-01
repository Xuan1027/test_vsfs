#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#include "spdk.h"

/**
 *  @author Hyam
 *  @date 2023/04/02
 *  @brief 寫入資料、讀出資料
 */

#define ROUND 1000
#define MAX_LBA 100
#define PAGE_SIZE 4096

static void test_func(queue_type type) {
    void *buf, *tmp;
    unsigned lba;
    buf = alloc_dma_buffer(PAGE_SIZE);
    tmp = malloc(PAGE_SIZE);
    for (int i = 0; i < ROUND; i++) {
        printf("\nROUND =\t%d\n",i);
        lba = rand() % MAX_LBA;
        sprintf(buf, "%d", rand() % 1000000);
        strcpy(tmp, buf);
        printf("(tmp)randnum = %s\n", (char *)tmp);
        write_spdk(buf, lba, 1, type);  // 寫進SSD
        memset(buf, 0, PAGE_SIZE);
        read_spdk(buf, lba, 1, type);  // 重新讀出來
        printf("(buf)randnum = %s\n", (char *)buf);
        trim_spdk(lba, 1, type);       // trim
        if (strcmp(buf, tmp)) {        // 比較
            printf("===================\n"
                   "|    TEST FAIL    |\n"
                   "===================\n");
            exit(-1);
        }
    }
    free_dma_buffer(buf);
    free(tmp);
}

int main(int argc, char* argv[]) {
    init_spdk();  //
    write(STDOUT_FILENO, "Initialization complete.\n", 26);
    srand(time(NULL));
    test_func(IO_QUEUE);
    printf("===================\n"
           "|  TEST SUCCESS   |\n"
           "===================\n");
    exit_spdk();
}