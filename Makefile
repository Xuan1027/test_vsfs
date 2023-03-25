Q ?= @
CC := gcc

VSFS_ROOT := $(CURDIR)
VSFS_SRCDIR := $(VSFS_ROOT)/src
VSFS_OBJDIR := $(VSFS_ROOT)/obj
VSFS_EXEDIR := $(VSFS_ROOT)/compile
VSFS_INCDIR := $(VSFS_SRCDIR)/inc
VSFS_SHMDIR := $(VSFS_SRCDIR)/shm
VSFS_TESDIR := $(VSFS_SRCDIR)/test

CFLAGS += -Wall -O2 -march=native -finline-functions -g
LDFLAGS = -I$(VSFS_INCDIR)
SYS_LIBS = -lrt -lpthread

tar := $(patsubst %.c,%,$(notdir $(wildcard $(VSFS_SRCDIR)/*/*.c)))
EXEC = $(tar:%=$(VSFS_EXEDIR)/%)

# EXT = .c
# SRCS = $(wildcard $(VSFS_SRCDIR)/*$(EXT))
# OBJS = $(SRC:$(VSFS_SRCDIR)/%$(EXT)=$(VSFS_OBJDIR)/%.o)
# DEPS = $(OBJ:$(VSFS_OBJDIR)/%.o=%.d)

.PHONY: all clean setup
.PRECIOUS: $(VSFS_OBJDIR)/%.o

all : $(EXEC)

clean :
	-$(Q)$(VSFS_EXEDIR)/shm_unlink test;\
	$(CLEAN_C)

setup :
	$(VSFS_EXEDIR)/shm_mkfs vsfs
	$(VSFS_EXEDIR)/shm_mount vsfs
	$(VSFS_EXEDIR)/testcreat vsfs

$(VSFS_OBJDIR)/%.o : $(VSFS_SHMDIR)/%.c $(VSFS_INCDIR)/*.h
	$(Q)echo "  CC $(notdir $@)";\
	mkdir -p obj;\
	$(CC) $(LDFLAGS) -c $< $(CFLAGS) -o $@

$(VSFS_OBJDIR)/%.o : $(VSFS_TESDIR)/%.c $(VSFS_INCDIR)/*.h
	$(Q)echo "  CC $(notdir $@)";\
	mkdir -p obj;\
	$(CC) $(LDFLAGS) -c $< $(CFLAGS) -o $@

$(VSFS_EXEDIR)/% : $(VSFS_OBJDIR)/%.o
	$(Q)echo "  LINK $(notdir $@)";\
	mkdir -p compile;\
	$(CC) $< $(SYS_LIBS) -o $@

CLEAN_C=\
	rm -f $(VSFS_OBJDIR)/*.o;\
	rm -f $(VSFS_EXEDIR)/*

