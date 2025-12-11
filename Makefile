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
.PHONY: all clean build-x86_64 build-aarch64 run-x86_64 run-aarch64 check help

# Default target
all: help

# Help target
help:
	@echo "Devine Kernel Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  build-x86_64      - Build kernel for x86_64"
	@echo "  build-aarch64     - Build kernel for aarch64"
	@echo "  bootimage-x86_64  - Create bootable x86_64 image"
	@echo "  run-x86_64        - Build and run x86_64 in QEMU"
	@echo "  run-aarch64       - Build and run aarch64 in QEMU"
	@echo "  check             - Check all workspace crates"
	@echo "  clean             - Clean build artifacts"
	@echo "  help              - Show this help message"

# Build targets
build-x86_64:
	cargo build --release --target x86_64-devine.json

build-aarch64:
	cargo build --release --target aarch64-devine.json

bootimage-x86_64:
	cargo bootimage --release --target x86_64-devine.json

# Run targets
run-x86_64: bootimage-x86_64
	qemu-system-x86_64 \
		-drive format=raw,file=target/x86_64-devine/release/bootimage-devine-kernel-core.bin \
		-serial stdio \
		-display none

run-aarch64: build-aarch64
	cargo objcopy --release --target aarch64-devine.json -- -O binary target/aarch64-devine/release/devine-kernel-core.bin
	qemu-system-aarch64 \
		-machine virt \
		-cpu cortex-a72 \
		-kernel target/aarch64-devine/release/devine-kernel-core \
		-serial stdio \
		-display none

# Development targets
check:
	cargo check --workspace

clean:
	cargo clean

# Install prerequisites
install-tools:
	@echo "Installing Rust toolchain components..."
	rustup component add rust-src llvm-tools-preview
	@echo "Installing cargo tools..."
	cargo install bootimage cargo-binutils
	@echo "Done! Make sure to install QEMU and cross-compilation toolchains manually."
	@echo "See README.md for platform-specific installation instructions."
.PHONY: all build-x86_64 build-arm64 qemu-x86_64 qemu-arm64 clean help

help:
	@echo "Available targets:"
	@echo "  make build-x86_64       Build x86_64 kernel"
	@echo "  make build-arm64        Build ARM64 kernel"
	@echo "  make qemu-x86_64        Run x86_64 kernel on QEMU"
	@echo "  make qemu-arm64         Run ARM64 kernel on QEMU"
	@echo "  make clean              Clean build artifacts"
	@echo "  make all                Build both architectures"

all: build-x86_64 build-arm64

build-x86_64:
	@echo "Building x86_64 kernel..."
	@./build.sh x86_64

build-arm64:
	@echo "Building ARM64 kernel..."
	@./build.sh arm64

qemu-x86_64: build-x86_64
	@chmod +x scripts/qemu-x86_64.sh
	@./scripts/qemu-x86_64.sh

qemu-arm64: build-arm64
	@chmod +x scripts/qemu-arm64.sh
	@./scripts/qemu-arm64.sh

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build/
	@rm -rf target/
	@cargo clean

test:
	@echo "Running tests..."
	@cargo test --target x86_64-unknown-linux-gnu
