CC=gcc
CFLAGS=-g -Wall `pkg-config --cflags fuse3`
LDFLAGS=-fsanitize=address

mountfs : src/libsfs/libsfs.o src/mountsfs/main.o
	$(CC) -o $@ $^ $(LDFLAGS) `pkg-config --libs fuse3`
mkfs.sfs : src/mkfs.sfs/main.o src/libsfs/libsfs.o
	$(CC) -o $@ $^ $(LDFLAGS)
