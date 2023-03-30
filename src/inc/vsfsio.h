#ifndef VSFSIO_H
#define VSFSIO_H

#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_func.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"

#define SHM_NAME "vsfs"
#define SHM_CACHE_NAME "vsfs_cached"
#define SHOW_PROC 0
#define SHOW_PROG 0
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
struct superblock *sb = NULL;
struct vsfs_cached_data *sb_cached = NULL;
unsigned short *op_counter = NULL;

static int vsfs_creat(const char *file_name) __attribute__((unused));
static int vsfs_stat(char *pathname, file_stat_t *fre) __attribute__((unused));
static int vsfs_open(char *pathname, int flags) __attribute__((unused));
static int vsfs_close(int fildes) __attribute__((unused));
static int vsfs_write(int fildes, const void *buf, size_t nbyte)
    __attribute__((unused));
static int vsfs_read(int fildes, void *buf, size_t nbyte)
    __attribute__((unused));
static off_t vsfs_lseek(int fd, off_t offset, int whence)
    __attribute__((unused));

/**
 * create a new file
 * \param shm_name the share memory name
 */
static int vsfs_creat(const char *file_name) {
  if (SHOW_PROC)
    printf("vsfs_creat(): init the setting\n");
  if (strlen(file_name) >= VSFS_FILENAME_LEN) {
    printf("the file_name's lens over the limit!\n");
    goto shm_err_exit;
  }

  int tmp_fd, tmp_fdc;
  char *name = (char *)malloc(VSFS_FILENAME_LEN);

  // printf("shm_cached_name = <%s>\n", shm_cache_name);

  if (strlen(file_name) >= VSFS_FILENAME_LEN) {
    printf("ERR: in vsfs_vreat(): the file_name over the limit!\n");
    goto sb_err_exit;
  }
  strcpy(name, file_name);

  struct superblock *tmp_sb = (struct superblock *)shm_oandm(
      SHM_NAME, O_RDWR, PROT_READ | PROT_WRITE, &tmp_fd);
  if (!tmp_sb)
    goto shm_err_exit;
  struct vsfs_cached_data *tmp_sb_cached = (struct vsfs_cached_data *)shm_oandm(
      SHM_CACHE_NAME, O_RDWR, PROT_READ | PROT_WRITE, &tmp_fdc);
  if (!tmp_sb_cached)
    goto sb_err_exit;
  struct vsfs_inode *tmp_inode_reg =
      (struct vsfs_inode *)((char *)tmp_sb +
                            tmp_sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  struct vsfs_dir_block *tmp_data_reg =
      (struct vsfs_dir_block *)((char *)tmp_sb +
                                tmp_sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

  uint32_t f_inode = get_free_inode(tmp_sb_cached),
           f_dblock = tmp_inode_reg->entry / 16;
  uint32_t d_entry = tmp_inode_reg->entry - f_dblock * 16;
  // printf("get free inode = %u\n", f_inode);

  if (SHOW_PROC)
    printf("vsfs_creat(): check block enough\n");
  /**
   * the step of added an new block to / dir
   * 1.get a free data region
   * 2.setting the new block to / inode --> blocks, pointer
   */
  if (tmp_inode_reg->entry < tmp_inode_reg->blocks * 16)
    goto adding_entry;
  // printf("added new entry faild, adding a new block for / dir\n");
  if (tmp_inode_reg->blocks < 49)
    goto adding_level1_block;
  printf(
      "the / inode dentry bigger than 49 blocks, need to turn to level 2 "
      "pointer!\n");
  // 0~48 level 1
  // 49~53 level 2
  // 54 level 3
  if (SHOW_PROC)
    printf("vsfs_creat(): turning to level 2 pointer\n");

  put_inode(tmp_sb_cached, f_inode);
  goto sup_cached_err_exit;

adding_level1_block:
  f_dblock = get_free_dblock(tmp_sb_cached);
  // printf("get free dblock = %u\n", f_dblock);
  d_entry = tmp_inode_reg->entry - tmp_inode_reg->blocks * 16;
  // if inode pointer <= level 1
  tmp_inode_reg->l1[tmp_inode_reg->blocks] = htole32(f_dblock);
  tmp_inode_reg->blocks++;

adding_entry:
  if (SHOW_PROC)
    printf("vsfs_creat(): setting the new inode\n");
  // writing the data block entry
  tmp_data_reg[f_dblock].files[d_entry].inode = f_inode;
  memcpy(tmp_data_reg[f_dblock].files[d_entry].filename, name,
         strlen(name) + 1);
  tmp_inode_reg->entry++;
  free(name);

  // getting the time at this moment
  time_t now;
  time(&now);
  tmp_inode_reg->atime = now;
  tmp_inode_reg->mtime = now;

  // setting the new inode
  tmp_inode_reg[f_inode].mode = htole32(0x07);
  tmp_inode_reg[f_inode].blocks = htole32(0);
  tmp_inode_reg[f_inode].atime = tmp_inode_reg[f_inode].ctime =
      tmp_inode_reg[f_inode].mtime = now;
  tmp_inode_reg[f_inode].size = htole32(0);

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

  shm_close(tmp_sb, &tmp_fd);
  shm_close(tmp_sb_cached, &tmp_fdc);

  return 0;
sup_cached_err_exit:
  shm_close(tmp_sb_cached, &tmp_fdc);
sb_err_exit:
  shm_close(tmp_sb, &tmp_fd);
  free(name);
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

  int ret = -1;

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
  op_ftable_t *op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_open(): open file table faild!\n");
    goto wt_list_exit;
  }

  if (!sb) {
    if (SHOW_PROC)
      printf("vsfs_open(): opening the disk(mem)\n");
    sb = (struct superblock *)shm_oandm(SHM_NAME, O_RDWR,
                                        PROT_READ | PROT_WRITE, &fd);
  }
  if (!sb) {
    printf("ERR: in vsfs_open(): open disk(memory) faild!\n");
    goto sb_exit;
  }
  struct vsfs_inode *inode_reg =
      (struct vsfs_inode *)((char *)sb +
                            sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  struct vsfs_dir_block *data_reg =
      (struct vsfs_dir_block *)((char *)sb +
                                sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

  if (!sb_cached) {
    sb_cached = (struct vsfs_cached_data *)shm_oandm(
        SHM_CACHE_NAME, O_RDWR, PROT_READ | PROT_WRITE, &fdc);
    if (!sb_cached)
      goto free_sb_ca;
  }

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
  uint16_t target_inode;
  short inode_find = 0;
  for (int i = 0; i < inode_reg->entry; i++) {
    int offset = inode_reg->l1[i / 16];
    if (!strcmp(pathname, data_reg[offset].files[i - 16 * offset].filename)) {
      inode_find = 1;
      target_inode = data_reg[offset].files[i - 16 * offset].inode;
      fd_table->tail->p_tail->next = (path_t *)malloc(sizeof(path_t));
      if (!fd_table->tail->p_tail->next) {
        printf("ERR: in vsfs_open(): malloc walk_through->tail->next <%s>\n",
               strerror(errno));
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
  short optab_find = 0;
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
    for (path_t *tmp = fd_table->tail->p_head; tmp; tmp = tmp->next)
      inode_reg[tmp->inode].atime = now;
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
  shm_close(sb, &fd);
sb_exit:
  shm_close(op_counter, &opfd);

wt_list_exit:
  return ret;
}

// Remember to free the table
/**
 * \return 0 for success, -1 for err
 */
static int vsfs_close(int fildes) {
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
    printf("vsfs_close(): modify the open file table\n");
  // printf("the position of ptr_counter is: %p\n", fd_rm->ptr);
  if (fd_rm->ptr->ptr_counter == 0) {
    printf(
        "ERR: in vsfs_close(): the target's open file table counter is already "
        "0\n");
    goto err_exit;
  }
  fd_rm->ptr->ptr_counter--;
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

  free(fd_rm);

  if (!fd_table->head) {
    free(fd_table);
    shm_close(sb, &fd);
    shm_close(sb_cached, &fdc);
    shm_close(op_counter, &opfd);
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
  int tmp_fd;
  struct superblock *tmp_sb = (struct superblock *)shm_oandm(
      SHM_NAME, O_RDWR, PROT_READ | PROT_WRITE, &tmp_fd);
  if (!tmp_sb)
    goto op_shm_err;
  struct vsfs_inode *tmp_inode_reg =
      (struct vsfs_inode *)((char *)tmp_sb +
                            tmp_sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  struct vsfs_dir_block *tmp_data_reg =
      (struct vsfs_dir_block *)((char *)tmp_sb +
                                tmp_sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

  if (SHOW_PROC)
    printf("vsfs_stat(): finding the file in shm\n");
  uint16_t target_inode = 0;
  short find = 0;
  for (int i = 0; i < tmp_inode_reg->entry; i++) {
    int offset = tmp_inode_reg->l1[i / 16];
    if (SHOW_PROC)
      printf("%s\n", tmp_data_reg[offset].files[i - 16 * offset].filename);
    if (!strcmp(pathname,
                tmp_data_reg[offset].files[i - 16 * offset].filename)) {
      target_inode = tmp_data_reg[offset].files[i - 16 * offset].inode;
      find = 1;
      break;
    }
  }
  if (!find) {
    fre = NULL;
    printf("vsfs_stat(): the filename not find in shm\n");
    goto not_find;
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
  if ((tmp_inode_reg[target_inode].mode & (1 << 3))) {
    fre->mode[0] = 'd';
    fre->entry = tmp_inode_reg[target_inode].entry;
  } else {
    fre->mode[0] = '-';
    fre->size = tmp_inode_reg[target_inode].size;
  }
  if ((tmp_inode_reg[target_inode].mode & (1 << 2)))
    fre->mode[1] = 'r';
  else
    fre->mode[1] = '-';
  if ((tmp_inode_reg[target_inode].mode & (1 << 1)))
    fre->mode[2] = 'w';
  else
    fre->mode[2] = '-';
  if ((tmp_inode_reg[target_inode].mode & (1 << 0)))
    fre->mode[3] = 'x';
  else
    fre->mode[3] = '-';
  fre->mode[4] = '\0';

  if (tmp_inode_reg[target_inode].size == 0 &&
      tmp_inode_reg[target_inode].blocks == 1048576)
    fre->size = 0x100000000;

  fre->blocks = tmp_inode_reg[target_inode].blocks;
  strcpy(fre->ctime, ctime(&(tmp_inode_reg[target_inode].ctime)));
  strcpy(fre->atime, ctime(&(tmp_inode_reg[target_inode].atime)));
  strcpy(fre->mtime, ctime(&(tmp_inode_reg[target_inode].mtime)));

  // print all of the / dir
  // printf("the contain of the / dir is:\n");
  // printf("inode_num\tfilename\n");
  // for(int i=0;i<inode_reg->entry;i++){
  //     int offset = inode_reg->block[i/16];
  //     printf("%2hu\t\t%s\n", data_reg[offset].files[i-16*offset].inode,
  //     data_reg[offset].files[i-16*offset].filename);
  // }

  shm_close(tmp_sb, &tmp_fd);
  return 0;
not_find:
  shm_close(tmp_sb, &tmp_fd);
  return -1;
op_shm_err:
  fre = NULL;
  return -1;
}

void vsfs_print_block_nbr(int fildes) {
  op_ftable_t *target_op = NULL;
  for (fd_table_t *tmp = fd_table->head; tmp; tmp = tmp->next) {
    if (tmp->index == fildes) {
      target_op = tmp->ptr;
    }
  }
  struct vsfs_inode *inode_reg =
      (struct vsfs_inode *)((char *)sb +
                            sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  struct vsfs_data_block *data_reg =
      (struct vsfs_data_block *)((char *)sb +
                                 sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

  uint32_t blks = inode_reg[target_op->inode_nr].blocks;

  printf("level 1:\n");
  for (int i = 0; i < inode_reg[target_op->inode_nr].blocks && i < 49;
       i++, blks--) {
    printf("%u ", inode_reg[target_op->inode_nr].l1[i]);
  }

  printf("\n");
  // PAUSE;
  if (!blks)
    return;

  struct vsfs_pointer_block *ptr_reg;
  for (int i = 0; blks > 0 && i < 5; i++) {
    printf("level 2[%d]: %u\n", i, inode_reg[target_op->inode_nr].l2[i]);
    ptr_reg = (struct vsfs_pointer_block *)(&(
        data_reg[inode_reg[target_op->inode_nr].l2[i]]));
    for (int j = 0; j < 1024 && blks > 0; j++, blks--)
      printf("%u ", ptr_reg->__pointer[j]);
    printf("\n");
    // PAUSE;
  }

  if (!blks)
    return;

  struct vsfs_pointer_block *l3_ptr_reg;
  l3_ptr_reg = (struct vsfs_pointer_block *)&(
      data_reg[inode_reg[target_op->inode_nr].l3[0]]);
  printf("level 3[0]: %u\n", inode_reg[target_op->inode_nr].l3[0]);
  for (int i = 0; blks > 0 && i < 1024; i++) {
    printf("level 2[%d]: %u\n", i, l3_ptr_reg->__pointer[i]);
    ptr_reg =
        (struct vsfs_pointer_block *)(&(data_reg[l3_ptr_reg->__pointer[i]]));
    for (int j = 0; j < 1024 && blks > 0; j++, blks--)
      printf("%u ", ptr_reg->__pointer[j]);
    printf("\n");
    // PAUSE;
  }
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
      if(target_fd)
        *target_fd = tmp;
      if(target_op)
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
/**
 * writting data into disk
 * @param shm_name shm for store data
 * @param fildes fd about the file, use vsfs_open to get
 * @param buf the data you want to write
 * @param nbyte the len of your data
 * @return int for success, -1 for err
 */
static int vsfs_write(int fildes, const void *buf, size_t nbyte) {
  if (SHOW_PROC)
    printf("vsfs_write(): opening the open file table\n");
  if (!op_counter) {
    printf("ERR: in vsfs_write(): open file table do not exist!\n");
    goto err_exit;
  }
  op_ftable_t *op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_write(): open file table faild!\n");
    goto err_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_write(): finding the file\n");
  // finding the fd in fd_table
  fd_table_t *target_fd = NULL;
  op_ftable_t *target_op = NULL;
  int ret = _find_target_in_fdtable(fildes, &target_fd, &target_op);
  if (ret == -1)
    goto err_exit;
  if (target_fd->flags == O_RDONLY) {
    printf("ERR: in vsfs_write(): the open flags error!\n");
    goto err_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_write(): opening the disk(shm)\n");
  if (!sb) {
    printf("ERR: in vsfs_write(): disk(memory) are not exist!\n");
    goto err_exit;
  }
  struct vsfs_inode *inode_reg =
      (struct vsfs_inode *)((char *)sb +
                            sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  struct vsfs_data_block *data_reg =
      (struct vsfs_data_block *)((char *)sb +
                                 sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

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

  if (inode_reg[target_op->inode_nr].blocks >
      (target_op->offset + nbyte) / VSFS_BLOCK_SIZE)
    goto check_for_over_datablock;

  uint32_t total_needed_blocks = (target_op->offset + nbyte) / 4096 -
                                 inode_reg[target_op->inode_nr].blocks;
  if ((target_op->offset + nbyte) % VSFS_BLOCK_SIZE)
    total_needed_blocks++;

  uint32_t count_down = total_needed_blocks;
  uint32_t real_blocks = inode_reg[target_op->inode_nr].blocks;
  uint16_t start_block_l1 = 0;
  uint16_t start_block_l2 = 0;

  // determine ptr level to use
  if (inode_reg[target_op->inode_nr].blocks >= VSFS_LEVEL2_LIMIT) {
    goto level3;
  }
  if (inode_reg[target_op->inode_nr].blocks >= VSFS_LEVEL1_LIMIT) {
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
        inode_reg[target_op->inode_nr].blocks, real_blocks, start_block_l2,
        start_block_l1, total_needed_blocks, count_down);

  // adding new block to level 1
  start_block_l1 = inode_reg[target_op->inode_nr].blocks;
  for (uint16_t i = start_block_l1; i < VSFS_LEVEL1_LIMIT && count_down > 0;
       i++, count_down--) {
    inode_reg[target_op->inode_nr].l1[i] = get_free_dblock(sb_cached);
    inode_reg[target_op->inode_nr].blocks++;
  }
  if (!count_down)
    goto check_for_over_datablock;

  // adding new block to level 2
level2:

  real_blocks = inode_reg[target_op->inode_nr].blocks;
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
        inode_reg[target_op->inode_nr].blocks, real_blocks, start_block_l2,
        start_block_l1, total_needed_blocks, count_down);

  struct vsfs_pointer_block *ptr_reg;
  for (uint16_t i = start_block_l2; i < 5 && count_down > 0; i++) {
    if (start_block_l1 == 0)
      inode_reg[target_op->inode_nr].l2[i] = get_free_dblock(sb_cached);
    ptr_reg = (struct vsfs_pointer_block *)&(
        data_reg[inode_reg[target_op->inode_nr].l2[i]]);
    for (uint16_t j = start_block_l1;
         j < VSFS_POINTER_PER_BLOCK && count_down > 0; j++, count_down--) {
      ptr_reg->__pointer[j] = get_free_dblock(sb_cached);
      inode_reg[target_op->inode_nr].blocks++;
    }
    start_block_l1 = 0;
  }

  if (!count_down)
    goto check_for_over_datablock;
level3:

  real_blocks = inode_reg[target_op->inode_nr].blocks;
  real_blocks -= VSFS_LEVEL2_LIMIT;
  start_block_l1 = real_blocks % 1024;
  start_block_l2 = real_blocks / 1024;
  if (SHOW_PROC)
    printf(
        "the level 3 inside flags is:\n"
        "\ttarget_op->offset = %lu\n"
        "\ttotal blocks = %u\n"
        "\treal_blocks = %u\n"
        "\ttart_block_l2 = %u\n"
        "\ttart_block_l1 = %u\n"
        "\ttotal_needed_blocks = %u\n"
        "\tcount_down = %u\n",
        target_op->offset, inode_reg[target_op->inode_nr].blocks, real_blocks,
        start_block_l2, start_block_l1, total_needed_blocks, count_down);

  if (real_blocks == 0) {
    inode_reg[target_op->inode_nr].l3[0] = get_free_dblock(sb_cached);
    if (SHOW_PROC)
      printf("l3_dblock = %u\n", inode_reg[target_op->inode_nr].l3[0]);
  }

  struct vsfs_pointer_block *l3_ptr_reg;
  l3_ptr_reg = (struct vsfs_pointer_block *)&(
      data_reg[inode_reg[target_op->inode_nr].l3[0]]);
  for (uint16_t i = start_block_l2; i < 1024 && count_down > 0; i++) {
    if (start_block_l1 == 0) {
      l3_ptr_reg->__pointer[i] = get_free_dblock(sb_cached);
      if (SHOW_PROC)
        printf("l2_dblock = %u\n", l3_ptr_reg->__pointer[i]);
    }
    ptr_reg =
        (struct vsfs_pointer_block *)&(data_reg[l3_ptr_reg->__pointer[i]]);
    for (uint16_t j = start_block_l1;
         j < VSFS_POINTER_PER_BLOCK && count_down > 0; j++, count_down--) {
      ptr_reg->__pointer[j] = get_free_dblock(sb_cached);
      if (SHOW_PROC)
        printf("l1_dblock = %u\n", ptr_reg->__pointer[j]);
      inode_reg[target_op->inode_nr].blocks++;
    }
    start_block_l1 = 0;
  }

check_for_over_datablock:
  if (SHOW_PROC)
    printf("vsfs_write(): check if there have idle dblock\n");
  // put the over data block back to free
  uint32_t need_return = 0;
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
    goto start_write;

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
    goto start_write;

re_level1:
  if (SHOW_PROC)
    printf("vsfs_write(): returning level 1 pointer\n");
  for (int i = VSFS_LEVEL1_LIMIT - 1; i >= 0 && need_return > 0;
       i--, need_return--) {
    put_dblock(sb_cached, inode_reg[target_op->inode_nr].l1[i]);
    inode_reg[target_op->inode_nr].blocks--;
  }

start_write:
  if (SHOW_PROC)
    printf("vsfs_write(): modify the inode of a,mtime\n");
  time_t now;
  time(&now);
  inode_reg[target_op->inode_nr].atime = now;
  inode_reg[target_op->inode_nr].mtime = now;

  if (SHOW_PROC)
    printf("vsfs_write(): starting to write the file\n");

  start_block_l1 = target_op->offset / VSFS_BLOCK_SIZE;
  uint16_t offset = target_op->offset % VSFS_BLOCK_SIZE;

  size_t left = nbyte;
  size_t written = 0;
  size_t write_length = 0;

  if (start_block_l1 >= VSFS_LEVEL2_LIMIT)
    goto write_level_3;
  if (start_block_l1 >= VSFS_LEVEL1_LIMIT)
    goto write_level_2;

  if (SHOW_PROC)
    printf("vsfs_write(): writting in level 1\n");
  for (uint16_t i = start_block_l1; i < VSFS_LEVEL1_LIMIT && left > 0; i++) {
    if (left >= VSFS_BLOCK_SIZE - offset)
      write_length = VSFS_BLOCK_SIZE - offset;
    else
      write_length = left;

    if (SHOW_PROC)
      printf(
          "the level 1 inside value:\n"
          "\tstart_block_l1 = %u\n"
          "\toffset = %u\n"
          "\tnbyte = %lu\n"
          "\tleft = %lu\n"
          "\twritten = %lu\n"
          "\twrite_length = %lu\n",
          i, offset, nbyte, left, written, write_length);

    memcpy(
        (char *)(data_reg[inode_reg[target_op->inode_nr].l1[i]].data) + offset,
        (char *)buf + written, write_length);
    written += write_length;
    left -= write_length;
    offset = 0;
    target_op->offset += write_length;
    if (SHOW_PROG)
      progress_bar(written, nbyte);
  }

  if (!left)
    goto written;

write_level_2:
  if (SHOW_PROC)
    printf("vsfs_write(): writting in level 2\n");

  real_blocks = target_op->offset / VSFS_BLOCK_SIZE - VSFS_LEVEL1_LIMIT;
  offset = target_op->offset % VSFS_BLOCK_SIZE;
  start_block_l1 = real_blocks % 1024;
  start_block_l2 = real_blocks / 1024;

  for (uint16_t i = start_block_l2; i < 5 && left > 0; i++) {
    ptr_reg = (struct vsfs_pointer_block *)(&(
        data_reg[inode_reg[target_op->inode_nr].l2[i]]));
    for (uint16_t j = start_block_l1; j < VSFS_POINTER_PER_BLOCK && left > 0;
         j++) {
      if (left >= VSFS_BLOCK_SIZE - offset)
        write_length = VSFS_BLOCK_SIZE - offset;
      else
        write_length = left;

      if (SHOW_PROC)
        printf(
            "the level 2 inside value:\n"
            "\ttarget_op->offset = %lu\n"
            "\treal_blocks = %u\n"
            "\tl1 = %u\n"
            "\tl2 = %u\n"
            "\toffset = %u\n"
            "\tnbyte = %lu\n"
            "\tleft = %lu\n"
            "\twritten = %lu\n"
            "\twrite_length = %lu\n"
            "\tdata_reg = %u\n",
            target_op->offset, real_blocks, j, i, offset, nbyte, left, written,
            write_length, ptr_reg->__pointer[j]);

      memcpy((char *)(data_reg[ptr_reg->__pointer[j]].data) + offset,
             (char *)buf + written, write_length);
      written += write_length;
      left -= write_length;
      offset = 0;
      target_op->offset += write_length;
      if (SHOW_PROG)
        progress_bar(written, nbyte);
    }
    start_block_l1 = 0;
  }

  if (!left)
    goto written;

write_level_3:
  if (SHOW_PROC)
    printf("vsfs_write(): writting in level 3\n");

  real_blocks = target_op->offset / VSFS_BLOCK_SIZE - 5169;
  offset = target_op->offset % VSFS_BLOCK_SIZE;
  start_block_l1 = real_blocks % VSFS_POINTER_PER_BLOCK;
  start_block_l2 = real_blocks / VSFS_POINTER_PER_BLOCK;

  l3_ptr_reg = (struct vsfs_pointer_block *)&(
      data_reg[inode_reg[target_op->inode_nr].l3[0]]);
  for (uint16_t i = start_block_l2; i < VSFS_POINTER_PER_BLOCK && left > 0;
       i++) {
    ptr_reg =
        (struct vsfs_pointer_block *)(&(data_reg[l3_ptr_reg->__pointer[i]]));
    for (uint16_t j = start_block_l1; j < VSFS_POINTER_PER_BLOCK && left > 0;
         j++) {
      if (left >= VSFS_BLOCK_SIZE - offset)
        write_length = VSFS_BLOCK_SIZE - offset;
      else
        write_length = left;

      if (SHOW_PROC)
        printf(
            "writting the level 3 inside value:\n"
            "\ttarget_op->offset = %lu\n"
            "\treal_blocks = %u\n"
            "\tl1 = %u\n"
            "\tl2 = %u\n"
            "\toffset = %u\n"
            "\tnbyte = %lu\n"
            "\tleft = %lu\n"
            "\twritten = %lu\n"
            "\twrite_length = %lu\n"
            "\tdata_reg = %u\n",
            target_op->offset, real_blocks, j, i, offset, nbyte, left, written,
            write_length, ptr_reg->__pointer[j]);

      memcpy((char *)(data_reg[ptr_reg->__pointer[j]].data) + offset,
             (char *)buf + written, write_length);
      written += write_length;
      left -= write_length;
      offset = 0;
      target_op->offset += write_length;
      if (SHOW_PROG)
        progress_bar(written, nbyte);
    }
    start_block_l1 = 0;
  }

written:
  inode_reg[target_op->inode_nr].size = target_op->offset;

  return nbyte;

err_exit:
  return -1;
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
  if (SHOW_PROC)
    printf("vsfs_read(): opening the open file table\n");
  if (!op_counter) {
    printf("ERR: in vsfs_read(): open file table are not exist!\n");
    goto err_exit;
  }
  op_ftable_t *op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_read(): open file table faild!\n");
    goto err_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_read(): finding the file\n");
  // finding the fd in fd_table
  fd_table_t *target_fd = NULL;
  op_ftable_t *target_op = NULL;
  int ret = _find_target_in_fdtable(fildes, &target_fd, &target_op);
  if (ret == -1)
    goto err_exit;
  if (target_fd->flags == O_WRONLY) {
    printf("ERR: in vsfs_read(): the open flags error!\n");
    goto err_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_read(): opening the disk(shm)\n");
  if (!sb) {
    printf("ERR: in vsfs_read(): disk(memory) are not exist!\n");
    goto err_exit;
  }
  struct vsfs_inode *inode_reg =
      (struct vsfs_inode *)((char *)sb +
                            sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  struct vsfs_data_block *data_reg =
      (struct vsfs_data_block *)((char *)sb +
                                 sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

  if (SHOW_PROC)
    printf("vsfs_read(): starting to read the file\n");

  if (SHOW_PROC)
    printf("vsfs_read(): modify the inode of a,mtime\n");
  time_t now;
  time(&now);
  inode_reg[target_op->inode_nr].atime = now;

  if (SHOW_PROC)
    printf("vsfs_read(): starting to read the file\n");

  uint16_t start_block_l1 = target_op->offset / VSFS_BLOCK_SIZE;
  uint16_t offset = target_op->offset % VSFS_BLOCK_SIZE;
  uint64_t file_size = inode_reg[target_op->inode_nr].size;

  if (inode_reg[target_op->inode_nr].size == 0 &&
      inode_reg[target_op->inode_nr].blocks == 1048576)
    file_size = 0x100000000;

  size_t left = 0;
  size_t padded = 0;
  if (file_size >= target_op->offset + nbyte)
    left = nbyte;
  else {
    if (file_size == target_op->offset)
      goto err_exit;
    else {
      left = file_size - target_op->offset;
      padded = nbyte - left;
    }
  }
  size_t readed = 0;
  size_t read_length = 0;
  if (SHOW_PROC)
    printf(
        "the reading flags:\n"
        "\tstart_block_l1= %u\n"
        "\toffset= %u\n"
        "\tleft= %lu\n"
        "\tpadded= %lu\n"
        "\treaded= %lu\n"
        "\tread_length= %lu\n",
        start_block_l1, offset, left, padded, readed, read_length);

  if (start_block_l1 >= VSFS_LEVEL2_LIMIT)
    goto read_level_3;
  if (start_block_l1 >= VSFS_LEVEL1_LIMIT)
    goto read_level_2;

  if (SHOW_PROC)
    printf("vsfs_read(): reading in level 1\n");
  for (uint16_t i = start_block_l1; i < VSFS_LEVEL1_LIMIT && left > 0; i++) {
    if (left >= VSFS_BLOCK_SIZE - offset)
      read_length = VSFS_BLOCK_SIZE - offset;
    else
      read_length = left;

    if (SHOW_PROC)
      printf(
          "the level 1 inside value:\n"
          "\tstart_block_l1 = %u\n"
          "\toffset = %u\n"
          "\tnbyte = %lu\n"
          "\tleft = %lu\n"
          "\treaded = %lu\n"
          "\tread_length = %lu\n",
          i, offset, nbyte, left, readed, read_length);

    memcpy(
        (char *)buf + readed,
        (char *)(data_reg[inode_reg[target_op->inode_nr].l1[i]].data) + offset,
        read_length);
    readed += read_length;
    left -= read_length;
    offset = 0;
    target_op->offset += read_length;
  }

  if (!left)
    goto readed;

read_level_2:
  if (SHOW_PROC)
    printf("vsfs_read(): reading in level 2\n");

  uint32_t real_blocks = target_op->offset / VSFS_BLOCK_SIZE - 49;
  offset = target_op->offset % VSFS_BLOCK_SIZE;
  start_block_l1 = real_blocks % 1024;
  uint16_t start_block_l2 = real_blocks / 1024;

  struct vsfs_pointer_block *ptr_reg;
  for (uint16_t i = start_block_l2; i < 5 && left > 0; i++) {
    ptr_reg = (struct vsfs_pointer_block *)(&(
        data_reg[inode_reg[target_op->inode_nr].l2[i]]));
    for (uint16_t j = start_block_l1; j < VSFS_POINTER_PER_BLOCK && left > 0;
         j++) {
      if (left >= VSFS_BLOCK_SIZE - offset)
        read_length = VSFS_BLOCK_SIZE - offset;
      else
        read_length = left;

      if (SHOW_PROC)
        printf(
            "the level 2 inside value:\n"
            "\ttarget_op->offset = %lu\n"
            "\treal_blocks = %u\n"
            "\tl1 = %u\n"
            "\tl2 = %u\n"
            "\toffset = %u\n"
            "\tnbyte = %lu\n"
            "\tleft = %lu\n"
            "\treaded = %lu\n"
            "\tread_length = %lu\n"
            "\tdata_reg = %u\n",
            target_op->offset, real_blocks, j, i, offset, nbyte, left, readed,
            read_length, ptr_reg->__pointer[j]);

      memcpy((char *)buf + readed,
             (char *)(data_reg[ptr_reg->__pointer[j]].data) + offset,
             read_length);
      readed += read_length;
      left -= read_length;
      offset = 0;
      target_op->offset += read_length;
    }
    start_block_l1 = 0;
  }

  if (!left)
    goto readed;

read_level_3:
  if (SHOW_PROC)
    printf("vsfs_write(): reading in level 3\n");

  real_blocks = target_op->offset / VSFS_BLOCK_SIZE - 5169;
  offset = target_op->offset % VSFS_BLOCK_SIZE;
  start_block_l1 = real_blocks % VSFS_POINTER_PER_BLOCK;
  start_block_l2 = real_blocks / VSFS_POINTER_PER_BLOCK;

  struct vsfs_pointer_block *l3_ptr_reg = (struct vsfs_pointer_block *)&(
      data_reg[inode_reg[target_op->inode_nr].l3[0]]);
  for (uint16_t i = start_block_l2; i < VSFS_POINTER_PER_BLOCK && left > 0;
       i++) {
    ptr_reg =
        (struct vsfs_pointer_block *)(&(data_reg[l3_ptr_reg->__pointer[i]]));
    for (uint16_t j = start_block_l1; j < VSFS_POINTER_PER_BLOCK && left > 0;
         j++) {
      if (left >= VSFS_BLOCK_SIZE - offset)
        read_length = VSFS_BLOCK_SIZE - offset;
      else
        read_length = left;

      if (SHOW_PROC)
        printf(
            "the level 3 inside value:\n"
            "\ttarget_op->offset = %lu\n"
            "\treal_blocks = %u\n"
            "\tl1 = %u\n"
            "\tl2 = %u\n"
            "\toffset = %u\n"
            "\tnbyte = %lu\n"
            "\tleft = %lu\n"
            "\treaded = %lu\n"
            "\tread_length = %lu\n"
            "\tdata_reg = %u\n",
            target_op->offset, real_blocks, j, i, offset, nbyte, left, readed,
            read_length, ptr_reg->__pointer[j]);

      memcpy((char *)buf + readed,
             (char *)(data_reg[ptr_reg->__pointer[j]].data) + offset,
             read_length);
      readed += read_length;
      left -= read_length;
      offset = 0;
      target_op->offset += read_length;
    }
    start_block_l1 = 0;
  }

readed:

  memset((char *)buf + readed, '\0', padded);

  return nbyte;

err_exit:
  return -1;
}

static off_t vsfs_lseek(int fd, off_t offset, int whence) {
  if (SHOW_PROC)
    printf("vsfs_lseek(): checking the init\n");
  if (!op_counter) {
    printf("ERR: in vsfs_lseek(): you need to open the file first!\n");
    goto err_exit;
  }
  op_ftable_t *op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_lseek(): open file table faild!\n");
    goto err_exit;
  }

  struct vsfs_inode *inode_reg =
      (struct vsfs_inode *)((char *)sb +
                            sb->info.ofs_iregion * VSFS_BLOCK_SIZE);

  if (SHOW_PROC)
    printf("vsfs_lseek(): finding the file\n");
  // finding the fd in fd_table
  op_ftable_t *target_op = NULL;
  int re = _find_target_in_fdtable(fd, NULL, &target_op);
  if (re == -1)
    goto err_exit;

  uint64_t file_size = inode_reg[target_op->inode_nr].size;
  if (inode_reg[target_op->inode_nr].size == 0 &&
      inode_reg[target_op->inode_nr].blocks == 1048576)
    file_size = 0x100000000;

  off_t ret;
  switch (whence) {
    case SEEK_SET:
      if (offset > file_size) {
        printf("ERR: in vsfs_lseek(): setting offset over the file size\n");
        goto err_exit;
      }
      ret = target_op->offset = offset;
      break;
    case SEEK_END:
      target_op->offset = inode_reg[target_op->inode_nr].size;
    case SEEK_CUR:
      if (target_op->offset + offset > file_size) {
        printf("ERR: in vsfs_lseek(): setting offset over the file size\n");
        goto err_exit;
      }
      ret = target_op->offset += offset;
      break;
    default:
      printf("ERR: in vsfs_lseek(): not know where to set the offset\n");
      goto err_exit;
  }

  return ret;

err_exit:
  return -1;
}

#endif /*VSFSIO_H*/
