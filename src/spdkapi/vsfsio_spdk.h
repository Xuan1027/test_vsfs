#ifndef VSFSIO_H
#define VSFSIO_H

#include "../inc/vsfs.h"
#include "../inc/vsfs_bitmap.h"
#include "../inc/vsfs_shmfunc.h"
#include "../inc/vsfs_stdinc.h"
#include "spdk.h"

#define SHM_NAME "vsfs"
#define SHM_CACHE_NAME "vsfs_cached"
#define SHOW_PROC 0
#define OP_LIMIT VSFS_OPTAB_SIZE / sizeof(op_ftable_t)
#define PAUSE                               \
  printf("Press Enter key to continue..."); \
  fgetc(stdin);

typedef struct path {
    uint16_t inode;
    struct path *next;
} path_t;

typedef struct file_descriptor_table {
    uint16_t index;
    op_ftable_t *ptr;
    path_t *p_head;
    path_t *p_tail;
    uint8_t flags;
    struct file_descriptor_table *next;
} fd_table_t;

struct fd_table_list {
    fd_table_t *head;
    fd_table_t *tail;
};

// struct file_stat total size is 128B
typedef struct file_stat {
    char mode[5];    /* File mode -> drwx */
    uint32_t blocks; /* Total number of data blocks count */
    union {
        uint64_t size; /* File: size in byte || Dir: entry num */
        uint64_t entry;
    };
    char atime[39]; /* Access time */
    char ctime[39]; /* Inode change time */
    char mtime[39]; /* Modification time */
} file_stat_t;

// the process's file descriptor talbe
struct fd_table_list *fd_table = NULL;

int fd = 0, fdc = 0, opfd = 0;
// struct superblock *sb = NULL;
struct vsfs_cached_data *sb_cached = NULL;
unsigned short *op_counter = NULL;

#define at_tmp_base(ptr, position) ptr = (typeof(ptr))(tmp_base_ptr + position)

struct vsfs_data_block *tmp_base_ptr;
// struct superblock *tmp_sb;
// struct vsfs_inode *tmp_inode_reg;
struct vsfs_dir_block *tmp_data_reg;
struct vsfs_dir_block *tmp_dir_reg;
struct vsfs_pointer_block *ptr_reg;
struct vsfs_pointer_block *l3_ptr_reg;
struct vsfs_inode *inode_reg;
struct vsfs_dir_block *data_reg;
void *dma_buf;

static int __attribute__((unused))
vsfs_creat(const char *file_name, uint32_t block_num);
static int __attribute__((unused)) vsfs_stat(char *pathname, file_stat_t *fre);
static int __attribute__((unused)) vsfs_open(char *pathname, int flags);
static int __attribute__((unused)) vsfs_close(int fildes);
static int __attribute__((unused))
vsfs_write(int fildes, const void *buf, size_t nbyte);
static int __attribute__((unused))
vsfs_read(int fildes, void *buf, size_t nbyte);
static int __attribute__((unused))
vsfs_get_inode_info(file_t *file, uint16_t inode_num);
static void __attribute__((unused)) vsfs_print_block_nbr(int fildes);
static uint64_t __attribute__((unused))
vsfs_lseek(int fd, uint64_t offset, int whence);

static int __alloc_block(unsigned long data_reg_offset, uint32_t inode_nr,
                         uint32_t need_alloc) {
  uint32_t count_down = need_alloc;
  uint32_t real_blocks = inode_reg[inode_nr].blocks;
  uint16_t start_block_l1 = 0;
  uint16_t start_block_l2 = 0;
  // struct vsfs_pointer_block *ptr_reg =
  //   (struct vsfs_pointer_block *)alloc_dma_buffer(VSFS_BLOCK_SIZE);
  // struct vsfs_pointer_block *l3_ptr_reg =
  //   (struct vsfs_pointer_block *)alloc_dma_buffer(VSFS_BLOCK_SIZE);
  // determine ptr level to use

  if (inode_reg[inode_nr].blocks >= VSFS_LEVEL2_LIMIT) {
    goto level3;
  }
  if (inode_reg[inode_nr].blocks >= VSFS_LEVEL1_LIMIT) {
    goto level2;
  }
  if (SHOW_PROC)
    printf(
        "the level 1 inside flags is:\n"
        "\ttotal blocks = %u\n"
        "\treal_blocks = %u\n"
        "\ttart_block_l2 = %u\n"
        "\ttart_block_l1 = %u\n"
        "\ttotal_needed_blocks = %u\n"
        "\tcount_down = %u\n",
        inode_reg[inode_nr].blocks, real_blocks, start_block_l2, start_block_l1,
        need_alloc, count_down);

  // adding new block to level 1
  start_block_l1 = inode_reg[inode_nr].blocks;
  for (uint16_t i = start_block_l1; i < VSFS_LEVEL1_LIMIT && count_down > 0;
       i++, count_down--) {
    inode_reg[inode_nr].l1[i] = get_free_dblock(sb_cached);
    inode_reg[inode_nr].blocks++;
  }

  if (!count_down)
    goto free_dma;

  // adding new block to level 2
level2:

  real_blocks = inode_reg[inode_nr].blocks;
  real_blocks -= VSFS_LEVEL1_LIMIT;
  start_block_l1 = real_blocks % 1024;
  start_block_l2 = real_blocks / 1024;
  if (SHOW_PROC)
    printf(
        "the level 2 inside flags is:\n"
        "\ttotal blocks = %u\n"
        "\treal_blocks = %u\n"
        "\ttart_block_l2 = %u\n"
        "\ttart_block_l1 = %u\n"
        "\ttotal_needed_blocks = %u\n"
        "\tcount_down = %u\n",
        inode_reg[inode_nr].blocks, real_blocks, start_block_l2, start_block_l1,
        need_alloc, count_down);

  for (uint16_t i = start_block_l2; i < 5 && count_down > 0; i++) {
    if (start_block_l1 == 0)
      inode_reg[inode_nr].l2[i] = get_free_dblock(sb_cached);
    read_spdk(ptr_reg, data_reg_offset + inode_reg[inode_nr].l2[i], 1,
              IO_QUEUE);
    // ptr_reg =
    //     (struct vsfs_pointer_block *)&(data_reg[inode_reg[inode_nr].l2[i]]);
    for (uint16_t j = start_block_l1;
         j < VSFS_POINTER_PER_BLOCK && count_down > 0; j++, count_down--) {
      ptr_reg->__pointer[j] = get_free_dblock(sb_cached);
      inode_reg[inode_nr].blocks++;
    }
    write_spdk(ptr_reg, data_reg_offset + inode_reg[inode_nr].l2[i], 1,
               IO_QUEUE);
    start_block_l1 = 0;
  }

  if (!count_down)
    goto free_dma;

level3:
  real_blocks = inode_reg[inode_nr].blocks;
  real_blocks -= VSFS_LEVEL2_LIMIT;
  start_block_l1 = real_blocks % 1024;
  start_block_l2 = real_blocks / 1024;
  if (SHOW_PROC)
    printf(
        "the level 3 inside flags is:\n"
        "\ttotal blocks = %u\n"
        "\treal_blocks = %u\n"
        "\ttart_block_l2 = %u\n"
        "\ttart_block_l1 = %u\n"
        "\ttotal_needed_blocks = %u\n"
        "\tcount_down = %u\n",
        inode_reg[inode_nr].blocks, real_blocks, start_block_l2, start_block_l1,
        need_alloc, count_down);

  if (real_blocks == 0) {
    inode_reg[inode_nr].l3[0] = get_free_dblock(sb_cached);
    if (SHOW_PROC)
      printf("l3_dblock = %u\n", inode_reg[inode_nr].l3[0]);
  }
  read_spdk(l3_ptr_reg, data_reg_offset + inode_reg[inode_nr].l3[0], 1,
            IO_QUEUE);
  // l3_ptr_reg =
  //     (struct vsfs_pointer_block *)&(data_reg[inode_reg[inode_nr].l3[0]]);
  for (uint16_t i = start_block_l2; i < 1024 && count_down > 0; i++) {
    if (start_block_l1 == 0) {
      l3_ptr_reg->__pointer[i] = get_free_dblock(sb_cached);
      if (SHOW_PROC)
        printf("l2_dblock = %u\n", l3_ptr_reg->__pointer[i]);
    }
    read_spdk(ptr_reg, data_reg_offset + l3_ptr_reg->__pointer[i], 1, IO_QUEUE);
    // ptr_reg =
    //     (struct vsfs_pointer_block *)&(data_reg[l3_ptr_reg->__pointer[i]]);
    for (uint16_t j = start_block_l1;
         j < VSFS_POINTER_PER_BLOCK && count_down > 0; j++, count_down--) {
      ptr_reg->__pointer[j] = get_free_dblock(sb_cached);
      if (SHOW_PROC)
        printf("l1_dblock = %u\n", ptr_reg->__pointer[j]);
      inode_reg[inode_nr].blocks++;
    }
    write_spdk(ptr_reg, data_reg_offset + l3_ptr_reg->__pointer[i], 1,
               IO_QUEUE);
    start_block_l1 = 0;
  }
  write_spdk(l3_ptr_reg, data_reg_offset + inode_reg[inode_nr].l3[0], 1,
             IO_QUEUE);

  if (!count_down)
    goto free_dma;

  return 1;
free_dma:
  // free_dma_buffer(ptr_reg);
  // free_dma_buffer(l3_ptr_reg);
  return 0;
}
/*
  tmp_inode_reg, tmp_data_reg, tmp_dir_reg, ptr_reg, l3_ptr_reg, inode_reg,
  data_reg
*/
static void init_base_ptr() {
  tmp_base_ptr =
      (struct vsfs_data_block *)alloc_dma_buffer(VSFS_BLOCK_SIZE * 6);
  // at_tmp_base(sb, 0);
  // read_spdk(sb, 0, 1, IO_QUEUE);
  // at_tmp_base(tmp_sb, 0);
  // at_tmp_base(tmp_inode_reg, 1);
  at_tmp_base(tmp_data_reg, 0);
  at_tmp_base(tmp_dir_reg, 1);
  at_tmp_base(ptr_reg, 2);
  at_tmp_base(l3_ptr_reg, 3);
  // at_tmp_base(inode_reg, 6);
  at_tmp_base(data_reg, 4);
  at_tmp_base(dma_buf, 5);
}

