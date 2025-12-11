#!/bin/bash
# QEMU launcher for x86_64 kernel

set -e

KERNEL_ELF="${1:-build/x86_64/kernel.elf}"
KERNEL_BIN="${KERNEL_ELF%.elf}.bin"

if [ ! -f "$KERNEL_ELF" ]; then
    echo "Error: Kernel ELF not found at $KERNEL_ELF"
    echo "Please build the kernel first: ./build.sh x86_64"
    exit 1
fi

echo "Launching x86_64 kernel on QEMU..."
echo "Kernel: $KERNEL_ELF"

qemu-system-x86_64 \
    -kernel "$KERNEL_ELF" \
    -m 256M \
    -serial stdio \
    -display none \
    -no-reboot \
    "$@"
