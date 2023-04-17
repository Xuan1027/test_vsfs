#  SPDX-License-Identifier: BSD-3-Clause
#  Copyright (C) 2017 Intel Corporation.
#  Copyright (c) 2015-2016, Micron Technology, Inc.
#  All rights reserved.
#

VSFS_ROOT := $(CURDIR)/../..
VSFS_SRCDIR := $(VSFS_ROOT)/src
VSFS_OBJDIR := $(VSFS_ROOT)/obj
VSFS_EXEDIR := $(VSFS_ROOT)/build
VSFS_INCDIR := $(VSFS_SRCDIR)/inc
VSFS_SHMDIR := $(VSFS_SRCDIR)/shm
VSFS_TESDIR := $(VSFS_SRCDIR)/test

SPDK_ROOT_DIR := $(abspath $(CURDIR)/../../spdk)
include $(SPDK_ROOT_DIR)/mk/spdk.common.mk
include $(SPDK_ROOT_DIR)/mk/spdk.modules.mk

FIO_PLUGIN := myfio

C_SRCS = myfio.c

CFLAGS += -I$(VSFS_INCDIR)

SPDK_LIB_LIST = $(ALL_MODULES_LIST) event event_bdev

include $(SPDK_ROOT_DIR)/mk/spdk.fio.mk