/**
 * create a new file
 * \param shm_name the share memory name
 */
static int vsfs_creat(const char *file_name, uint32_t block_num) {
  uint32_t f_inode, f_dblock, d_entry;
  // struct vsfs_cached_data *tmp_sb_cached;
  short ret_sb_flag = 0;
  if (tmp_base_ptr == NULL)
    init_base_ptr();

  char *name;

  if (SHOW_PROC)
    printf("vsfs_creat(): init the setting\n");

  if (strlen(file_name) >= VSFS_FILENAME_LEN) {
    printf("the file_name's lens over the limit!\n");
    goto shm_err_exit;
  }

  name = (char *)malloc(VSFS_FILENAME_LEN);

  if (strlen(file_name) >= VSFS_FILENAME_LEN) {
    printf("ERR: in vsfs_vreat(): the file_name over the limit!\n");
    goto sb_err_exit;
  }
  strcpy(name, file_name);

  if (!sb_cached) {
    sb_cached = (struct vsfs_cached_data *)shm_oandm(
        SHM_CACHE_NAME, O_RDWR, PROT_READ | PROT_WRITE, &fdc);
    if (!sb_cached)
      goto sb_err_exit;
  }
  // tmp_inode_reg =
  //     (struct vsfs_inode *)((char *)tmp_sb +
  //                           tmp_sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  // read_spdk(tmp_inode_reg, sb_cached->sbi.ofs_iregion, 1, IO_QUEUE);
  // tmp_dir_reg =
  //     (struct vsfs_dir_block *)((char *)tmp_sb +
  //                               tmp_sb->info.ofs_dregion * VSFS_BLOCK_SIZE);
  // read_spdk(tmp_dir_reg, tmp_sb_cached->sbi.ofs_dregion, 1, IO_QUEUE);
  // tmp_data_reg =
  //     (struct vsfs_data_block *)((char *)tmp_sb +
  //                                tmp_sb->info.ofs_dregion * VSFS_BLOCK_SIZE);
  // read_spdk(tmp_data_reg, tmp_sb_cached->sbi.ofs_dregion, 1, IO_QUEUE);
  // sb_cached = tmp_sb_cached;
  // if(SHOW_PROC)
  //   printf("vsfs_creat(): getting info from root\n");
  // if(vsfs_get_indoe_info(root,0)){
  //   printf("ERR: in vsfs_creat(): getting root info err!\n");
  //   goto sb_err_exit;
  // }
  // sb_cached = NULL;

  if(!inode_reg)
    inode_reg = sb_cached->inode_reg;
  // need to find if there have same file_name
  for (uint32_t i = 0; i < inode_reg->entry; i++) {
    if (i % 16 == 0)
      read_spdk(tmp_dir_reg,
                sb_cached->sbi.ofs_dregion + inode_reg->l1[i / 16], 1,
                IO_QUEUE);
    if (!strcmp(tmp_dir_reg->files[i % 16].filename, name)) {
      printf("ERR: in vsfs_creat(): you have the same file_name in FS\n");
      goto sup_cached_err_exit;
    }
  }

  f_inode = get_free_inode(sb_cached);
  f_dblock = inode_reg->entry / 16;
  d_entry = inode_reg->entry - f_dblock * 16;
  // printf("get free inode = %u\n", f_inode);

  if (SHOW_PROC)
    printf("vsfs_creat(): check block enough\n");
  /**
   * the step of added an new block to / dir
   * 1.get a free data region
   * 2.setting the new block to / inode --> blocks, pointer
   */
  if (inode_reg->entry < inode_reg->blocks * 16)
    goto adding_entry;
  // printf("added new entry faild, adding a new block for / dir\n");
  if (inode_reg->blocks < 49)
    goto adding_level1_block;
  printf(
      "the / inode dentry bigger than 49 blocks, need to turn to level 2 "
      "pointer!\n");
  // 0~48 level 1
  // 49~53 level 2
  // 54 level 3
  if (SHOW_PROC)
    printf("vsfs_creat(): turning to level 2 pointer\n");

  put_inode(sb_cached, f_inode);
  goto sup_cached_err_exit;

adding_level1_block:
  f_dblock = get_free_dblock(sb_cached);
  // printf("get free dblock = %u\n", f_dblock);
  d_entry = inode_reg->entry - inode_reg->blocks * 16;
  // if inode pointer <= level 1
  inode_reg->l1[inode_reg->blocks] = htole32(f_dblock);
  inode_reg->blocks++;

adding_entry:
  if (SHOW_PROC)
    printf("vsfs_creat(): setting the new inode\n");
  // writing the data block entry
  read_spdk(tmp_dir_reg, sb_cached->sbi.ofs_dregion + f_dblock, 1, IO_QUEUE);
  tmp_dir_reg->files[d_entry].inode = f_inode;
  // memcpy(tmp_dir_reg[f_dblock].files[d_entry].filename, name, strlen(name) +
  // 1);
  memcpy(tmp_dir_reg->files[d_entry].filename, name, strlen(name) + 1);
  write_spdk(tmp_dir_reg, sb_cached->sbi.ofs_dregion + f_dblock, 1, IO_QUEUE);

  inode_reg->entry++;

  free(name);

  // getting the time at this moment
  time_t now;
  time(&now);
  inode_reg->atime = now;
  inode_reg->mtime = now;

  // setting the new inode
  // if (!sb_cached) {
  //   sb_cached = sb_cached;
  //   ret_sb_flag = 1;
  // }
  inode_reg[f_inode].mode = htole32(0x07);
  inode_reg[f_inode].blocks = htole32(0);
  inode_reg[f_inode].atime = inode_reg[f_inode].ctime =
      inode_reg[f_inode].mtime = now;
  inode_reg[f_inode].size = htole32(0);

  // print all of the / dir
  // printf("the contain of the / dir is:\n");
  // printf("inode_num\tfilename\n");
  // int offset = 0;
  // for(int i=0;i<inode_reg->entry;i++){
  //     if(i/16 != offset)
  //         printf("in block <%d> :\n", offset+1);
  //     offset = inode_reg->block[i/16];
  //     printf("%2hu\t\t%s\n", data_reg[offset].files[i-16*offset].inode,
  //     data_reg[offset].files[i-16*offset].filename);
  // }

  __alloc_block(sb_cached->sbi.ofs_dregion, f_inode, block_num);
  if (ret_sb_flag)
    sb_cached = NULL;
  inode_reg[f_inode].blocks = htole32(block_num);
  inode_reg[f_inode].size = htole32(0);

  // shm_close(tmp_sb, tmp_fd);
  shm_close((void **)&sb_cached, fdc);
  // if (tmp_base_ptr) {
  //   free_dma_buffer(tmp_base_ptr);
  // }

  return 0;
sup_cached_err_exit:
  shm_close((void **)&sb_cached, fdc);
sb_err_exit:
  // shm_close(tmp_sb, tmp_fd);
  free(name);
  // free_dma_buffer(tmp_inode_reg);
  // free_dma_buffer(tmp_dir_reg);
  // free_dma_buffer(tmp_data_reg);
shm_err_exit:
  return -1;
}

