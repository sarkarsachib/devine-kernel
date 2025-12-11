ARCH ?= x86_64
BUILD_DIR := build/$(ARCH)
CC ?= gcc
AR ?= ar
INCLUDES := -Iinclude
CFLAGS := -ffreestanding -fno-stack-protector -fno-builtin -nostdlib -nostartfiles -Wall -Wextra -Os $(INCLUDES)

ifeq ($(ARCH),x86_64)
CRT0 := src/crt0_x86_64.S
CFLAGS += -m64
else ifeq ($(ARCH),arm64)
CRT0 := src/crt0_arm64.S
CC ?= aarch64-linux-gnu-gcc
AR ?= aarch64-linux-gnu-ar
else
$(error Unsupported ARCH $(ARCH))
endif

.PHONY: all lib clean

all: lib

lib: $(BUILD_DIR)/libkuser.a $(BUILD_DIR)/crt0.o

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/crt0.o: $(CRT0) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libkuser.a: src/libc.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $(BUILD_DIR)/libc.o
	$(AR) rcs $@ $(BUILD_DIR)/libc.o

clean:
	rm -rf build
