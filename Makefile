CC=gcc
CFLAGS=-g -Wall `pkg-config --cflags fuse3`
LDFLAGS=#-fsanitize=address

mountsfs : src/libsfs/libsfs.o src/mountsfs/main.o src/libbst/libbst.o src/libtable/libtable.o
	$(CC) -o $@ $^ $(LDFLAGS) `pkg-config --libs fuse3`
mkfs.sfs : src/mkfs.sfs/main.o src/libsfs/libsfs.o
	$(CC) -o $@ $^ $(LDFLAGS)
