#include "vsfs.h"
#include "debug_util.h"
#include "vsfs_stdinc.h"
#include "vsfs_bitmap.h"

int main(int argc, char *argv[]) {
  int fd = shm_open("test_cached", O_RDWR, 0666);
  struct stat fstats;
  if (fstat(fd, &fstats) == -1) {
    perror("fstat");
    exit(EXIT_FAILURE);
  }

  struct vsfs_cached_data *ptr =
      mmap(NULL, fstats.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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
         sizeof(struct vsfs_sb_info), ptr->sbi.magic, ptr->sbi.nr_blocks,
         ptr->sbi.nr_ibitmap_blocks, ptr->sbi.nr_iregion_blocks,
         ptr->sbi.nr_dbitmap_blocks, ptr->sbi.nr_dregion_blocks,
         ptr->sbi.ofs_ibitmap, ptr->sbi.ofs_iregion, ptr->sbi.ofs_dbitmap,
         ptr->sbi.ofs_dregion, ptr->sbi.nr_free_inodes, ptr->sbi.nr_free_dblock);

  printf("\n###check current cached ibitmap###\n");
  print_binary(ptr->ibitmap, VSFS_BLOCK_SIZE * ptr->sbi.nr_ibitmap_blocks);

  printf("\n###start testing get_free_inode###\n");
  int count = 0;
  while (get_free_inode(ptr)) {
    count++;
  };
  printf("- success to get %d free inodes\n", count);
  printf("\n###check current cached ibitmap###\n");
  print_binary(ptr->ibitmap, VSFS_BLOCK_SIZE * ptr->sbi.nr_ibitmap_blocks);

  printf("\n###start testing put_inode###\n");
  int i;
  for (i = 0; i<count; i++) {
    put_inode(ptr, i+1);
  };
  printf("- success to put %u inodes\n", i);
  printf("\n###check current cached ibitmap###\n");
  print_binary(ptr->ibitmap, VSFS_BLOCK_SIZE * ptr->sbi.nr_ibitmap_blocks);
  printf("\n###start testing get_free_inode###\n");

  return 0;
}
