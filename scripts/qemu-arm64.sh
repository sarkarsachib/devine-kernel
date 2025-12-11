#!/bin/bash
# QEMU launcher for ARM64 kernel

set -e

KERNEL_ELF="${1:-build/arm64/kernel.elf}"

if [ ! -f "$KERNEL_ELF" ]; then
    echo "Error: Kernel ELF not found at $KERNEL_ELF"
    echo "Please build the kernel first: ./build.sh arm64"
    exit 1
fi

echo "Launching ARM64 kernel on QEMU..."
echo "Kernel: $KERNEL_ELF"

qemu-system-aarch64 \
    -kernel "$KERNEL_ELF" \
    -m 256M \
    -serial stdio \
    -display none \
    -no-reboot \
    -cpu cortex-a72 \
    "$@"
