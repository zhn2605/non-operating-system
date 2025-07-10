.POSIX:
.PHONY: all clean

TARGET = write_gpt
CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -O2

SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)