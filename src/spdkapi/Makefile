#  SPDX-License-Identifier: BSD-3-Clause
#  Copyright (C) 2016 Intel Corporation.
#  Copyright (c) 2015-2016, Micron Technology, Inc.
#  All rights reserved.
#

SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../spdk)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

# share library
# FIO_PLUGIN := myfio

# C_SRCS = spdk.c myfio.c vsfs_init.c

FIO_PLUGIN := spdkapi

C_SRCS = spdk.c

###############
#  attention  #
###############
# modify the NVME_ADDR and MY_SSD_BLOCK_SIZE here
MY_CFLAGS := -D_NVME_ADDR_=\"0000:0b:00.0\" -D_MY_SSD_BLOCK_SIZE_=512

CFLAGS += $(MY_CFLAGS)

SPDK_LIB_LIST = $(SOCK_MODULES_LIST) nvme vmd

include $(SPDK_ROOT_DIR)/mk/spdk.fio.mk

example: example.c $(SPDK_ROOT_DIR)/build/fio/spdkapi vsfsio_spdk.h
	$(CC) example.c $(SPDK_ROOT_DIR)/build/fio/spdkapi -o example
test: vsfs_init.c testcreat.c testwrite_2.c vsfsio_spdk.h $(SPDK_ROOT_DIR)/build/fio/spdkapi
	$(CC) $(MY_CFLAGS) -c vsfs_init.c -o vsfs_init.o
	$(CC) $(MY_CFLAGS) -g test_vsfs_init.c vsfs_init.o $(SPDK_ROOT_DIR)/build/fio/spdkapi -o test_vsfs_init -lrt
	$(CC) $(MY_CFLAGS) -g testcreat.c vsfs_init.o $(SPDK_ROOT_DIR)/build/fio/spdkapi -o testcreat -lrt
	$(CC) $(MY_CFLAGS) -g testfs.c $(SPDK_ROOT_DIR)/build/fio/spdkapi -o testfs
	$(CC) $(MY_CFLAGS) -g testwrite_2.c vsfs_init.o $(SPDK_ROOT_DIR)/build/fio/spdkapi -o testwrite_2 -lrt
	$(CC) $(MY_CFLAGS) -g testwrite.c vsfs_init.o $(SPDK_ROOT_DIR)/build/fio/spdkapi -o testwrite -lrt

