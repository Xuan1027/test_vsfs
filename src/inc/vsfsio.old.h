#ifndef VSFSIO_H
#define VSFSIO_H

#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"

#define SHOW_PROC 1
#define OP_LIMIT VSFS_OPTAB_SIZE / sizeof(op_ftable_t)

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
    uint16_t blocks; /* Total number of data blocks count */
    union {
        uint32_t size; /* File: size in byte || Dir: entry num */
        uint32_t entry;
    };
    char atime[39]; /* Access time */
    char ctime[39]; /* Inode change time */
    char mtime[39]; /* Modification time */
} file_stat_t;

// the process's file descriptor talbe
struct fd_table_list *fd_table = NULL;

static int vsfs_creat(char *shm_name, char *file_name) __attribute__((unused));
static int vsfs_stat(char *shm_name, char *pathname, file_stat_t *fre)
    __attribute__((unused));
static int vsfs_open(char *shm_name, char *pathname, int flags)
    __attribute__((unused));
static int vsfs_close(int fildes) __attribute__((unused));
static int vsfs_write(char *shm_name, int fildes, const void *buf, size_t nbyte)
    __attribute__((unused));
static int vsfs_read(char *shm_name, int fildes, void *buf, size_t nbyte)
    __attribute__((unused));
// static off_t vsfs_lseek(int fd, off_t offset, int whence)
// __attribute__((unused));

/**
 * create a new file
 * \param shm_name the share memory name
 */
