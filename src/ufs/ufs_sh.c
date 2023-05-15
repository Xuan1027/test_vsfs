#include "ufs.h"
struct vsfs_cached_data *sb = NULL;
int sbf;

int stat_init(stat_t* st) {
  st->cwd = "/";
  return 0;
}

int stat_clean(stat_t* st) { return 0; }

int env_init(stat_t* st) {
  init_spdk();

  if (!sb) {
    sb = (struct vsfs_cached_data*)shm_oandm(
        SHM_CACHE_NAME, O_RDWR, PROT_READ | PROT_WRITE, &sbf);
    if (!sb) {
      printf("Error: open super block cached faild!\n");
    }
  }

  return 0;
}

int env_exit(stat_t* st) {
  shm_close((void**)&sb, sbf);

  exit_spdk();
  return 0;
}

int call_init_spdk(char* name) {
  init_spdk();
  init_vsfs(name);
  exit_spdk();

  return 0;
}

int call_mount_spdk(char* name) {
  init_spdk();
  mount_vsfs(name);
  exit_spdk();

  return 0;
}

int call_unmount_spdk(char* name) {
  init_spdk();
  unmount_vsfs(name);
  exit_spdk();

  return 0;
}

void prompt(stat_t* st) {
  printf("UFS@~%s >", st->cwd);
  return;
}

int cmd_ls() {
  struct vsfs_inode* inode_reg = sb->inode_reg;
  struct vsfs_dir_block* root_dir_info = alloc_dma_buffer(VSFS_BLOCK_SIZE);

  printf("%p\n", sb);
  for (uint32_t i = 0; i < inode_reg->entry; i++) {
    if (i % 16 == 0) {
      read_spdk(root_dir_info,
                sb->sbi.ofs_dregion + inode_reg->l1[i], 1,
                IO_QUEUE);
    }

    printf("%s ", root_dir_info->files[i].filename);
  }
  printf("\n");

  free_dma_buffer(root_dir_info);

  return 0;
}

int cmd_create() {
  int ret = -1;
  char *input = (char*)malloc(256 * sizeof(char)), *file_name;
  const char* d = " ";
  ret = scanf("%[^\n]", input);
  getchar();
  if (!ret) {
    handle_error("create: scanf");
    return -1;
  }

  file_name = strtok(input, d);
  while (file_name != NULL) {
    printf("creating %s\n", file_name);
    vsfs_creat(file_name, 0);
    file_name = strtok(NULL, d);
  }

  free(input);
  return 0;
}

int cmd_pwd(stat_t* st) {
  printf("%s\n", st->cwd);
  return 0;
}

int cmd_stat() { return 0; }
int cmd_rm() { return 0; }
int cmd_read() { return 0; }
int cmd_write() { return 0; }
int cmd_help() {
  printf(
      "Command:\n"
      "create\t<file> \tcreate <file> inside UFS\n"
      "help         \tprint this helpful page\n"
      "ls           \tlist all the containt under cwd\n"
      "pwd          \tprint the working directory\n"
      "quit, q      \tleave UFS\n"
      "read  \t<file> \tread data inside <file>\n"
      "rm    \t<file> \tremove the <file> from UFS\n"
      "stat  \t<file> \tlist the info about <file>\n"
      "write \t<file> \twrite data into <file>\n");
  return 0;
}

void print_ufs_info() {
  printf("\tversion: v1.0.0\n");
  printf("\tcreated by xuan, uzuki\n");
  cmd_help();
  return;
}

void p_sys_info() {
  printf("_MY_SSD_BLOCK_SIZE_=%d\n", _MY_SSD_BLOCK_SIZE_);
  printf("VSFS_BLOCK_SIZE=%d\n", VSFS_BLOCK_SIZE);
  printf("PER_DEV_BLOCKS=%d\n", PER_DEV_BLOCKS);

  printf(
      "Superblock: (%ld)\n"
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
      sizeof(struct superblock), sb->sbi.magic, sb->sbi.nr_blocks,
      sb->sbi.nr_ibitmap_blocks, sb->sbi.nr_iregion_blocks,
      sb->sbi.nr_dbitmap_blocks, sb->sbi.nr_dregion_blocks,
      sb->sbi.ofs_ibitmap, sb->sbi.ofs_iregion,
      sb->sbi.ofs_dbitmap, sb->sbi.ofs_dregion,
      sb->sbi.nr_free_inodes, sb->sbi.nr_free_dblock);

  return;
}

