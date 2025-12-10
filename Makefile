# OS Kernel Build System - Working Version
CC = gcc
LD = ld
OBJCOPY = objcopy

# Compiler flags for kernel development
CFLAGS = -m64 -ffreestanding -fno-builtin -fno-strict-aliasing \
         -mcmodel=large -I./src/include -Wall -Wextra \
         -std=c11 -O2 -g

# Linker flags
LDFLAGS = -nostdlib -z max-page-size=0x1000

# Source files - minimal working set
KERNEL_SRCS = src/boot/kmain.c \
              src/lib/console.c \
              src/lib/utils.c \
              src/arch/x86_64/gdt.c \
              src/arch/x86_64/idt.c \
              src/arch/x86_64/apic.c

KERNEL_OBJS = $(KERNEL_SRCS:.c=.o)
KERNEL_ELF = build/kernel.elf
KERNEL_BIN = build/kernel.bin

.PHONY: all clean run test

all: $(KERNEL_BIN)

$(KERNEL_ELF): $(KERNEL_OBJS)
	@echo "Linking kernel..."
	$(LD) $(LDFLAGS) -T src/arch/x86_64/linker.ld -o $@ $^

$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "Creating binary..."
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/*.bin build/*.elf build/*.o src/**/*.o

run: $(KERNEL_BIN)
	@echo "Running kernel in QEMU..."
	timeout 10s qemu-system-x86_64 -kernel $(KERNEL_BIN) -nographic -serial stdio -no-reboot

test: $(KERNEL_BIN)
	@echo "Testing kernel..."
	@echo "Kernel binary created: $(KERNEL_BIN)"
	@ls -la $(KERNEL_BIN)