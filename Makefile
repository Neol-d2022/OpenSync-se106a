CC = gcc

INCLUDE_DIR = Include
SOURCE_DIR = Source
RELEASE_DIR = Release

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
	@cd Source
	$(CC) $(INCLUDE) $(LFLAGS) $^ -o $(RELEASE_DIR)/$@ $(LIBS)

%.o: $(SOURCE_DIR)/%.c
	$(CC) $(INCLUDE) $(CFLAGS) $^ -o $(SOURCE_DIR)/$@ 

clean: 
	rm -f $(RELEASE_DIR)/$(TARGET) $(OBJS) 

test:
	./OpenSync
