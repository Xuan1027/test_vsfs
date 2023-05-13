#ifndef UFS_H
#define UFS_H

#include "spdk.h"
#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_func.h"
#include "vsfs_init.h"
#include "vsfs_stdinc.h"
#include "vsfsio_spdk.h"

typedef struct status{
  char* home;
  char* cwd;
}stat_t;

#endif