/**
 * open a file
 * @return int for success, -1 for error
 */
static int vsfs_open(char *pathname, int flags) {
  /**
   * need to create a file descriptor table
   * the table need to point to the openfile table
   * the open file table's counter need to +1
   * need to update the file's inode and the root's inode
   * list the path of the dir and prepare to modify the a,c,mtime
   */
  if (tmp_base_ptr == NULL)
    init_base_ptr();

  int ret = -1;
  op_ftable_t *op_ftable;
  // struct vsfs_inode *inode_reg =
  //   (struct vsfs_inode *)alloc_dma_buffer(VSFS_BLOCK_SIZE);
  // struct vsfs_dir_block *data_reg =
  //   (struct vsfs_dir_block *)alloc_dma_buffer(VSFS_BLOCK_SIZE);
  uint16_t target_inode;
  file_t *file;
  short inode_find = 0;
  short optab_find = 0;

  if (strlen(pathname) >= VSFS_FILENAME_LEN) {
    printf("ERR: the pathname's lens over the limit!\n");
    goto wt_list_exit;
  }

  if (!op_counter) {
    if (SHOW_PROC)
      printf("vsfs_open(): opening the open file table\n");
    op_counter = (unsigned short *)shm_oandm("optab", O_RDWR,
                                             PROT_READ | PROT_WRITE, &opfd);
  }
  if (!op_counter) {
    printf("ERR: in vsfs_open(): open file table faild!\n");
    goto wt_list_exit;
  }
  op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_open(): open file table faild!\n");
    goto wt_list_exit;
  }

  // if (!sb) {
  //   if (SHOW_PROC)
  //     printf("vsfs_open(): opening the disk(mem)\n");
  //   // sb = (struct superblock *)alloc_dma_buffer(VSFS_BLOCK_SIZE);
  //   // sb = (struct superblock *)shm_oandm(SHM_NAME, O_RDWR,
  //   //                                     PROT_READ | PROT_WRITE, &fd);
  //   read_spdk(sb, 0, 1, IO_QUEUE);
  // }
  // if (!sb) {
  //   printf("ERR: in vsfs_open(): open disk(memory) faild!\n");
  //   goto sb_exit;
  // }

  if (!sb_cached) {
    sb_cached = (struct vsfs_cached_data *)shm_oandm(
        SHM_CACHE_NAME, O_RDWR, PROT_READ | PROT_WRITE, &fdc);
    if (!sb_cached)
      goto free_sb_ca;
  }

  // inode_reg = (struct vsfs_inode *)((char *)sb +
  //                                   sb_cached->sbi.ofs_iregion *
  //                                   VSFS_BLOCK_SIZE);
  if (!inode_reg)
    inode_reg = sb_cached->inode_reg;
  // data_reg = (struct vsfs_dir_block *)((char *)sb +
  //                                      sb_cached->sbi.ofs_dregion *
  //                                      VSFS_BLOCK_SIZE);
  // read_spdk(data_reg, sb_cached->sbi.ofs_dregion, 1, IO_QUEUE);

  if (SHOW_PROC)
    printf("vsfs_open(): check the fd_table\n");
  // adding fd_table entry
  if (!fd_table) {
    fd_table = (struct fd_table_list *)malloc(sizeof(struct fd_table_list));
    if (!fd_table) {
      printf("ERR: in vsfs_open(): malloc fd_table <%s>\n", strerror(errno));
      goto free_sb_ca;
    }
    fd_table->head = NULL;
    fd_table->tail = NULL;
  }

  if (SHOW_PROC)
    printf("vsfs_open(): adding the fd_table\n");
  // TODO: build the fd_table
  if (!fd_table->head) {
    fd_table->head = (fd_table_t *)malloc(sizeof(fd_table_t));
    if (!fd_table->head) {
      printf("ERR: in vsfs_open(): malloc fd_table->head <%s>\n",
             strerror(errno));
      goto free_fd;
    }
    fd_table->head->index = ret = 1;
    fd_table->head->next = NULL;
    fd_table->head->p_head = (path_t *)malloc(sizeof(path_t));
    if (!fd_table->head->p_head) {
      printf("ERR: in vsfs_open(): malloc fd_table->head->p_head <%s>\n",
             strerror(errno));
      goto free_fd_path;
    }
    fd_table->head->p_head->inode = 0;
    fd_table->head->p_head->next = NULL;
    fd_table->head->p_tail = fd_table->head->p_head;
    fd_table->tail = fd_table->head;
  } else {
    fd_table->tail->next = (fd_table_t *)malloc(sizeof(fd_table_t));
    if (!fd_table->tail->next) {
      printf("ERR: in vsfs_open(): malloc fd_table->tail->next <%s>\n",
             strerror(errno));
      goto free_fd_path;
    }
    ret = fd_table->tail->index + 1;
    fd_table->tail = fd_table->tail->next;
    fd_table->tail->p_head = (path_t *)malloc(sizeof(path_t));
    if (!fd_table->tail->p_head) {
      printf("ERR: in vsfs_open(): malloc fd_table->head->p_head <%s>\n",
             strerror(errno));
      goto free_fd_path;
    }
    fd_table->tail->p_head->inode = 0;
    fd_table->tail->p_head->next = NULL;
    fd_table->tail->p_tail = fd_table->tail->p_head;
    fd_table->tail->index = ret;
    fd_table->tail->next = NULL;
  }

  if (SHOW_PROC)
    printf("vsfs_open(): finding the pathname\n");
  // check for the pathname in disk

  // adding struct file here
  file = (file_t *)malloc(sizeof(file_t));

  for (uint32_t i = 0; i < inode_reg->entry; i++) {
    int offset = inode_reg->l1[i / 16];
    read_spdk(data_reg, sb_cached->sbi.ofs_dregion + offset, 1, IO_QUEUE);
    if (!strcmp(pathname, data_reg->files[i - 16 * offset].filename)) {
      inode_find = 1;
      target_inode = data_reg->files[i - 16 * offset].inode;
      fd_table->tail->p_tail->next = (path_t *)malloc(sizeof(path_t));
      if (!fd_table->tail->p_tail->next) {
        printf("ERR: in vsfs_open(): malloc walk_through->tail->next <%s>\n",
               strerror(errno));
        goto free_fd_path;
      }
      if (vsfs_get_inode_info(file, target_inode)) {
        printf("ERR: in vsfs_open(): creating struct file\n");
        goto free_fd_path;
      }
      fd_table->tail->p_tail = fd_table->tail->p_tail->next;
      fd_table->tail->p_tail->inode = target_inode;
      fd_table->tail->p_tail->next = NULL;
      break;
    }
  }
  if (!inode_find) {
    printf("ERR: in vsfs_open(): file or dir not find!\n");
    ret = -1;
    goto free_fd_path;
  }

  if (SHOW_PROC) {
    printf("vsfs_open(): printing the path of inode <%u>: ", target_inode);
    for (path_t *tmp = fd_table->tail->p_head; tmp; tmp = tmp->next) {
      printf("%u -> ", tmp->inode);
    }
    printf("NULL\n");
  }

  if (SHOW_PROC)
    printf("vsfs_open(): finding the op file table\n");
  // find the inode in open file table
  // and set the fd_table ptr to open file table
  // for(unsigned short i=0;i<(*op_counter);i++){
  //     if(op_ftable[i].inode_nr == target_inode){
  //         optab_find = 1;
  //         op_ftable[i].ptr_counter++;
  //         fd_table->tail->ptr = &(op_ftable[i]);
  //         break;
  //     }
  // }
  if ((*op_counter) >= OP_LIMIT && !optab_find) {
    printf(
        "ERR: in vsfs_open(): open file table full, need to allocate a new "
        "block\n");
    ret = -1;
    goto free_fd_path;
  }
  if (!optab_find) {
    op_ftable[(*op_counter)].inode_nr = target_inode;
    op_ftable[(*op_counter)].ptr_counter = 1;
    op_ftable[(*op_counter)].offset = 0;
    op_ftable[(*op_counter)].lock = 0;
    op_ftable[(*op_counter)].file_ptr = file;
    fd_table->tail->ptr = &(op_ftable[(*op_counter)]);
    // printf("the pointer point to %p\n", &(op_ftable[(*op_counter)]));
    (*op_counter)++;
  }

  if (SHOW_PROC)
    printf("vsfs_open(): modify the time\n");
  // modify the inode in the walk_through
  time_t now;
  time(&now);
  if (flags == O_RDONLY) {
    fd_table->tail->flags = O_RDONLY;
    for (path_t *tmp = fd_table->tail->p_head; tmp; tmp = tmp->next) {
      inode_reg[tmp->inode].atime = now;
    }
  } else if (flags == O_WRONLY || flags == O_RDWR) {
    if (flags == O_WRONLY)
      fd_table->tail->flags = O_WRONLY;
    else if (flags == O_RDWR)
      fd_table->tail->flags = O_RDWR;
    for (path_t *tmp = fd_table->tail->p_head; tmp; tmp = tmp->next) {
      inode_reg[tmp->inode].atime = now;
      inode_reg[tmp->inode].mtime = now;
    }
  }

  return ret;
