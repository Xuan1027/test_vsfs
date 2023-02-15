CC := gcc
CFLAGS += -Wall -lpthread -O2 -march=native -finline-functions
LDFLAGS = 

EXEC = shm_mkfs shm_mount testfs
EXT = .c
SRCDIR = src
OBJDIR = obj

SRC = $(wildcard $(SRCDIR)/*$(EXT))
OBJ = $(SRC:$(SRCDIR)/%$(EXT)=$(OBJDIR)/%.o)
DEP = $(OBJ:$(OBJDIR)/%.o=%.d)

all: $(EXEC)

shm_mkfs: src/shm_mkfs.c
	$(CC) $(CFLAGS) $< -o compile/$@

shm_mount: src/shm_mount.c
	$(CC) $(CFLAGS) $< -o compile/$@

testfs: src/testfs.c
	$(CC) $(CFLAGS) $< -o compile/$@
