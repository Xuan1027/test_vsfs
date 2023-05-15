Q := @
CC := gcc

VSFS_ROOT := $(abspath $(CURDIR)/../..)
VSFS_SRCDIR := $(VSFS_ROOT)/src
# VSFS_OBJDIR := $(VSFS_ROOT)/obj
# VSFS_EXEDIR := $(VSFS_ROOT)/build
VSFS_INCDIR := $(VSFS_SRCDIR)/inc
# VSFS_SHMDIR := $(VSFS_SRCDIR)/shm
# VSFS_TESDIR := $(VSFS_SRCDIR)/test
VSFS_UFS := $(VSFS_SRCDIR)/ufs
VSFS_SPDK := $(VSFS_SRCDIR)/spdkapi
SPDK_API := $(VSFS_ROOT)/spdk/build/fio/spdkapi

CFLAGS += -Wall -O2 -march=native -finline-functions -g
LDFLAGS = -I$(VSFS_INCDIR)
SYS_LIBS = -lrt

C_SRCS := $(wildcard $(VSFS_UFS)/*.c)
# EXEC = $(patsubst %.c,$(VSFS_EXEDIR)/%,$(notdir $(C_SRCS)))

MY_CFLAGS := -D_NVME_ADDR_=\"0000:0b:00.0\" -D_MY_SSD_BLOCK_SIZE_=512

.PHONY: clean

all: ufs test

clean :
	$(Q)rm -f /dev/shm/optab /dev/shm/vsfs_cached;\
	$(CLEAN_C)

ufs: ufs_sh.o vsfs_init.o
	$(Q)echo "  LINK $(notdir $@)";\
	$(CC) $^ $(SPDK_API) $(SYS_LIBS) -o $@

test: test.o
	$(Q)echo "  LINK $(notdir $@)";\
	$(CC) $^ $(SPDK_API) $(SYS_LIBS) -o $@

%.o : $(VSFS_UFS)/%.c $(VSFS_INCDIR)/*.h
	$(Q)echo "  CC $(notdir $@)";\
	$(CC) $(MY_CFLAGS) $(LDFLAGS) -c $< $(CFLAGS) -o $@

CLEAN_C=\
	rm -f $(VSFS_OBJDIR)/*.o;\
	rm -f $(VSFS_EXEDIR)/*