static int vsfs_creat(char *shm_name, char *file_name) {
  if (SHOW_PROC)
    printf("vsfs_creat(): init the setting\n");
  if (strlen(file_name) >= VSFS_FILENAME_LEN) {
    printf("the file_name's lens over the limit!\n");
    goto shm_err_exit;
  }

  int fd, fdc;
  char *name = (char *)malloc(VSFS_FILENAME_LEN);
  char *shm_cache_name = (char *)malloc(strlen(shm_name) + 8);

  memcpy(shm_cache_name, shm_name, strlen(shm_name));
  strncpy(shm_cache_name + strlen(shm_name), "_cached", 8);
  // printf("shm_cached_name = <%s>\n", shm_cache_name);

  strcpy(name, file_name);

  struct superblock *sb = (struct superblock *)shm_oandm(
      shm_name, O_RDWR, PROT_READ | PROT_WRITE, &fd);
  if (!sb)
    goto shm_err_exit;
  struct vsfs_cached_data *sb_cached = (struct vsfs_cached_data *)shm_oandm(
      shm_cache_name, O_RDWR, PROT_READ | PROT_WRITE, &fdc);
  if (!sb_cached)
    goto sb_err_exit;
  struct vsfs_inode *inode_reg =
      (struct vsfs_inode *)((char *)sb +
                            sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  struct vsfs_dir_block *data_reg =
      (struct vsfs_dir_block *)((char *)sb +
                                sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

  uint32_t f_inode = get_free_inode(sb_cached),
           f_dblock = inode_reg->entry / 16;
  uint32_t d_entry = inode_reg->entry - f_dblock * 16;
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
  if (inode_reg->blocks < 50)
    goto adding_level1_block;
  printf(
      "the / inode dentry bigger than 50 blocks, need to turn to level 2 "
      "pointer!\n");
  // 0~49 level 1
  // 50~54 level 2
  // 55 level 3
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
  data_reg[f_dblock].files[d_entry].inode = f_inode;
  memcpy(data_reg[f_dblock].files[d_entry].filename, name, strlen(name) + 1);
  inode_reg->entry++;
  free(name);

  // getting the time at this moment
  time_t now;
  time(&now);
  inode_reg->atime = now;
  inode_reg->mtime = now;

  // setting the new inode
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

  shm_close(sb, &fd);
  shm_close(sb_cached, &fdc);
  free(shm_cache_name);

  return 0;
sup_cached_err_exit:
  shm_close(sb_cached, &fdc);
sb_err_exit:
  shm_close(sb, &fd);
  free(shm_cache_name);
shm_err_exit:
  return -1;
}

/**
 * open a file
 * @return int for success, -1 for error
 */
static int vsfs_open(char *shm_name, char *pathname, int flags) {
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

  /**
   * first the open file table
   * second the file descriptor table
   * then modify the inode
   */
  if (SHOW_PROC)
    printf("vsfs_open(): opening the shm\n");
  int opfd, fd;
  unsigned short *op_counter = (unsigned short *)shm_oandm(
      "optab", O_RDWR, PROT_READ | PROT_WRITE, &opfd);
  if (!op_counter) {
    printf("ERR: in vsfs_open(): open file table faild!\n");
    goto wt_list_exit;
  }
  op_ftable_t *op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_open(): open file table faild!\n");
    goto wt_list_exit;
  }
  // printf("in vsfs_open(): the open file table position is: %p\n", op_ftable);
  // TODO: find the pathname of the file
  struct superblock *sb = (struct superblock *)shm_oandm(
      shm_name, O_RDWR, PROT_READ | PROT_WRITE, &fd);
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

  shm_close(sb, &fd);
  shm_close(op_counter, &opfd);

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
  /**
   * need to close fd_table, path, open file table counter--
   */

  int opfd;
  unsigned short *op_counter = (unsigned short *)shm_oandm(
      "optab", O_RDWR, PROT_READ | PROT_WRITE, &opfd);
  if (!op_counter) {
    printf("ERR: in vsfs_open(): open file table faild!\n");
    goto err_exit;
  }
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
  shm_close(op_counter, &opfd);
  return 0;
err_exit:
  shm_close(op_counter, &opfd);
  return -1;
}

/**
 * looking the status of an inode
 * \param shm_name the share memory name
 * \param pathename the file wantted to see for the inode
 * \param ret the status of an inode
 * \return 0 for success, -1 for error
 */
static int vsfs_stat(char *shm_name, char *pathname, file_stat_t *fre) {
  int fd;
  struct superblock *sb = (struct superblock *)shm_oandm(
      shm_name, O_RDWR, PROT_READ | PROT_WRITE, &fd);
  if (!sb)
    goto op_shm_err;
  struct vsfs_inode *inode_reg =
      (struct vsfs_inode *)((char *)sb +
                            sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  struct vsfs_dir_block *data_reg =
      (struct vsfs_dir_block *)((char *)sb +
                                sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

  if (SHOW_PROC)
    printf("vsfs_stat(): finding the file in shm\n");
  uint16_t target_inode = 0;
  short find = 0;
  for (int i = 0; i < inode_reg->entry; i++) {
    int offset = inode_reg->l1[i / 16];
    if (!strcmp(pathname, data_reg[offset].files[i - 16 * offset].filename)) {
      target_inode = data_reg[offset].files[i - 16 * offset].inode;
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

  shm_close(sb, &fd);
  return 0;
not_find:
  shm_close(sb, &fd);
  return -1;
op_shm_err:
  fre = NULL;
  return -1;
}

/**
 * writting data into disk
 * @param shm_name shm for store data
 * @param fildes fd about the file, use vsfs_open to get
 * @param buf the data you want to write
 * @param nbyte the len of your data
 * @return int for success, -1 for err
 */
static int vsfs_write(char *shm_name, int fildes, const void *buf,
                      size_t nbyte) {
  if (SHOW_PROC)
    printf("vsfs_write(): opening the open file table\n");
  int opfd, fdc, fd;
  unsigned short *op_counter = (unsigned short *)shm_oandm(
      "optab", O_RDWR, PROT_READ | PROT_WRITE, &opfd);
  if (!op_counter) {
    printf("ERR: in vsfs_write(): open file table faild!\n");
    goto shm_err_exit;
  }
  op_ftable_t *op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_write(): open file table faild!\n");
    goto shm_err_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_write(): finding the file\n");
  // finding the fd in fd_table
  short fd_find = 0;
  fd_table_t *target_fd = NULL;
  op_ftable_t *target_op = NULL;
  for (fd_table_t *tmp = fd_table->head; tmp; tmp = tmp->next) {
    if (tmp->index == fildes) {
      fd_find = 1;
      target_fd = tmp;
      target_op = tmp->ptr;
    }
  }
  if (!fd_find) {
    printf("ERR: in vsfs_write(): not found the fd in fd_table\n");
    goto op_exit;
  }
  if (target_fd->flags == O_RDONLY) {
    printf("ERR: in vsfs_write(): the open flags error!\n");
    goto op_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_write(): opening the disk(shm)\n");
  struct superblock *sb = (struct superblock *)shm_oandm(
      shm_name, O_RDWR, PROT_READ | PROT_WRITE, &fd);
  if (!sb) {
    printf("ERR: in vsfs_write(): open disk(memory) faild!\n");
    goto op_exit;
  }
  struct vsfs_inode *inode_reg =
      (struct vsfs_inode *)((char *)sb +
                            sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  struct vsfs_data_block *data_reg =
      (struct vsfs_data_block *)((char *)sb +
                                 sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

  if (SHOW_PROC)
    printf("vsfs_write(): opening the cached\n");
  char *shm_cache_name = (char *)malloc(strlen(shm_name) + 8);

  memcpy(shm_cache_name, shm_name, strlen(shm_name));
  strncpy(shm_cache_name + strlen(shm_name), "_cached", 8);
  struct vsfs_cached_data *sb_cached = (struct vsfs_cached_data *)shm_oandm(
      shm_cache_name, O_RDWR, PROT_READ | PROT_WRITE, &fdc);
  if (!sb_cached) {
    printf("ERR: in vsfs_write(): cannot open the vsfa_cached\n");
    goto sb_exit;
  }

  if (SHOW_PROC)
    printf(
        "vsfs_write(): checking if the size in target inode is able to "
        "write\n");
  // order for another block to store data
  unsigned int start_block_l1, start_block_l2;
  unsigned int need_alloc_level3 = 0, need_alloc_level2 = 0;
  if (inode_reg[target_op->inode_nr].blocks * VSFS_BLOCK_SIZE >=
      target_op->offset + nbyte)
    goto check_for_over_datablock;

  int total_need_alloc = (target_op->offset + nbyte) / VSFS_BLOCK_SIZE -
                         inode_reg[target_op->inode_nr].blocks;
  if ((target_op->offset + nbyte) % VSFS_BLOCK_SIZE)
    total_need_alloc++;

  if (inode_reg[target_op->inode_nr].blocks + total_need_alloc <=
      VSFS_LEVEL1_LIMIT)
    goto using_level1_pointer;
  unsigned int max_alloc_level1 = 0;
  if (inode_reg[target_op->inode_nr].blocks < VSFS_LEVEL1_LIMIT)
    max_alloc_level1 =
        VSFS_LEVEL1_LIMIT - inode_reg[target_op->inode_nr].blocks;

  unsigned int l2_ptr;
  // printf("vsfs_write(): need to using the level2 pointer\n");
  if (inode_reg[target_op->inode_nr].blocks + total_need_alloc <=
      VSFS_LEVEL2_LIMIT)
    goto using_level2_pointer;
  unsigned int max_alloc_level2 = 0;
  if (inode_reg[target_op->inode_nr].blocks < VSFS_LEVEL2_LIMIT)
    max_alloc_level2 = VSFS_LEVEL2_LIMIT -
                       inode_reg[target_op->inode_nr].blocks - max_alloc_level1;

  // printf("vsfs_write(): need to using the level3 pointer\n");
  if (inode_reg[target_op->inode_nr].blocks + total_need_alloc <=
      VSFS_LEVEL3_LIMIT)
    goto using_level3_pointer;
  printf("ERR: in vsfs_write(): too much data to write\n");
  goto err_exit;

using_level3_pointer:
  need_alloc_level3 = total_need_alloc - max_alloc_level2 - max_alloc_level1;
  printf("you are using the level3 pointer\n");
  printf("ERR!\n");
  goto err_exit;
using_level2_pointer:
  if (SHOW_PROC)
    printf("vsfs_write(): adding level 2 dblock\n");
  need_alloc_level2 = total_need_alloc - max_alloc_level1;
  if (!max_alloc_level1) {
    start_block_l2 = (inode_reg[target_op->inode_nr].blocks -
                      need_alloc_level3 - VSFS_LEVEL1_PTR) /
                     VSFS_POINTER_PER_BLOCK;
    start_block_l1 = (inode_reg[target_op->inode_nr].blocks -
                      need_alloc_level3 - VSFS_LEVEL1_PTR) %
                     VSFS_POINTER_PER_BLOCK;
  } else {
    start_block_l2 = 0;
    start_block_l1 = 0;
  }
  l2_ptr = start_block_l2 + need_alloc_level2 / VSFS_POINTER_PER_BLOCK;
  if (need_alloc_level2 % 1024)
    l2_ptr++;

  for (; start_block_l2 < l2_ptr; start_block_l2++) {
    if (!start_block_l1) {
      uint32_t l1_dblock = get_free_dblock(sb_cached);
      inode_reg[target_op->inode_nr].l2[start_block_l2] = l1_dblock;
    }
    struct vsfs_pointer_block *ptr_reg = (struct vsfs_pointer_block *)(&(
        data_reg[inode_reg[target_op->inode_nr].l2[start_block_l2]]));
    for (int j = need_alloc_level2;
         start_block_l1 < VSFS_POINTER_PER_BLOCK && j > 0;
         start_block_l1++, j--) {
      uint32_t new_dblock = get_free_dblock(sb_cached);
      ptr_reg->__pointer[start_block_l1] = new_dblock;
      inode_reg[target_op->inode_nr].blocks++;
    }
    start_block_l1 = 0;
  }
  total_need_alloc -= need_alloc_level2;

using_level1_pointer:
  if (SHOW_PROC)
    printf("vsfs_write(): adding level 1 dblock\n");
  start_block_l1 = inode_reg[target_op->inode_nr].blocks - need_alloc_level3 -
                   need_alloc_level2;
  for (; total_need_alloc > 0; total_need_alloc--, start_block_l1++) {
    uint32_t dblock = get_free_dblock(sb_cached);
    inode_reg[target_op->inode_nr].l1[start_block_l1] = dblock;
    inode_reg[target_op->inode_nr].blocks++;
  }

check_for_over_datablock:
  // if(SHOW_PROC)
  //     printf("vsfs_write(): check if there have idle dblock\n");
  // // put the over data block back to free
  // if((target_op->offset+nbyte)/VSFS_BLOCK_SIZE+1 <
  // inode_reg[target_op->inode_nr].blocks){
  //     int need_return =
  //     inode_reg[target_op->inode_nr].blocks-(target_op->offset+nbyte)/VSFS_BLOCK_SIZE-1;
  //     for(int i=0;i<need_return;i++){
  //         inode_reg[target_op->inode_nr].blocks--;
  //         put_dblock(sb_cached,inode_reg[target_op->inode_nr].l1[inode_reg[target_op->inode_nr].blocks]);
  //     }
  // }

  if (SHOW_PROC)
    printf("vsfs_write(): modify the inode of a,mtime\n");
  time_t now;
  time(&now);
  inode_reg[target_op->inode_nr].atime = now;
  inode_reg[target_op->inode_nr].mtime = now;

  if (SHOW_PROC)
    printf("vsfs_write(): starting to write the file\n");
  // const char* cbuf = buf;
  // unsigned short start_block =
  // inode_reg[target_op->inode_nr].l1[target_op->offset/VSFS_BLOCK_SIZE];
  // unsigned short end_block =
  // inode_reg[target_op->inode_nr].l1[(target_op->offset +
  // nbyte)/VSFS_BLOCK_SIZE]; uint32_t start_offset =
  // target_op->offset%VSFS_BLOCK_SIZE; if(!((target_op->offset +
  // nbyte)%VSFS_BLOCK_SIZE))
  //     end_block = inode_reg[target_op->inode_nr].l1[(target_op->offset +
  //     nbyte-1)/VSFS_BLOCK_SIZE];

  // if(SHOW_PROC){
  //     printf("vsfs_write(): writing to the first block\n");
  //     printf("vsfs_write(): printing the inside value\n");
  //     printf("\tnbyte = %ld\n"
  //     "\tstart_block = %u\n"
  //     "\tend_block = %u\n"
  //     "\tstart_offset = %u\n",
  //     nbyte,start_block,end_block,start_offset
  //     );
  // }

  // // writting size less than one block
  // if(start_block == end_block){
  //     memcpy((char*)(data_reg[start_block].data) + start_offset, cbuf,
  //     nbyte); goto written;
  // }
  // else{
  //     // writing the first block
  //     memcpy((char*)(data_reg[start_block].data) + start_offset, cbuf,
  //     VSFS_BLOCK_SIZE - start_offset);
  // }

  // uint32_t buf_offset = VSFS_BLOCK_SIZE - start_offset;
  // if(SHOW_PROC){
  //     printf("vsfs_write(): writing to middle block\n");
  //     printf("\tbuf_offset = %u\n", buf_offset);
  // }
  // // writting size bigger than one block
  // for(unsigned short start=start_block+1;start<end_block;start++){
  //     memcpy(data_reg[start].data, &cbuf[buf_offset], VSFS_BLOCK_SIZE);
  //     buf_offset+=VSFS_BLOCK_SIZE;
  // }

  // if(SHOW_PROC){
  //     printf("vsfs_write(): writing to the last block\n");
  //     printf("\tbuf_offset = %u\n", buf_offset);
  // }
  // // writting the last block
  // memcpy(data_reg[end_block].data, &cbuf[buf_offset], nbyte-buf_offset);

written:
  inode_reg[target_op->inode_nr].size = target_op->offset + nbyte;
  target_op->offset += nbyte;

  shm_close(sb_cached, &fdc);
  shm_close(sb, &fd);
  shm_close(op_counter, &opfd);
  return nbyte;

err_exit:
  shm_close(sb_cached, &fdc);
sb_exit:
  shm_close(sb, &fd);
op_exit:
  shm_close(op_counter, &opfd);
shm_err_exit:
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
static int vsfs_read(char *shm_name, int fildes, void *buf, size_t nbyte) {
  if (SHOW_PROC)
    printf("vsfs_read(): opening the open file table\n");
  int opfd, fd;
  unsigned short *op_counter = (unsigned short *)shm_oandm(
      "optab", O_RDWR, PROT_READ | PROT_WRITE, &opfd);
  if (!op_counter) {
    printf("ERR: in vsfs_read(): open file table faild!\n");
    goto shm_err_exit;
  }
  op_ftable_t *op_ftable = (op_ftable_t *)(op_counter + 1);
  if (!op_ftable) {
    printf("ERR: in vsfs_read(): open file table faild!\n");
    goto shm_err_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_read(): finding the file\n");
  // finding the fd in fd_table
  short fd_find = 0;
  fd_table_t *target_fd = NULL;
  op_ftable_t *target_op = NULL;
  for (fd_table_t *tmp = fd_table->head; tmp; tmp = tmp->next) {
    if (tmp->index == fildes) {
      fd_find = 1;
      target_fd = tmp;
      target_op = tmp->ptr;
    }
  }
  if (!fd_find) {
    printf("ERR: in vsfs_read(): not found the fd in fd_table\n");
    goto op_exit;
  }
  if (target_fd->flags == O_WRONLY) {
    printf("ERR: in vsfs_read(): the open flags error!\n");
    goto op_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_read(): opening the disk(shm)\n");
  struct superblock *sb = (struct superblock *)shm_oandm(
      shm_name, O_RDWR, PROT_READ | PROT_WRITE, &fd);
  if (!sb) {
    printf("ERR: in vsfs_read(): open disk(memory) faild!\n");
    goto op_exit;
  }
  struct vsfs_inode *inode_reg =
      (struct vsfs_inode *)((char *)sb +
                            sb->info.ofs_iregion * VSFS_BLOCK_SIZE);
  struct vsfs_data_block *data_reg =
      (struct vsfs_data_block *)((char *)sb +
                                 sb->info.ofs_dregion * VSFS_BLOCK_SIZE);

  if (target_op->offset + nbyte > inode_reg[target_op->inode_nr].size) {
    printf("ERR: in vsfs_read(): read over the size of the file\n");
    goto sb_exit;
  }

  if (SHOW_PROC)
    printf("vsfs_read(): starting to read the file\n");
  // char* cbuf = buf;
  // unsigned short start_block =
  // inode_reg[target_op->inode_nr].l1[target_op->offset/VSFS_BLOCK_SIZE];
  // unsigned short end_block =
  // inode_reg[target_op->inode_nr].l1[(target_op->offset +
  // nbyte)/VSFS_BLOCK_SIZE]; uint32_t start_offset =
  // target_op->offset%VSFS_BLOCK_SIZE; if(!((target_op->offset +
  // nbyte)%VSFS_BLOCK_SIZE))
  //     end_block = inode_reg[target_op->inode_nr].l1[(target_op->offset +
  //     nbyte-1)/VSFS_BLOCK_SIZE];

  // if(SHOW_PROC){
  //     printf("vsfs_read(): reading the first block\n");
  //     printf("vsfs_read(): printing the inside value\n");
  //     printf("\tnbyte = %ld\n"
  //     "\tstart_block = %u\n"
  //     "\tend_block = %u\n"
  //     "\tstart_offset = %u\n",
  //     nbyte,start_block,end_block,start_offset
  //     );
  // }

  // // reading less than one block
  // if(start_block == end_block){
  //     memcpy(cbuf, (char*)(data_reg[start_block].data) + start_offset,
  //     nbyte); goto read_end;
  // }
  // else{
  //     // reading the first block
  //     memcpy(cbuf, (char*)(data_reg[start_block].data) + start_offset,
  //     VSFS_BLOCK_SIZE - start_offset);
  // }

  // uint32_t buf_offset = VSFS_BLOCK_SIZE - start_offset;
  // if(SHOW_PROC){
  //     printf("vsfs_read(): writing to middle block\n");
  //     printf("\tbuf_offset = %u\n", buf_offset);
  // }
  // // reading size bigger than one block
  // for(unsigned short start=start_block+1;start<end_block;start++){
  //     memcpy(cbuf+buf_offset, data_reg[start].data, VSFS_BLOCK_SIZE);
  //     buf_offset+=VSFS_BLOCK_SIZE;
  // }

  // if(SHOW_PROC){
  //     printf("vsfs_read(): writing to the last block\n");
  //     printf("\tbuf_offset = %u\n", buf_offset);
  // }
  // // reading the last block
  // memcpy(&cbuf[buf_offset], data_reg[end_block].data, nbyte-buf_offset);

read_end:
  shm_close(sb, &fd);
  shm_close(op_counter, &opfd);
  return nbyte;

sb_exit:
  shm_close(sb, &fd);
op_exit:
  shm_close(op_counter, &opfd);
shm_err_exit:
  return -1;
}

#endif /*VSFSIO_H*/
