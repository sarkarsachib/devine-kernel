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