free_fd_path:
  if (fd_table->head->p_head) {
    for (path_t *tmp = fd_table->head->p_head->next; tmp; tmp = tmp->next) {
      free(fd_table->head->p_head);
      fd_table->head->p_head = tmp;
    }
    if (fd_table->head->p_head)
      free(fd_table->head->p_head);
  }

free_fd:
  if (fd_table->head) {
    for (fd_table_t *tmp = fd_table->head->next; tmp; tmp = tmp->next) {
      free(fd_table->head);
      fd_table->head = tmp;
    }
    if (fd_table->head)
      free(fd_table->head);
    fd_table->head = NULL;
  }

  free(fd_table);
free_sb_ca:
  // shm_close(sb, fd);
  shm_close((void **)&op_counter, opfd);
  // free_dma_buffer(inode_reg);
  // free_dma_buffer(data_reg);
wt_list_exit:
  return ret;
}

// Remember to free the table
/**
 * \return 0 for success, -1 for err
 */
static int vsfs_close(int fildes) {
  if (tmp_base_ptr) {
    free_dma_buffer(tmp_base_ptr);
  }
  if (inode_reg)
    inode_reg = NULL;
  if (SHOW_PROC)
    printf("vsfs_close(): checking all the table\n");
  if (!fd_table) {
    printf("ERR: in vsfs_close(): fd_table are not exist!\n");
    return -1;
  }
  if (!fd_table->head) {
    printf("ERR: in vsfs_close(): fd_table's head are not exist!\n");
    return -1;
  }
  if (!fd_table->head->p_head) {
    printf("ERR: in vsfs_close(): fd_table's path_head are not exist!\n");
    return -1;
  }
  if (!op_counter) {
    printf("ERR: in vsfs_close(): open file table are not exist!\n");
    return -1;
  }
  /**
   * need to close fd_table, path, open file table counter--
   */

  op_ftable_t *op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_close(): open file table faild!\n");
    return -1;
  }
  // printf("in vsfs_close(): the open file table position is: %p\n",
  // op_ftable);

  if (SHOW_PROC)
    printf("vsfs_close(): finding the fd in fd_table\n");
  short fd_find = 0;
  fd_table_t *fd_rm;
  if (fd_table->head->index == fildes) {
    fd_find = 1;
    fd_rm = fd_table->head;
    fd_table->head = fd_table->head->next;
  } else {
    for (fd_table_t *tmp = fd_table->head; tmp; tmp = tmp->next) {
      if (tmp->next->index == fildes) {
        fd_find = 1;
        fd_rm = tmp->next;
        tmp->next = tmp->next->next;
      }
    }
  }
  if (!fd_find) {
    printf("ERR: in vsfs_close(): doesn't find the target!\n");
    goto err_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_close(): remove the path about the fd\n");
  // deal with the path
  if (fd_rm->p_head) {
    for (path_t *tmp = fd_rm->p_head->next; tmp; tmp = tmp->next) {
      free(fd_rm->p_head);
      fd_rm->p_head = tmp;
    }
    if (fd_rm->p_head)
      free(fd_rm->p_head);
  }

  if (SHOW_PROC)
    printf("vsfs_close(): free the struct file's block\n");
  free(fd_rm->ptr->file_ptr->block);
  if (SHOW_PROC)
    printf("vsfs_close(): free the struct file\n");
  free(fd_rm->ptr->file_ptr);
  if (SHOW_PROC)
    printf("vsfs_close(): ptr_counter--\n");
  // printf("the position of ptr_counter is: %p\n", fd_rm->ptr);
  if (fd_rm->ptr->ptr_counter == 0) {
    printf(
        "ERR: in vsfs_close(): the target's open file table counter is already "
        "0\n");
    goto err_exit;
  }
  fd_rm->ptr->ptr_counter--;

  if (SHOW_PROC)
    printf("vsfs_close(): del open file table\n");
  if (!fd_rm->ptr->ptr_counter) {
    // del the open file table and move the last one replace
    if ((*op_counter) <= 0) {
      printf("ERR: in vsfs_close(): the (*op_counter) is small than 0\n");
      goto err_exit;
    } else if ((*op_counter) > 1) {
      // untest this memcpy
      memcpy(fd_rm->ptr, &(op_ftable[(*op_counter) - 1]), sizeof(op_ftable_t));
    }
    (*op_counter)--;
  }

  if (SHOW_PROC)
    printf("vsfs_close(): free fd entry\n");
  free(fd_rm);

  if (!fd_table->head) {
    if (SHOW_PROC)
      printf("vsfs_close(): free fd table\n");
    free(fd_table);
    // shm_close(sb, fd);
    shm_close((void **)&sb_cached, fdc);
    shm_close((void **)&op_counter, opfd);
  }

  return 0;
err_exit:
  return -1;
}

