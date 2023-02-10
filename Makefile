Q ?= @
VSFS_ROOT := $(CURDIR)

CFLAGS = -I$(VSFS_ROOT)/src
SYS_LIBS = -lrt

exec = shm_mkfs shm_mount testfs

des := $(exec:%=$(VSFS_ROOT)/compile/%)


.PHONY: all clean

all : $(des)
	@:

clean :
	$(Q)rm -f compile/*

%.o : $(VSFS_ROOT)/src/%.c
	$(Q)echo "  CC $@";\
	gcc -o $@ -c $< $(CFLAGS)

$(VSFS_ROOT)/compile/% : %.o
	$(Q)echo "  LINK $(notdir $@)";\
	gcc -o $@ $< $(SYS_LIBS)
	$(CLEAN_C)

CLEAN_C=\
	$(Q)rm -f *.o
