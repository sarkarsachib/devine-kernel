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
