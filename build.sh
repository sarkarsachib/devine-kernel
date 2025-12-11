#!/bin/bash

set -e

ARCH=${1:-x86_64}
BUILD_DIR="build/${ARCH}"
TARGET_FILE="target/${ARCH}.json"

mkdir -p "$BUILD_DIR"

case "$ARCH" in
    x86_64)
        echo "Building x86_64 kernel..."
        
        as -32 src/x86_64/boot.s -o "$BUILD_DIR/boot.o"
        
        rustup target add i686-unknown-linux-gnu 2>/dev/null || true
        
        cargo build \
            --target i686-unknown-linux-gnu \
            --release \
            2>&1 | tee "$BUILD_DIR/build.log"
        
        i686-linux-gnu-ld \
            -T src/x86_64/linker.ld \
            -o "$BUILD_DIR/kernel.elf" \
            "$BUILD_DIR/boot.o" \
            target/i686-unknown-linux-gnu/release/libkernel.a
        
        echo "x86_64 kernel built: $BUILD_DIR/kernel.elf"
        ;;
        
    arm64)
        echo "Building ARM64 kernel..."
        
        aarch64-linux-gnu-as src/arm64/boot.s -o "$BUILD_DIR/boot.o"
        
        rustup target add aarch64-unknown-linux-gnu 2>/dev/null || true
        
        cargo build \
            --target aarch64-unknown-linux-gnu \
            --release \
            2>&1 | tee "$BUILD_DIR/build.log"
        
        aarch64-linux-gnu-ld \
            -T src/arm64/linker.ld \
            -o "$BUILD_DIR/kernel.elf" \
            "$BUILD_DIR/boot.o" \
            target/aarch64-unknown-linux-gnu/release/libkernel.a
        
        echo "ARM64 kernel built: $BUILD_DIR/kernel.elf"
        ;;
        
    *)
        echo "Unknown architecture: $ARCH"
        echo "Supported: x86_64, arm64"
        exit 1
        ;;
esac
