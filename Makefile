CC = gcc

INCLUDE_DIR = Include
SOURCE_DIR = Source

INCLUDE := -I $(INCLUDE_DIR)
SOURCES := $(wildcard $(SOURCE_DIR)/*.c)

CFLAGS := -Wall -Wextra -g3 -c
LFLAGS := 

LIBS = -lm -lpthread
ifeq ($(OS),Windows_NT)
	LIBS += -lws2_32
endif

OBJS = client.o configurer.o configurer_test.o crc32.o dirmanager.o filetree.o filetree_test.o main.o mb.o mm.o mm_test.o netwprot.o server.o strings.o strings_test.o syncprot.o transformcontainer.o xsocket.o

TARGET = OpenSync

.PHONY: clean

$(TARGET): $(OBJS)
	$(CC) $(INCLUDE) $(LFLAGS) $^ -o $@ $(LIBS)
	@rm *.o

%.o: $(SOURCE_DIR)/%.c
	$(CC) $(INCLUDE) $(CFLAGS) $^ -o $@ 

clean: 
	rm -f $(TARGET) $(OBJS) 

test:
	./OpenSync