void p_root_info() {
  struct vsfs_inode* root_inode = sb->inode_reg;
  // read_spdk(root_inode, sb_cached->sbi.ofs_iregion, 1, IO_QUEUE);
  

  printf(
      "root inode:\n"
      "\tmode: %x\n"
      "\tblocks: %u\n"
      "\tentry: %u\n"
      "\tatime: %lu\n"
      "\tctime: %lu\n"
      "\tmtime: %lu\n",
      root_inode->mode, root_inode->blocks, root_inode->entry,
      root_inode->atime, root_inode->ctime, root_inode->mtime);

  struct vsfs_dir_block* root_dir_info = alloc_dma_buffer(VSFS_BLOCK_SIZE);
  read_spdk(root_dir_info, sb->sbi.ofs_dregion + root_inode->l1[0], 1,
            IO_QUEUE);

  int i;
  printf("/\n");
  for (i = 0; i < root_inode->entry; i++) {
    if (i % 16 == 0) {
      read_spdk(root_dir_info,
                sb->sbi.ofs_dregion + root_inode->l1[i / 16], 1,
                IO_QUEUE);
    }

    printf("\t%s\tinode:%u\n", root_dir_info->files[i % 16].filename,
           root_dir_info->files[i % 16].inode);
  }
  printf("nr_inode/64=%ld\n", VSFS_NR_INODES / 64);

  free_dma_buffer(root_inode);
  return;
}

int cmd_print() {
  char* cmd = (char*)malloc(10 * sizeof(char));
  if(!scanf("%s", cmd)){
    printf("Error: cmd_print!\n");
    return -1;
  }
  if(!strcmp(cmd,"sys")){
    p_sys_info();
  }
  if(!strcmp(cmd,"root")){
    p_root_info();
  }
  return 0;
}

void start_ufs() {
  int ret;
  stat_t* st = (stat_t*)malloc(sizeof(stat_t));

  if (stat_init(st)) {
    printf("Error: init stat faild!\n");
    return;
  }

  if (env_init(st)) {
    printf("Error: init env faild!\n");
    return;
  }

  printf("ufs > using ufs\n");
  printf("ufs > file system info:\n");
  print_ufs_info();

  char* cmd = (char*)malloc(10 * sizeof(char));
  char* input = (char*)malloc(256 * sizeof(char));

  while (1) {
    prompt(st);
    ret = scanf("%s", cmd);
    if (!ret) {
      printf("Error: Read command err.\n");
      break;
    }
    switch (cmd[0]) {
      case 'c':
        if (!strcmp(cmd, "create")) {
          if (cmd_create()) {
            handle_error("cmd_create");
            goto clean_up;
          }
          break;
        }
      case 'h':
        if (!strcmp(cmd, "help") || !strcmp(cmd, "h")) {
          if (cmd_help()) {
            handle_error("cmd_help");
            goto clean_up;
          }
          break;
        }
      case 'l':
        if (!strcmp(cmd, "ls")) {
          if (cmd_ls()) {
            handle_error("cmd_ls");
            goto clean_up;
          }
          break;
        }
      case 'p':
        if (!strcmp(cmd, "pwd")) {
          if (cmd_pwd(st)) {
            handle_error("cmd_pwd");
            goto clean_up;
          }
          break;
        } else if (!strcmp(cmd, "p")) {
          if (cmd_print()) {
            handle_error("cmd_print");
            goto clean_up;
          }
          break;
        }
      case 'q':
        if (!strcmp(cmd, "quit") || !strcmp(cmd, "q")) {
          goto clean_up;
        }
      case 'r':
        if (!strcmp(cmd, "rm")) {
          if (cmd_rm()) {
            handle_error("cmd_rm");
            goto clean_up;
          }
          break;
        } else if (!strcmp(cmd, "read")) {
          if (cmd_read()) {
            handle_error("cmd_read");
            goto clean_up;
          }
          break;
        }
      case 's':
        if (!strcmp(cmd, "stat")) {
          if (cmd_stat()) {
            handle_error("cmd_stat");
            goto clean_up;
          }
          break;
        }
      case 'w':
        if (!strcmp(cmd, "write")) {
          if (cmd_write()) {
            handle_error("cmd_stat");
            goto clean_up;
          }
          break;
        }
      default:
        printf("Error: Unknown command '%s'\n", cmd);
        ret = scanf("%[^\n]", input);
        getchar();
        break;
    }
  }

clean_up:
  stat_clean(st);

  env_exit(st);

  free(input);
  free(cmd);
  free(st);

  return;
}

int main(int argc, char** argv) {
  if (geteuid() != 0) {
    printf("Error: This program must be run with sudo.\n");
    return -1;
  }

  int ret = -1;

  switch (argc) {
    // enter ufs
    case (1):
      start_ufs();
      break;
    // try to do something to ufs
    case (3):
      if (!strcmp(argv[1], "mkfs")) {
        printf("%s %s\n", argv[1], argv[2]);
        ret = call_init_spdk(argv[2]);
        if (ret) {
          printf("ERR: in mkfs_spdk\n");
          return -1;
        }
        break;
      }
      if (!strcmp(argv[1], "mount")) {
        printf("%s %s\n", argv[1], argv[2]);
        ret = call_mount_spdk(argv[2]);
        if (ret) {
          printf("ERR: in mount_spdk\n");
          return -1;
        }
        break;
      }
      if (!strcmp(argv[1], "unmount")) {
        printf("%s %s\n", argv[1], argv[2]);
        ret = call_unmount_spdk(argv[2]);
        if (ret) {
          printf("ERR: in unmount_spdk\n");
          return -1;
        }
        break;
      }
    default:
      printf("bad args\n");
      return -1;
  }

  return 0;
}