/**
 * looking the status of an inode
 * \param shm_name the share memory name
 * \param pathename the file wantted to see for the inode
 * \param ret the status of an inode
 * \return 0 for success, -1 for error
 */
static int vsfs_stat(char *pathname, file_stat_t *fre) {
  // int tmp_fd;
  // struct superblock *tmp_sb =
  //   (struct superblock *)alloc_dma_buffer(VSFS_BLOCK_SIZE);
  // struct vsfs_inode *tmp_inode_reg =
  //   (struct vsfs_inode *)alloc_dma_buffer(VSFS_BLOCK_SIZE);
  // struct vsfs_dir_block *tmp_data_reg =
  //   (struct vsfs_dir_block *)alloc_dma_buffer(VSFS_BLOCK_SIZE);
  if (!sb_cached) {
    sb_cached = (struct vsfs_cached_data *)shm_oandm(
        SHM_CACHE_NAME, O_RDWR, PROT_READ | PROT_WRITE, &fdc);
    if (!sb_cached)
      goto op_sb_fail;
  }
  uint16_t target_inode = 0;
  short find = 0;
  // tmp_sb = (struct superblock *)shm_oandm(SHM_NAME, O_RDWR,
  //                                         PROT_READ | PROT_WRITE, &tmp_fd);
  // if (!tmp_sb)
  //   goto op_shm_err;
  // read_spdk(tmp_sb, 0, 1, IO_QUEUE);
  // tmp_inode_reg =
  //     (struct vsfs_inode *)((char *)tmp_sb +
  //                           tmp_sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  if(!inode_reg)
    inode_reg = sb_cached->inode_reg;
  // tmp_data_reg =
  //     (struct vsfs_dir_block *)((char *)tmp_sb +
  //                               tmp_sb->info.ofs_dregion * VSFS_BLOCK_SIZE);
  read_spdk(tmp_data_reg, sb_cached->sbi.ofs_dregion, 1, IO_QUEUE);

  if (SHOW_PROC)
    printf("vsfs_stat(): finding the file in shm\n");
  for (uint32_t i = 0; i < inode_reg->entry; i++) {
    int offset = inode_reg->l1[i / 16];
    read_spdk(tmp_data_reg, sb_cached->sbi.ofs_dregion + offset, 1, IO_QUEUE);

    if (SHOW_PROC)
      printf("%s\n", tmp_data_reg->files[i - 16 * offset].filename);
    if (!strcmp(pathname, tmp_data_reg->files[i - 16 * offset].filename)) {
      target_inode = tmp_data_reg->files[i - 16 * offset].inode;
      find = 1;
      break;
    }
  }
  if (!find) {
    fre = NULL;
    printf("vsfs_stat(): the filename not find in FS\n");
    goto op_sb_fail;
  }

  // printf("\nthe inode <%d>:\n"
  //         "vsfs_inode_region: %p\n"
  //         "\tmode=%hu\n"
  //         "\tblocks=%hu\n"
  //         "\tsize=%u\n"
  //         "\tatime=%s"
  //         , target_inode,  &inode_reg[target_inode],
  //         inode_reg[target_inode].mode, inode_reg[target_inode].blocks,
  //         inode_reg[target_inode].size ,
  //         ctime(&(inode_reg[target_inode].atime)));
  // printf("\tctime=%s"
  //         , ctime(&(inode_reg[target_inode].ctime)));
  // printf("\tmtime=%s"
  //         , ctime(&(inode_reg[target_inode].mtime)));

  if (SHOW_PROC)
    printf("vsfs_stat(): setting the return struct\n");
  // setting the ret
  if ((inode_reg[target_inode].mode & (1 << 3))) {
    fre->mode[0] = 'd';
    fre->entry = inode_reg[target_inode].entry;
  } else {
    fre->mode[0] = '-';
    fre->size = inode_reg[target_inode].size;
  }
  if ((inode_reg[target_inode].mode & (1 << 2)))
    fre->mode[1] = 'r';
  else
    fre->mode[1] = '-';
  if ((inode_reg[target_inode].mode & (1 << 1)))
    fre->mode[2] = 'w';
  else
    fre->mode[2] = '-';
  if ((inode_reg[target_inode].mode & (1 << 0)))
    fre->mode[3] = 'x';
  else
    fre->mode[3] = '-';
  fre->mode[4] = '\0';

  if (inode_reg[target_inode].size == 0 &&
      inode_reg[target_inode].blocks == 1048576)
    fre->size = 0x100000000;

  fre->blocks = inode_reg[target_inode].blocks;
  strcpy(fre->ctime, ctime(&(inode_reg[target_inode].ctime)));
  strcpy(fre->atime, ctime(&(inode_reg[target_inode].atime)));
  strcpy(fre->mtime, ctime(&(inode_reg[target_inode].mtime)));

  // print all of the / dir
  // printf("the contain of the / dir is:\n");
  // printf("inode_num\tfilename\n");
  // for(int i=0;i<inode_reg->entry;i++){
  //     int offset = inode_reg->block[i/16];
  //     printf("%2hu\t\t%s\n", data_reg[offset].files[i-16*offset].inode,
  //     data_reg[offset].files[i-16*offset].filename);
  // }

  return 0;
op_sb_fail:
  shm_close((void **)&sb_cached, fdc);
  return -1;
  fre = NULL;
  // free_dma_buffer(tmp_sb);
  // free_dma_buffer(tmp_inode_reg);
  // free_dma_buffer(tmp_data_reg);
  return -1;
}

static void __attribute__((unused)) vsfs_print_block_nbr(int fildes) {
  op_ftable_t *target_op = NULL;
  for (fd_table_t *tmp = fd_table->head; tmp; tmp = tmp->next) {
    if (tmp->index == fildes) {
      target_op = tmp->ptr;
    }
  }

  // struct vsfs_inode *inode_reg =
  //   (struct vsfs_inode *)alloc_dma_buffer(VSFS_BLOCK_SIZE);
  // (struct vsfs_inode *)((char *)sb +
  //                       sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  // struct vsfs_data_block *data_reg =
  // (struct vsfs_data_block *)((char *)sb +
  //                           sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

  // read_spdk(inode_reg, sb_cached->sbi.ofs_iregion + (target_op->inode_nr /
  // 16), 1,
  //           IO_QUEUE);
  if (!inode_reg)
    inode_reg = sb_cached->inode_reg;
  uint32_t blks = inode_reg[target_op->inode_nr].blocks;

  printf("level 1:\n");
  for (uint32_t i = 0; i < inode_reg[target_op->inode_nr].blocks && i < 49;
       i++, blks--) {
    printf("%u ", inode_reg[target_op->inode_nr].l1[i]);
  }

  printf("\n");
  // PAUSE;
  if (!blks)
    return;

  // struct vsfs_pointer_block *ptr_reg =
  //   (struct vsfs_pointer_block *)alloc_dma_buffer(VSFS_BLOCK_SIZE);

  for (int i = 0; blks > 0 && i < 5; i++) {
    printf("level 2[%d]: %u\n", i, inode_reg[target_op->inode_nr].l2[i]);
    read_spdk(ptr_reg,
              sb_cached->sbi.ofs_dregion + inode_reg[target_op->inode_nr].l2[i],
              1, IO_QUEUE);
    // ptr_reg = (struct vsfs_pointer_block *)(&(
    //     data_reg[inode_reg[target_op->inode_nr%16].l2[i]]));
    for (int j = 0; j < 1024 && blks > 0; j++, blks--)
      printf("%u ", ptr_reg->__pointer[j]);
    printf("\n");
    // PAUSE;
  }

  if (!blks)
    return;

  // struct vsfs_pointer_block *l3_ptr_reg =
  //   (struct vsfs_pointer_block *)alloc_dma_buffer(VSFS_BLOCK_SIZE);
  // l3_ptr_reg = (struct vsfs_pointer_block *)&(
  //     data_reg[inode_reg[target_op->inode_nr%16].l3[0]]);
  read_spdk(l3_ptr_reg,
            sb_cached->sbi.ofs_dregion + inode_reg[target_op->inode_nr].l3[0],
            1, IO_QUEUE);
  printf("level 3[0]: %u\n", inode_reg[target_op->inode_nr].l3[0]);
  for (int i = 0; blks > 0 && i < 1024; i++) {
    printf("level 2[%d]: %u\n", i, l3_ptr_reg->__pointer[i]);
    read_spdk(ptr_reg, sb_cached->sbi.ofs_dregion + l3_ptr_reg->__pointer[i], 1,
              IO_QUEUE);
    // ptr_reg =
    //     (struct vsfs_pointer_block *)(&(data_reg[l3_ptr_reg->__pointer[i]]));
    for (int j = 0; j < 1024 && blks > 0; j++, blks--)
      printf("%u ", ptr_reg->__pointer[j]);
    printf("\n");
    // PAUSE;
  }
  inode_reg = NULL;
  // free_dma_buffer(inode_reg);
  // free_dma_buffer(ptr_reg);
  // free_dma_buffer(l3_ptr_reg);
}
static int _find_target_in_fdtable(int fildes, fd_table_t **target_fd,
                                   op_ftable_t **target_op) {
  short fd_find = 0;

  if (!fd_table) {
    printf("ERR: in _find_target_in_fdtable(): fd table are not exist!\n");
    return -1;
  }
  for (fd_table_t *tmp = fd_table->head; tmp; tmp = tmp->next) {
    if (tmp->index == fildes) {
      fd_find = 1;
      if (target_fd)
        *target_fd = tmp;
      if (target_op)
        *target_op = tmp->ptr;
    }
  }
  if (!fd_find) {
    printf(
        "ERR: in _find_target_in_fdtable(): not found the target in "
        "fd_table!\n");
    return -1;
  }

  return 0;
}

