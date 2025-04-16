CC=gcc
CFLAGS=-g -Wall
LDFLAGS=-fsanitize=address

mkfs.sfs : src/mkfs.sfs/main.o src/libsfs/libsfs.o
	$(CC) -o $@ $^ $(LDFLAGS)
