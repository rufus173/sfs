CC=gcc
CFLAGS=-g -Wall
LDFLAGS=-fsanitize=address

test : test.o libtable.o
	$(CC) $^ -o $@ $(LDFLAGS)