static int __attribute__((unused))
__return_block(struct vsfs_data_block *data_reg, op_ftable_t *target_op,
               size_t nbyte) {
  uint32_t total_needed_blocks;
  uint32_t real_blocks = inode_reg[target_op->inode_nr].blocks;
  uint16_t start_block_l1 = 0;
  uint16_t start_block_l2 = 0;
  uint32_t need_return = 0;
  // struct vsfs_pointer_block *ptr_reg;
  // struct vsfs_pointer_block *l3_ptr_reg;

  if (SHOW_PROC)
    printf("vsfs_write(): check if there have idle dblock\n");
  // put the over data block back to free
  total_needed_blocks = (target_op->offset + nbyte) / VSFS_BLOCK_SIZE;
  if ((target_op->offset + nbyte) % VSFS_BLOCK_SIZE)
    total_needed_blocks++;
  if (total_needed_blocks < inode_reg[target_op->inode_nr].blocks) {
    need_return = inode_reg[target_op->inode_nr].blocks - total_needed_blocks;
  }
  if (SHOW_PROC)
    printf("vsfs_write(): needed return = %u\n", need_return);
  if (inode_reg[target_op->inode_nr].blocks <= VSFS_LEVEL1_LIMIT)
    goto re_level1;
  if (inode_reg[target_op->inode_nr].blocks <= VSFS_LEVEL2_LIMIT)
    goto re_level2;

  if (SHOW_PROC)
    printf("vsfs_write(): returning level 3 pointer\n");
  real_blocks = inode_reg[target_op->inode_nr].blocks - 1;
  real_blocks -= VSFS_LEVEL2_LIMIT;
  start_block_l1 = real_blocks % 1024;
  start_block_l2 = real_blocks / 1024;

  l3_ptr_reg = (struct vsfs_pointer_block *)&(
      data_reg[inode_reg[target_op->inode_nr].l3[0]]);
  for (int i = start_block_l2; i >= 0 && need_return > 0; i--) {
    ptr_reg =
        (struct vsfs_pointer_block *)(&(data_reg[l3_ptr_reg->__pointer[i]]));
    for (int j = start_block_l1; j >= 0 && need_return > 0;
         j--, need_return--) {
      put_dblock(sb_cached, ptr_reg->__pointer[j]);
      inode_reg[target_op->inode_nr].blocks--;
      if (j == 0)
        put_dblock(sb_cached, l3_ptr_reg->__pointer[i]);
      if (j == 0 && i == 0)
        put_dblock(sb_cached, inode_reg[target_op->inode_nr].l3[0]);
    }
    start_block_l1 = 1023;
  }

  if (!need_return)
    return 0;

re_level2:
  if (SHOW_PROC)
    printf("vsfs_write(): returning level 2 pointer\n");
  real_blocks = inode_reg[target_op->inode_nr].blocks - 1;
  real_blocks -= VSFS_LEVEL1_LIMIT;
  start_block_l1 = real_blocks % 1024;
  start_block_l2 = real_blocks / 1024;

  for (int i = start_block_l2; i >= 0 && need_return > 0; i--) {
    ptr_reg = (struct vsfs_pointer_block *)(&(
        data_reg[inode_reg[target_op->inode_nr].l2[i]]));
    for (int j = start_block_l1; j >= 0 && need_return > 0;
         j--, need_return--) {
      put_dblock(sb_cached, ptr_reg->__pointer[j]);
      inode_reg[target_op->inode_nr].blocks--;
      if (j == 0) {
        put_dblock(sb_cached, inode_reg[target_op->inode_nr].l2[i]);
      }
    }
    start_block_l1 = (unsigned short)1023;
  }

  if (!need_return)
    return 0;

re_level1:
  if (SHOW_PROC)
    printf("vsfs_write(): returning level 1 pointer\n");
  for (int i = VSFS_LEVEL1_LIMIT - 1; i >= 0 && need_return > 0;
       i--, need_return--) {
    put_dblock(sb_cached, inode_reg[target_op->inode_nr].l1[i]);
    inode_reg[target_op->inode_nr].blocks--;
  }

  if (!need_return)
    return 0;

  return 1;
}

static size_t __write_dblock(op_ftable_t *target_op, size_t nbyte,
                             const void *buf) {
  uint16_t offset = target_op->offset % VSFS_BLOCK_SIZE;
  uint32_t start_block = target_op->offset / VSFS_BLOCK_SIZE;

  size_t left = nbyte;
  size_t written = 0;
  size_t write_length = 0;

  for (uint32_t i = start_block; left > 0; i++) {
    if (left >= (size_t)(VSFS_BLOCK_SIZE - offset))
      write_length = VSFS_BLOCK_SIZE - offset;
    else
      write_length = left;

    if (SHOW_PROC)
      printf("file->block = %u\n", target_op->file_ptr->block[i]);

    if (SHOW_PROC)
      printf(
          "the level 1 inside value:\n"
          "\tstart_block = %u\n"
          "\toffset = %u\n"
          "\tnbyte = %lu\n"
          "\tleft = %lu\n"
          "\twritten = %lu\n"
          "\twrite_length = %lu\n"
          "\ttarget_op->offset = %lu\n",
          i, offset, nbyte, left, written, write_length, target_op->offset);

    if (write_length != VSFS_BLOCK_SIZE) {
      read_spdk(dma_buf,
                sb_cached->sbi.ofs_dregion + target_op->file_ptr->block[i], 1,
                IO_QUEUE);
      memcpy(dma_buf + offset, buf + written, write_length);
      write_spdk(dma_buf,
                 sb_cached->sbi.ofs_dregion + target_op->file_ptr->block[i], 1,
                 IO_QUEUE);
    } else {
      write_spdk((void *)buf + written,
                 sb_cached->sbi.ofs_dregion + target_op->file_ptr->block[i], 1,
                 IO_QUEUE);
    }
    written += write_length;
    left -= write_length;
    offset = 0;
    target_op->offset += write_length;

    if (SHOW_PROC)
      printf(
          "the level 1 inside value:\n"
          "\tstart_block = %u\n"
          "\toffset = %u\n"
          "\tnbyte = %lu\n"
          "\tleft = %lu\n"
          "\twritten = %lu\n"
          "\twrite_length = %lu\n"
          "\ttarget_op->offset = %lu\n",
          i, offset, nbyte, left, written, write_length, target_op->offset);
  }

  return written;
}

