SRC_DIR = src
BUILD_DIR = build
CC = gcc
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_NAME = emu
INCLUDE_PATHS = -Iinclude -D_REENTRANT
LIBRARY_PATHS = -Llib
COMPILER_FLAGS = -Wall -O0 -g -rdynamic $(shell pkg-config sdl3 --cflags)
LINKER_FLAGS = $(shell pkg-config sdl3 --libs)

all:
	$(CC) $(COMPILER_FLAGS) $(LINKER_FLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(SRC_FILES) -o $(BUILD_DIR)/$(OBJ_NAME)
