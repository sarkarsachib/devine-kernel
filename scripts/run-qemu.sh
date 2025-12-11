#!/bin/bash
# QEMU script to run the kernel

KERNEL_PATH="${KERNEL_PATH:-build/kernel.bin}"

echo "Starting OS Kernel in QEMU..."
echo "Kernel: $KERNEL_PATH"

# Check if kernel exists
if [ ! -f "$KERNEL_PATH" ]; then
    echo "Error: Kernel binary not found at $KERNEL_PATH"
    echo "Run 'make' to build the kernel first"
    exit 1
fi

# Run QEMU with appropriate options for a 64-bit OS kernel
qemu-system-x86_64 \
    -kernel "$KERNEL_PATH" \
    -m 512M \
    -smp 2 \
    -serial stdio \
    -display curses \
    -no-reboot \
    -no-shutdown \
    -d guest_errors \
    -D qemu.log

echo "QEMU session ended"