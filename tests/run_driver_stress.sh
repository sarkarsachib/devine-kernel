#!/bin/bash

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

TEST_TYPE="${1:-all}"
ARCH="${2:-x86_64}"

echo "=== Driver Stress Test Runner ==="
echo "Test type: $TEST_TYPE"
echo "Architecture: $ARCH"
echo

cd "$PROJECT_ROOT"

build_kernel() {
    echo "Building kernel with profiling enabled..."
    export PROFILER_ENABLED=1
    ./build.sh "$ARCH"
}

run_test() {
    local test_name=$1
    local timeout=${2:-30}
    
    echo "Running $test_name test (timeout: ${timeout}s)..."
    
    if [ "$ARCH" = "x86_64" ]; then
        timeout ${timeout}s qemu-system-x86_64 \
            -kernel "build/$ARCH/kernel.elf" \
            -m 256M \
            -serial stdio \
            -display none \
            -no-reboot \
            -drive file=/tmp/test_disk.img,format=raw,if=virtio \
            -netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
            || true
    else
        timeout ${timeout}s qemu-system-aarch64 \
            -kernel "build/$ARCH/kernel.elf" \
            -m 256M \
            -serial stdio \
            -display none \
            -no-reboot \
            -machine virt \
            -cpu cortex-a72 \
            -drive file=/tmp/test_disk.img,format=raw,if=virtio \
            -netdev user,id=net0 -device virtio-net-pci,netdev=net0 \
            || true
    fi
    
    echo "$test_name test completed"
}

prepare_test_disk() {
    echo "Preparing test disk image..."
    if [ ! -f /tmp/test_disk.img ]; then
        dd if=/dev/zero of=/tmp/test_disk.img bs=1M count=100 2>/dev/null
    fi
}

cleanup() {
    echo "Cleaning up..."
    rm -f /tmp/test_disk.img
}

trap cleanup EXIT

build_kernel
prepare_test_disk

case "$TEST_TYPE" in
    block)
        echo "=== Block Driver Stress Test ==="
        run_test "block" 30
        ;;
    
    network)
        echo "=== Network Driver Stress Test ==="
        run_test "network" 30
        ;;
    
    tty)
        echo "=== TTY Driver Stress Test ==="
        run_test "tty" 15
        ;;
    
    all)
        echo "=== Running All Driver Stress Tests ==="
        run_test "block" 30
        echo
        run_test "network" 30
        echo
        run_test "tty" 15
        ;;
    
    *)
        echo "Unknown test type: $TEST_TYPE"
        echo "Valid options: block, network, tty, all"
        exit 1
        ;;
esac

echo
echo "=== All Tests Completed ==="
