#  SPDX-License-Identifier: BSD-3-Clause
#  Copyright (C) 2016 Intel Corporation.
#  Copyright (c) 2015-2016, Micron Technology, Inc.
#  All rights reserved.
#

SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../spdk)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

# share library
FIO_PLUGIN := myfio

C_SRCS = spdk.c myfio.c

SPDK_LIB_LIST = $(SOCK_MODULES_LIST) nvme vmd

include $(SPDK_ROOT_DIR)/mk/spdk.fio.mk

example:
	$(CC) example.c $(SPDK_ROOT_DIR)/build/fio/spdkapi -o example
test:
	$(CC) -c vsfs_init.c -o vsfs_init.o
	$(CC) -g test_vsfs_init.c vsfs_init.o $(SPDK_ROOT_DIR)/build/fio/spdkapi -o test_vsfs_init
	$(CC) -g testcreat.c vsfs_init.o $(SPDK_ROOT_DIR)/build/fio/spdkapi -o testcreat
	$(CC) -g testfs.c $(SPDK_ROOT_DIR)/build/fio/spdkapi -o testfs