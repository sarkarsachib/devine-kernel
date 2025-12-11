#!/bin/bash

set -e

ARCH=${1:-x86_64}
BUILD_DIR="build/${ARCH}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

mkdir -p "$BUILD_DIR"

cd "$PROJECT_ROOT"

case "$ARCH" in
    x86_64)
        echo "Building x86_64 kernel..."
        
        # Add rust target support
        rustup target add x86_64-unknown-linux-gnu 2>/dev/null || true
        
        # Build using custom target spec
        cargo build \
            --target x86_64-devine.json \
            --release \
            2>&1 | tee "$BUILD_DIR/build.log"
        
        # The kernel output is the result of the build process
        # Copy it to the build directory for easier access
        if [ -f target/x86_64-devine/release/kernel ]; then
            cp target/x86_64-devine/release/kernel "$BUILD_DIR/kernel.elf"
        fi
        
        if [ -f "$BUILD_DIR/kernel.elf" ]; then
            echo "x86_64 kernel built: $BUILD_DIR/kernel.elf"
        else
            echo "ERROR: Kernel binary not found at expected location"
            echo "Looking in target directory..."
            find target -name "kernel" -type f 2>/dev/null | head -5
            exit 1
        fi
        ;;
        
    arm64)
        echo "Building ARM64 kernel..."
        
        # Add rust target support
        rustup target add aarch64-unknown-linux-gnu 2>/dev/null || true
        
        # Build using custom target spec
        cargo build \
            --target aarch64-devine.json \
            --release \
            2>&1 | tee "$BUILD_DIR/build.log"
        
        # The kernel output is the result of the build process
        # Copy it to the build directory for easier access
        if [ -f target/aarch64-devine/release/kernel ]; then
            cp target/aarch64-devine/release/kernel "$BUILD_DIR/kernel.elf"
        fi
        
        if [ -f "$BUILD_DIR/kernel.elf" ]; then
            echo "ARM64 kernel built: $BUILD_DIR/kernel.elf"
        else
            echo "ERROR: Kernel binary not found at expected location"
            echo "Looking in target directory..."
            find target -name "kernel" -type f 2>/dev/null | head -5
            exit 1
        fi
        ;;
        
    *)
        echo "Unknown architecture: $ARCH"
        echo "Supported: x86_64, arm64"
        exit 1
        ;;
esac
