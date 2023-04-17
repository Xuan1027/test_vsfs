#include "config-host.h"
#include "fio.h"
#include "optgroup.h"
#include "vsfs.h"
#include "vsfs_bitmap.h"
#include "vsfs_init.c"
#include "vsfs_shmfunc.h"
#include "vsfs_stdinc.h"
#include "vsfsio.h"

static int vsfs_init(struct thread_data *td) {
  return 0;
}

static void vsfs_cleanup(struct thread_data *td) {}

static int
vsfs_fio_open(struct thread_data *td, struct fio_file *f)
{
  f->fd = vsfs_open(f->file_name, O_RDWR);
	return 0;
}

static int
vsfs_fio_close(struct thread_data *td, struct fio_file *f)
{
  vsfs_close(f->fd);
  f->fd = -1;
	return 0;
}

static enum fio_q_status vsfs_queue(struct thread_data *td,
                                     struct io_u *io_u) {
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

static int myfio_setup(struct thread_data *td) { 
  printf("------------------initialize vsfs-------------------\n");
  init_vsfs("vsfs");
  printf("------------------mounting   vsfs-------------------\n");
  mount_vsfs("vsfs");
  printf("--------------creating file in vsfs-----------------\n");
  vsfs_creat("test", 1048576);
  printf("-----------------starting fio-----------------------\n");

  return 0;
}

static int vsfs_getevents(struct thread_data *td, unsigned int min,
                           unsigned int max, const struct timespec *t) {
  return 0;
}

static struct io_u *vsfs_event(struct thread_data *td, int event) { return 0; }

/* FIO imports this structure using dlsym */
struct ioengine_ops ioengine = {.name = "myfio",
                                .version = FIO_IOOPS_VERSION,
                                .setup = myfio_setup,
                                .init = vsfs_init,
                                .queue = vsfs_queue,
                                .getevents = vsfs_getevents,
                                .event = vsfs_event,
                                .open_file = vsfs_fio_open,
                                .close_file = vsfs_fio_close,
                                .cleanup = vsfs_cleanup};

static void fio_init spdk_fio_register(void) { register_ioengine(&ioengine); }

static void fio_exit spdk_fio_unregister(void) {
  unregister_ioengine(&ioengine);
}