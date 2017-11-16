CC=gcc

CFLAGS=-Wall -Wextra -g3
LFLAGS=

OBJS=crc32.o filetree.o filetree_test.o main.o mb.o mm.o mm_test.o strings.o strings_test.o transformcontainer.o
DEPS=crc32.h filetree.h filetree_test.h mb.h mm.h mm_test.h strings.h strings_test.h transformcontainer.h
LIBS=

BIN=OpenSync

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJS)
	$(CC) -o $@ $^ $(LFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(BIN)

test:
	./OpenSync
