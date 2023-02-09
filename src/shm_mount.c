#include <fcntl.h>
#include <linux/fs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "inc/vsfs.h"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    ret = EXIT_FAILURE;                                                        \
  } while (0)

struct superblock {
  struct vsfs_sb_info info;
  /* Padding to match block size */
  char padding[4096 - sizeof(struct vsfs_sb_info)];
};

static int make_shm_cached(char *name, char *ptr) {
  struct superblock *sb = (struct superblock *)ptr;
  char *ibitmap = ptr + (sb->info.ofs_ibitmap * VSFS_BLOCK_SIZE);
  char *dbitmap = ptr + (sb->info.ofs_dbitmap * VSFS_BLOCK_SIZE);

  int ret = 0;
  int s_len = strlen(name);
  char *cached = malloc(s_len + 7);
  if (!cached) {
    handle_error("malloc():");
    goto end;
  }
  strncpy(cached, name, s_len);
  strncpy(cached + s_len, "_cached", 7);

  int cfd = shm_open(cached, O_CREAT | O_RDWR, 0666);
  if (cfd == -1) {
    handle_error("shm_open():");
    goto free_str;
  }

  // sizeof(inode bitmap + data bitmap)
  uint32_t bitmap_total_size =
      (sb->info.nr_ibitmap_blocks + sb->info.nr_dbitmap_blocks) *
      VSFS_BLOCK_SIZE;

  ftruncate(cfd, sizeof(struct vsfs_sb_info) + bitmap_total_size);
  char *cptr = mmap(NULL, sizeof(struct vsfs_sb_info) + bitmap_total_size,
                    PROT_READ | PROT_WRITE, MAP_SHARED, cfd, 0);
  if (cptr == MAP_FAILED) {
    handle_error("mmap():");
    goto free_str;
  }
  struct vsfs_sb_info *cached_sb = (struct vsfs_sb_info *)cptr;
  char *cached_ibitmap = cptr + sizeof(struct vsfs_sb_info *);
  char *cached_dbitmap = cptr + sizeof(struct vsfs_sb_info *) +
                         (sb->info.nr_ibitmap_blocks * VSFS_BLOCK_SIZE);

  memcpy(cached_sb, sb, sizeof(struct vsfs_sb_info));
  memcpy(cached_ibitmap, ibitmap,
         sizeof(sb->info.nr_ibitmap_blocks * VSFS_BLOCK_SIZE));
  memcpy(cached_dbitmap, dbitmap,
         sizeof(sb->info.nr_dbitmap_blocks * VSFS_BLOCK_SIZE));
  munmap(cptr, sizeof(struct vsfs_sb_info) + bitmap_total_size);
free_str:
  free(cached);
end:
  return ret;
}

static int init_spdk_daemon(){
  int ret = 0;
  // todo:
  return ret;
}

static int open_file_table_test(){
  // open file table test
  int opfd = shm_open("optab", O_CREAT | O_RDWR, 0777);
  if(opfd < 0)
    return opfd;

  ftruncate(opfd, VSFS_BLOCK_SIZE);

  op_ftable_t* data = 
    (op_ftable_t*)mmap(NULL, VSFS_BLOCK_SIZE, PROT_READ, MAP_SHARED, opfd, 0);


  munmap(opfd, VSFS_BLOCK_SIZE);

  close(opfd);

  return 0;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("bad arg num\n");
    exit(EXIT_FAILURE);
  }
  char *name = argv[1];
  int fd = shm_open(name, O_CREAT | O_RDWR, 0666);

  struct stat fstats;
  int ret = fstat(fd, &fstats);
  if (ret) {
    handle_error("fstat():");
    return ret;
  }

  char *ptr;
  ptr = mmap(NULL, fstats.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  struct superblock *sb = (struct superblock *)ptr;
  // check magic number
  if (sb->info.magic != VSFS_MAGIC) {
    handle_error("magic number unmatehed.");
    goto unmap;
  }

  if (make_shm_cached(name, ptr) != 0) {
    goto unmap;
  }

  if(open_file_table_test() < 0)
    handle_error("open_file_table_test():");

  return 0;
unmap:
  munmap(ptr, fstats.st_size);
  close(fd);
  return ret;
}
