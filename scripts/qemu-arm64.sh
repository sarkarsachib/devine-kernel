#!/bin/bash
# QEMU launcher for ARM64 kernel

set -e

KERNEL_ELF="${1:-build/arm64/kernel.elf}"
shift || true

if [ ! -f "$KERNEL_ELF" ]; then
    echo "Error: Kernel ELF not found at $KERNEL_ELF"
    echo "Please build the kernel first: ./build.sh arm64"
    exit 1
fi

QEMU_ARGS=(
    -kernel "$KERNEL_ELF"
    -m 256M
    -serial stdio
    -display none
    -no-reboot
    -machine virt
    -cpu cortex-a72
)

if [ ! -z "$GDB" ] || [ ! -z "$DEBUG" ]; then
    echo "GDB debugging enabled on port 1234"
    QEMU_ARGS+=(-s)
    if [ ! -z "$GDB_WAIT" ]; then
        echo "Waiting for GDB connection..."
        QEMU_ARGS+=(-S)
    fi
fi

if [ -f "disk.img" ]; then
    echo "Attaching disk image: disk.img"
    QEMU_ARGS+=(-drive file=disk.img,format=raw,if=virtio)
fi

if [ ! -z "$NETWORK" ] || [ ! -z "$NET" ]; then
    echo "Enabling network device"
    QEMU_ARGS+=(
        -netdev user,id=net0
        -device virtio-net-pci,netdev=net0
    )
fi

if [ ! -z "$PROFILER" ]; then
    echo "Profiling enabled"
fi

echo "Launching ARM64 kernel on QEMU..."
echo "Kernel: $KERNEL_ELF"

qemu-system-aarch64 "${QEMU_ARGS[@]}" "$@"
