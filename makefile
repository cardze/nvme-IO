.PHONY: clean dir

# SPDK_DIR = /home/qoolili/spdk
# PKG_CONFIG_PATH = $(SPDK_DIR)/build/lib/pkgconfig
# SPDK_LIB := $(shell PKG_CONFIG_PATH="$(PKG_CONFIG_PATH)" pkg-config --libs spdk_bdev spdk_bdev_nvme spdk_env_dpdk)
 

INCLUDE = -Iinclude
SHARED_LIB = -lnvme -lzbd -luring
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