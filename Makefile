# Devine OS Kernel Build System

.PHONY: help all build-x86_64 build-arm64 qemu-x86_64 qemu-arm64 qemu-debug-x86_64 qemu-debug-arm64 test-drivers test-drivers-arm64 clean

help:
	@echo "Devine Kernel Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make build-x86_64       Build x86_64 kernel"
	@echo "  make build-arm64        Build ARM64 kernel"
	@echo "  make qemu-x86_64        Run x86_64 kernel on QEMU"
	@echo "  make qemu-arm64         Run ARM64 kernel on QEMU"
	@echo "  make qemu-debug-x86_64  Run x86_64 with GDB support"
	@echo "  make qemu-debug-arm64   Run ARM64 with GDB support"
	@echo "  make test-drivers       Run driver stress tests (x86_64)"
	@echo "  make test-drivers-arm64 Run driver stress tests (ARM64)"
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

qemu-debug-x86_64: build-x86_64
	@echo "Launching x86_64 with GDB support..."
	@GDB=1 GDB_WAIT=1 ./scripts/qemu-x86_64.sh

qemu-debug-arm64: build-arm64
	@echo "Launching ARM64 with GDB support..."
	@GDB=1 GDB_WAIT=1 ./scripts/qemu-arm64.sh

test-drivers: build-x86_64
	@echo "Running driver stress tests..."
	@chmod +x tests/run_driver_stress.sh
	@./tests/run_driver_stress.sh all x86_64

test-drivers-arm64: build-arm64
	@echo "Running driver stress tests (ARM64)..."
	@chmod +x tests/run_driver_stress.sh
	@./tests/run_driver_stress.sh all arm64

qemu-debug-x86_64: build-x86_64
    @echo "Launching x86_64 with GDB support..."
    @GDB=1 GDB_WAIT=1 ./scripts/qemu-x86_64.sh

qemu-debug-arm64: build-arm64
    @echo "Launching ARM64 with GDB support..."
    @GDB=1 GDB_WAIT=1 ./scripts/qemu-arm64.sh
# ext2 filesystem targets
mkfs-ext2:
@echo "Building mkfs.ext2 tool..."
@gcc -o build/mkfs_ext2 examples/fs/mkfs_ext2.c -Wall -Wextra -std=c11
@echo "Creating ext2 image..."
@mkdir -p build
@./build/mkfs_ext2 build/ext2.img 16
@echo "ext2 image created at build/ext2.img"

test-ext2: mkfs-ext2
@echo "Testing ext2 filesystem..."
@echo "  Image info:"
@ls -lh build/ext2.img
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf build/
	@rm -rf target/
	@cargo clean
