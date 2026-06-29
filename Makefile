# Plain C demo at repo root: src/*.c + include/*.h, single main in src/main.c
CC      := gcc
CFLAGS  := -Wall -Wextra -std=c11 -g -Iinclude
SRCS    := $(filter-out src/main.c,$(wildcard src/*.c))
OBJS    := $(SRCS:.c=.o)
TARGET  := build/app

.PHONY: all run clean

all: $(TARGET)

$(TARGET): src/main.c $(OBJS) | build
	$(CC) $(CFLAGS) src/main.c $(OBJS) -o $@

build:
	mkdir -p build

src/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build src/*.o