/**
 * writting data into disk
 * @param shm_name shm for store data
 * @param fildes fd about the file, use vsfs_open to get
 * @param buf the data you want to write
 * @param nbyte the len of your data
 * @return int for success, -1 for err
 */
static int vsfs_write(int fildes, const void *buf, size_t nbyte) {
  fd_table_t *target_fd = NULL;
  op_ftable_t *target_op = NULL;
  size_t written;
  int ret = _find_target_in_fdtable(fildes, &target_fd, &target_op);
  op_ftable_t *op_ftable = (op_ftable_t *)(op_counter + 1);

  uint32_t total_needed_blocks = (target_op->offset + nbyte) / VSFS_BLOCK_SIZE;
  if ((target_op->offset + nbyte) % VSFS_BLOCK_SIZE)
    total_needed_blocks++;

  if (ret == -1)
    goto err_exit;

  if (SHOW_PROC)
    printf("vsfs_write(): opening the open file table\n");
  if (!op_counter) {
    printf("ERR: in vsfs_write(): open file table do not exist!\n");
    goto err_exit;
  }
  if (!op_ftable) {
    printf("ERR: in vsfs_write(): open file table faild!\n");
    goto err_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_write(): finding the file\n");
  // finding the fd in fd_table

  if (target_fd->flags == O_RDONLY) {
    printf("ERR: in vsfs_write(): the open flags error!\n");
    goto err_exit;
  }

  // if (SHOW_PROC)
  //   printf("vsfs_write(): opening the disk(shm)\n");
  // if (!sb) {
  //   printf("ERR: in vsfs_write(): disk(memory) are not exist!\n");
  //   goto err_exit;
  // }
  if (SHOW_PROC)
    printf("vsfs_write(): opening the cached\n");

  if (!sb_cached) {
    printf("ERR: in vsfs_write(): vsfa_cached are not exist!\n");
    goto err_exit;
  }

  if (SHOW_PROC)
    printf(
        "vsfs_write(): checking if the size in target inode is able to "
        "write\n");
  // order for another block to store data
  if (VSFS_LEVEL3_LIMIT * VSFS_BLOCK_SIZE < target_op->offset + nbyte) {
    printf(
        "ERR: in vsfs_write(): there are too much data, exceed the "
        "limit(4G)!\n");
    goto err_exit;
  }

  if ((target_op->offset + nbyte) / VSFS_BLOCK_SIZE > 0x100000000) {
    printf("ERR: in vsfs_write(): over the file size limit(4G)!\n");
    goto err_exit;
  }

  inode_reg = sb_cached->inode_reg;
  // inode_reg =
  //     (struct vsfs_inode *)((char *)sb_cached +
  //                           sb_cached->sbi.ofs_iregion * VSFS_BLOCK_SIZE);
  // read_spdk(inode_reg, sb->info.ofs_iregion + target_op->inode_nr / 16, 1,
  //           IO_QUEUE);
  if (inode_reg[target_op->inode_nr].blocks <= total_needed_blocks) {
    __alloc_block(sb_cached->sbi.ofs_dregion, target_op->inode_nr,
                  total_needed_blocks - inode_reg[target_op->inode_nr].blocks);
    // write_spdk(inode_reg, sb->info.ofs_iregion +
    // target_op->inode_nr/16,1,IO_QUEUE);
  }
  // __return_block(inode_reg, data_reg, target_op, nbyte);

  if (SHOW_PROC)
    printf("vsfs_write(): modify the inode of a,mtime\n");
  time_t now;
  time(&now);
  target_op->file_ptr->atime = now;
  target_op->file_ptr->mtime = now;
  inode_reg[target_op->inode_nr].atime = now;
  inode_reg[target_op->inode_nr].mtime = now;

  written = __write_dblock(target_op, nbyte, buf);

  target_op->file_ptr->size += written;
  // write_spdk(inode_reg, sb->info.ofs_iregion + target_op->inode_nr / 16, 1,
  //            IO_QUEUE);

  // free_dma_buffer(inode_reg);
  return written;
err_exit:
  return -1;
}

static int64_t __read_dblock(op_ftable_t *target_op, size_t nbyte,
                             const void *buf) {
  uint16_t offset = target_op->offset % VSFS_BLOCK_SIZE;
  uint32_t start_block = target_op->offset / VSFS_BLOCK_SIZE;

  size_t left = 0;
  size_t padded = 0;
  size_t readed = 0;
  size_t read_length = 0;

  if (SHOW_PROC)
    printf("size = %lu\n", target_op->file_ptr->size);
  if (SHOW_PROC)
    printf(
        "the level 1 inside value:\n"
        "\tstart_block = %u\n"
        "\toffset = %u\n"
        "\tnbyte = %lu\n"
        "\tleft = %lu\n"
        "\treaded = %lu\n"
        "\tpadded = %lu\n"
        "\tread_length = %lu\n"
        "\ttarget_op->offset = %lu\n",
        target_op->file_ptr->block[0], offset, nbyte, left, readed, padded,
        read_length, target_op->offset);

  if (target_op->file_ptr->size >= target_op->offset + nbyte) {
    left = nbyte;
    if (left < VSFS_BLOCK_SIZE)
      padded = VSFS_BLOCK_SIZE - left;
  } else {
    if (target_op->file_ptr->size == target_op->offset)
      return -1;
    else {
      left = target_op->file_ptr->size - target_op->offset;
      padded = nbyte - left;
    }
  }

  for (uint32_t i = start_block; i < target_op->file_ptr->blocks && left > 0;
       i++) {

    if (left >= (size_t)(VSFS_BLOCK_SIZE - offset))
      read_length = VSFS_BLOCK_SIZE - offset;
    else
      read_length = left;

    if (SHOW_PROC)
      printf(
          "the level 1 inside value:\n"
          "\tstart_block = %u\n"
          "\toffset = %u\n"
          "\tnbyte = %lu\n"
          "\tleft = %lu\n"
          "\treaded = %lu\n"
          "\tpadded = %lu\n"
          "\tread_length = %lu\n"
          "\ttarget_op->offset = %lu\n",
          target_op->file_ptr->block[i], offset, nbyte, left, readed, padded,
          read_length, target_op->offset);

    read_spdk((void *)buf,
              sb_cached->sbi.ofs_dregion + target_op->file_ptr->block[i], 1,
              IO_QUEUE);
    readed += read_length;
    left -= read_length;
    offset = 0;
    target_op->offset += read_length;
  }

  memset((char *)buf + readed, '\0', padded);

  return nbyte;
}

/**
 * reading data from disk
 * @param shm_name shm for store data
 * @param fildes fd about the file, use vsfs_open to get
 * @param buf the data will be place after read
 * @param nbyte the len of data you want to read
 * @return int for success, -1 for err
 */
static int vsfs_read(int fildes, void *buf, size_t nbyte) {
  fd_table_t *target_fd = NULL;
  op_ftable_t *target_op = NULL;
  op_ftable_t *op_ftable;
  // struct vsfs_inode *inode_reg = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  // struct vsfs_data_block *data_reg = alloc_dma_buffer(VSFS_BLOCK_SIZE);;
  time_t now;
  int ret;

  if (SHOW_PROC)
    printf("vsfs_read(): opening the open file table\n");
  if (!op_counter) {
    printf("ERR: in vsfs_read(): open file table are not exist!\n");
    goto err_exit;
  }
  op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_read(): open file table faild!\n");
    goto err_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_read(): finding the file\n");
  // finding the fd in fd_table
  ret = _find_target_in_fdtable(fildes, &target_fd, &target_op);
  if (ret == -1)
    goto err_exit;
  if (target_fd->flags == O_WRONLY) {
    printf("ERR: in vsfs_read(): the open flags error!\n");
    goto err_exit;
  }

  // if (SHOW_PROC)
  //   printf("vsfs_read(): opening the disk(shm)\n");
  // if (!sb) {
  //   printf("ERR: in vsfs_read(): disk(memory) are not exist!\n");
  //   goto err_exit;
  // }
  // inode_reg = (struct vsfs_inode *)((char *)sb +
  //                                   sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  // read_spdk(inode_reg, sb_cached->sbi.ofs_iregion + (target_op->inode_nr /
  // 16),
  //           1, IO_QUEUE);
  // data_reg = (struct vsfs_data_block *)((char *)sb +
  //                                       sb->info.ofs_dregion *
  //                                       VSFS_BLOCK_SIZE);

  if (SHOW_PROC)
    printf("vsfs_read(): starting to read the file\n");

  if (SHOW_PROC)
    printf("vsfs_read(): modify the inode of a,mtime\n");
  time(&now);
  target_op->file_ptr->atime = now;
  inode_reg[target_op->inode_nr].atime = now;

  return __read_dblock(target_op, nbyte, buf);

err_exit:
  // free_dma_buffer(inode_reg);
  return -1;
}

static uint64_t vsfs_lseek(int fd, uint64_t offset, int whence) {
  op_ftable_t *op_ftable;
  // struct vsfs_inode *inode_reg = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  // struct vsfs_data_block *data_reg;
  uint32_t total_needed_blocks;
  op_ftable_t *target_op = NULL;
  uint64_t ret;
  int re;

  if (SHOW_PROC)
    printf("vsfs_lseek(): checking the init\n");
  if (!op_counter) {
    printf("ERR: in vsfs_lseek(): you need to open the file first!\n");
    goto err_exit;
  }
  op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_lseek(): open file table faild!\n");
    goto err_exit;
  }

  if (!inode_reg)
    inode_reg = sb_cached->inode_reg;
  // inode_reg =
  //     (struct vsfs_inode *)((char *)sb_cached +
  //                           sb_cached->sbi.ofs_iregion * VSFS_BLOCK_SIZE);

  // read_spdk(inode_reg, sb->info.ofs_iregion + (op_ftable->inode_nr / 16), 1,
  //           IO_QUEUE);
  // inode_reg = (struct vsfs_inode *)((char *)sb +
  //                                   sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  // data_reg = (struct vsfs_data_block *)((char *)sb +
  //                                       sb->info.ofs_dregion *
  //                                       VSFS_BLOCK_SIZE);

  if (SHOW_PROC)
    printf("vsfs_lseek(): finding the file\n");
  // finding the fd in fd_table
  re = _find_target_in_fdtable(fd, NULL, &target_op);
  if (re == -1)
    goto err_exit;

  if (SHOW_PROC)
    printf("file_size = %lu\n", target_op->file_ptr->size);

  switch (whence) {
    case SEEK_SET:
      if (SHOW_PROC)
        printf("offset = %lu\n", offset);
      if (offset > (uint64_t)0x100000000) {
        printf(
            "ERR: in vsfs_lseek(): setting offset over the file size limit\n");
        goto err_exit;
      }
      if (offset > target_op->file_ptr->size) {
        total_needed_blocks = offset / VSFS_BLOCK_SIZE;
        if (offset % VSFS_BLOCK_SIZE)
          total_needed_blocks++;
        __alloc_block(sb_cached->sbi.ofs_dregion, target_op->inode_nr,
                      total_needed_blocks - target_op->file_ptr->blocks);
        target_op->file_ptr->blocks += total_needed_blocks;
        target_op->file_ptr->size += offset - target_op->file_ptr->size;
      }
      ret = target_op->offset = offset;
      break;
    case SEEK_END:
      target_op->offset = target_op->file_ptr->size;
      if (target_op->offset + offset > target_op->file_ptr->size) {
        printf("ERR: in vsfs_lseek(): setting offset over the file size\n");
        goto err_exit;
      }
      ret = target_op->offset += offset;
      break;
    case SEEK_CUR:
      if (target_op->offset + offset > target_op->file_ptr->size) {
        printf("ERR: in vsfs_lseek(): setting offset over the file size\n");
        goto err_exit;
      }
      ret = target_op->offset += offset;
      break;
    default:
      printf("ERR: in vsfs_lseek(): not know where to set the offset\n");
      goto err_exit;
  }

  if (SHOW_PROC)
    printf("target_oft = %lu\n", target_op->offset);

  return ret;

  // free_dma_buffer(inode_reg);
err_exit:
  return -1;
}

