ARCH ?= x86_64
BUILD_DIR := build/$(ARCH)
LIBC_DIR ?= ../libc/build/$(ARCH)
CC ?= gcc
PROGRAMS := init shell cat ls stress
INCLUDES := -I../libc/include
CFLAGS := -ffreestanding -fno-stack-protector -nostdlib -nostartfiles -Wall -Wextra -Os $(INCLUDES)
LDFLAGS :=
CRT0 := $(LIBC_DIR)/crt0.o
LIBKUSER := $(LIBC_DIR)/libkuser.a

ifeq ($(ARCH),x86_64)
CFLAGS += -m64
else ifeq ($(ARCH),arm64)
CC ?= aarch64-linux-gnu-gcc
endif

.PHONY: all clean

all: $(addprefix $(BUILD_DIR)/,$(addsuffix .elf,$(PROGRAMS)))

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/%.elf: src/%.c $(CRT0) $(LIBKUSER) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< $(CRT0) $(LIBKUSER) -o $@

clean:
	rm -rf build
