.POSIX:
.PHONY: all clean

TARGET = main
CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -O2 -Iinclude

SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:src/%.c=src/%.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) src/*.o src/*.img