CC=gcc

CFLAGS=-Wall -Wextra -g3
LFLAGS=

OBJS=main.o mm.o mm_test.o
DEPS=mm.h mm_test.h
LIBS=

BIN=OpenSync

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(BIN): $(OBJS)
	$(CC) -o $@ $^ $(LFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(BIN)
