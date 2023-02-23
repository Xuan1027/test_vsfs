Q ?= @
CC := gcc

VSFS_ROOT := $(CURDIR)
VSFS_SRCDIR := $(VSFS_ROOT)/src
VSFS_OBJDIR := $(VSFS_ROOT)/obj
VSFS_INCDIR := $(VSFS_ROOT)/src/inc
VSFS_EXEDIR := $(VSFS_ROOT)/compile

CFLAGS += -Wall -O2 -march=native -finline-functions -g
LDFLAGS = -I$(VSFS_INCDIR)
SYS_LIBS = -lrt -lpthread

tar = shm_mkfs shm_mount shm_unlink testfs testinode testcached testget
EXEC = $(tar:%=$(VSFS_EXEDIR)/%)
EXT = .c

SRCS = $(wildcard $(VSFS_SRCDIR)/*$(EXT))
OBJS = $(SRC:$(VSFS_SRCDIR)/%$(EXT)=$(VSFS_OBJDIR)/%.o)
DEPS = $(OBJ:$(VSFS_OBJDIR)/%.o=%.d)

.PHONY: all clean setup
.PRECIOUS: $(VSFS_OBJDIR)/%.o

all : $(EXEC)

clean :
	$(Q)$(VSFS_EXEDIR)/shm_unlink test;\
	$(CLEAN_C)

setup :
	$(VSFS_EXEDIR)/shm_mkfs test
	$(VSFS_EXEDIR)/shm_mount test
	$(VSFS_EXEDIR)/testinode test

$(VSFS_OBJDIR)/%.o : $(VSFS_SRCDIR)/%.c $(VSFS_INCDIR)/*.h
	$(Q)echo "  CC $(notdir $@)";\
	$(CC) $(LDFLAGS) -c $< $(CFLAGS) -o $@

$(VSFS_EXEDIR)/% : $(VSFS_OBJDIR)/%.o
	$(Q)echo "  LINK $(notdir $@)";\
	$(CC) $< $(SYS_LIBS) -o $@

CLEAN_C=\
	rm -f $(VSFS_OBJDIR)/*.o;\
	rm -f $(VSFS_EXEDIR)/*