static int vsfs_get_inode_info(file_t *file, uint16_t inode_num) {
  if (!sb_cached) {
    printf("ERR: in vsfs_get_inode_info(): sb_cached are NULL!\n");
    return -1;
  }
  if (!inode_reg)
    inode_reg = sb_cached->inode_reg;
  *file = (file_t){
      .mode = inode_reg[inode_num].mode,
      .blocks = inode_reg[inode_num].blocks,
      .inode = inode_num,
      .atime = inode_reg[inode_num].atime,
      .ctime = inode_reg[inode_num].ctime,
      .mtime = inode_reg[inode_num].mtime,
  };
  if (SHOW_PROC)
    printf(
        "the contain of file:\n"
        "\tmode = %u\n"
        "\tinode = %u\n"
        "\tsize = %lu\n"
        "\tblocks = %u\n"
        "\tatime = %lu\n"
        "\tctime = %lu\n"
        "\tmtime = %lu\n",
        file->mode, file->inode, file->size, file->blocks, file->atime,
        file->ctime, file->mtime);
  if (inode_reg[inode_num].size == 0 && inode_reg[inode_num].blocks == 1048576)
    file->size = (uint64_t)0x100000000;
  else
    file->size = inode_reg[inode_num].size;

  file->block = (uint32_t *)malloc(file->blocks * sizeof(uint32_t));

  uint32_t index = 0;
  uint32_t blks = inode_reg[inode_num].blocks;
  for (uint32_t i = 0; i < inode_reg[inode_num].blocks && i < 49; i++, blks--) {
    file->block[index++] = inode_reg[inode_num].l1[i];
  }

  if (!blks)
    goto fini;

  for (int i = 0; blks > 0 && i < 5; i++) {
    file->l2_block[i] = inode_reg[inode_num].l2[i];
    read_spdk(ptr_reg, sb_cached->sbi.ofs_dregion + inode_reg[inode_num].l2[i],
              1, IO_QUEUE);
    for (int j = 0; j < 1024 && blks > 0; j++, blks--)
      file->block[index++] = ptr_reg->__pointer[j];
  }

  if (!blks)
    goto fini;

  read_spdk(l3_ptr_reg, sb_cached->sbi.ofs_dregion + inode_reg[inode_num].l3[0],
            1, IO_QUEUE);

  file->l3_block = inode_reg[inode_num].l3[0];
  for (int i = 0; blks > 0 && i < 1024; i++) {
    file->l2_block[i + 5] = l3_ptr_reg->__pointer[i];
    read_spdk(ptr_reg, sb_cached->sbi.ofs_dregion + l3_ptr_reg->__pointer[i], 1,
              IO_QUEUE);
    for (int j = 0; j < 1024 && blks > 0; j++, blks--)
      file->block[index++] = ptr_reg->__pointer[j];
  }

fini:
  // if (SHOW_PROC) {
    for (uint32_t i = 0; i < file->blocks; i++) printf("%u ", file->block[i]);
    printf("\n");
  // }

  return 0;
}

#endif /*VSFSIO_H*/
