CC = gcc

INCLUDE_DIR = ./Include/

INCLUDE := -I $(INCLUDE_DIR)
SOURCE := ./Source/

CFLAGS := -Wall -Wextra -g3 -c
LFLAGS :=

OBJS = client.o configurer.o configurer_test.o crc32.o dirmanager.o filetree.o filetree_test.o main.o mb.o mm.o mm_test.o netwprot.o server.o strings.o strings_test.o syncprot.o transformcontainer.o xsocket.o
DEPS = childthreads.h client.h configurer.h configurer_test.h crc32.h dirmanager.h filetree.h filetree_test.h mb.h mm.h mm_test.h netwprot.h server.h strings.h strings_test.h syncprot.h transformcontainer.h xsocket.h
LIBS = -lm -lpthread
ifeq ($(OS),Windows_NT)
	LIBS += -lws2_32
endif

BIN = OpenSync

#$(INCLUDE_DIR)%.h

%.o: $(SOURCE)%.c 
	$(CC) $(CFLAGS) $(INCLUDE) $^ -o $@

$(BIN): $(OBJS)
	$(CC) -o $@ $^ $(LFLAGS) $(LIBS)

clean:
	rm -f $(OBJS) $(BIN)

test:
	./OpenSync
