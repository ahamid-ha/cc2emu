SRC_DIR = src
BUILD_DIR = build
CC = gcc
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_NAME = emu
INCLUDE_PATHS = -Iinclude -I/usr/include/SDL2 -D_REENTRANT
LIBRARY_PATHS = -Llib
COMPILER_FLAGS = -Wall -O0 -g -rdynamic
LINKER_FLAGS = -L/usr/lib -lSDL2 -lSDL2_ttf

all:
	$(CC) $(COMPILER_FLAGS) $(LINKER_FLAGS) $(INCLUDE_PATHS) $(LIBRARY_PATHS) $(SRC_FILES) -o $(BUILD_DIR)/$(OBJ_NAME)
