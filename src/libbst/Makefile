CC=gcc
CFLAGS=-g
LDFLAGS=-fsanitize=address

test : libbst.o test.o
	$(CC) $^ -o $@ $(LDFLAGS)
