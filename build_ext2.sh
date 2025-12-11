#!/bin/bash

# Build ext2 filesystem components

set -e

echo "=== Building ext2 Filesystem Components ==="

# Build mkfs.ext2 tool
echo "Building mkfs.ext2 tool..."
mkdir -p build
gcc -o build/mkfs_ext2 examples/fs/mkfs_ext2.c -Wall -Wextra -std=c11

# Create ext2 image
echo "Creating ext2 image..."
./build/mkfs_ext2 build/ext2.img 16

echo "=== ext2 Build Complete ==="
echo "  mkfs tool: build/mkfs_ext2"
echo "  ext2 image: build/ext2.img"
ls -lh build/ext2.img
