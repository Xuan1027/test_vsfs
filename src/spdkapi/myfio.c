#include "config-host.h"
#include "fio.h"
#include "optgroup.h"
#include "../inc/vsfs.h"
#include "../inc/vsfs_bitmap.h"
#include "../inc/vsfs_shmfunc.h"
#include "../inc/vsfs_stdinc.h"
#include "vsfsio_spdk.h"
#include "vsfs_init.h"

#include "spdk.h"
#include <stddef.h>
#include <stdio.h>

static int vsfs_init(struct thread_data *td)
{
  printf("---init---\n");
  init_spdk();
  printf("------------------initialize vsfs-------------------\n");
  init_vsfs("vsfs");
  printf("------------------mounting   vsfs-------------------\n");
  mount_vsfs("vsfs");
  printf("--------------creating file in vsfs-----------------\n");
  vsfs_creat("test", 1024*1024); // 512KB
  printf("-----------------starting fio-----------------------\n");
  return 0;
}

static void vsfs_cleanup(struct thread_data *td)
{
  printf("---cleanup---\n");
  exit_spdk();
}

static int myfio_setup(struct thread_data *td) { 
  printf("---setup---\n");
  return 0;
}

static int
vsfs_fio_open(struct thread_data *td, struct fio_file *f)
{
  printf("---open---\n");
  f->fd = vsfs_open("test", O_RDWR);
  return 0;
}

static int
vsfs_fio_close(struct thread_data *td, struct fio_file *f)
{
  printf("---close---\n");
  vsfs_close(f->fd);
  f->fd = -1;
  return 0;
}

static enum fio_q_status vsfs_queue(struct thread_data *td,
                                    struct io_u *io_u)
{
  switch (io_u->ddir) {
    case DDIR_READ:
      vsfs_lseek(io_u->file->fd, io_u->offset, SEEK_SET);
      vsfs_read(io_u->file->fd, io_u->xfer_buf, io_u->xfer_buflen);
      break;
    case DDIR_WRITE:
      vsfs_lseek(io_u->file->fd, io_u->offset, SEEK_SET);
      vsfs_write(io_u->file->fd, io_u->xfer_buf, io_u->xfer_buflen);
      break;
    default:
      assert(false);
      break;
  }
  return 0;
}

static int vsfs_getevents(struct thread_data *td, unsigned int min,
                          unsigned int max, const struct timespec *t)
{
  return 0;
}

static struct io_u *vsfs_event(struct thread_data *td, int event) { return 0; }

static int vsfs_alloc(struct thread_data *td, size_t size)
{
  printf("---alloc buf---\n");
  td->orig_buffer = alloc_dma_buffer(size);
  return td->orig_buffer == NULL;
}

static void vsfs_free(struct thread_data *td)
{
  printf("---free buf---\n");
  free_dma_buffer(td->orig_buffer);
}

/* FIO imports this structure using dlsym */
struct ioengine_ops ioengine = {
    .name = "myfio",
    .version = FIO_IOOPS_VERSION,
    .setup = myfio_setup,
    .init = vsfs_init,
    .queue = vsfs_queue,
    .getevents = vsfs_getevents,
    .event = vsfs_event,
    .iomem_alloc = vsfs_alloc,
    .iomem_free = vsfs_free,
    .open_file = vsfs_fio_open,
    .close_file = vsfs_fio_close,
    .cleanup = vsfs_cleanup,
};

static void fio_init spdk_fio_register(void) { register_ioengine(&ioengine); }

static void fio_exit spdk_fio_unregister(void)
{
  unregister_ioengine(&ioengine);
}
