.POSIX:
.PHONY: all clean

TARGET = main
CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -O2 -I../include

SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) *.o *.img