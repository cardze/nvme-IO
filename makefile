.PHONY: clean dir

INCLUDE = -Iinclude
SHARED_LIB = -lnvme -lzbd
CC = gcc
CFLAGS = -Wfatal-errors -Wall -g $(INCLUDE)

OBJ = src/*.c

all: clean main

	
run:
	./bin/main

main: main.c dir $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) main.c -o bin/main $(SHARED_LIB) -D_GNU_SOURCE

dir:
	mkdir -p bin

clean:
	rm -rf bin