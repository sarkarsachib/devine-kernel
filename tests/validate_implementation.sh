#!/bin/bash

set -e

echo "==================================="
echo "Driver + Debug Infrastructure Test"
echo "==================================="
echo

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

PASSED=0
FAILED=0

log_test() {
    echo "TEST: $1"
}

log_pass() {
    echo "  ✓ PASS: $1"
    ((PASSED++))
}

log_fail() {
    echo "  ✗ FAIL: $1"
    ((FAILED++))
}

# Test 1: Check if all source files exist
log_test "Source files exist"
if [ -f "src/drivers/pci.c" ]; then log_pass "PCI driver"; else log_fail "PCI driver"; fi
if [ -f "src/drivers/devicetree.c" ]; then log_pass "Device Tree parser"; else log_fail "Device Tree parser"; fi
if [ -f "src/drivers/block/virtio_blk.c" ]; then log_pass "VirtIO block driver"; else log_fail "VirtIO block driver"; fi
if [ -f "src/drivers/net/virtio_net.c" ]; then log_pass "VirtIO network driver"; else log_fail "VirtIO network driver"; fi
if [ -f "src/drivers/tty/uart16550.c" ]; then log_pass "UART 16550 driver"; else log_fail "UART 16550 driver"; fi
if [ -f "src/drivers/tty/pl011.c" ]; then log_pass "PL011 driver"; else log_fail "PL011 driver"; fi
if [ -f "src/kernel/profiler.rs" ]; then log_pass "Profiler module"; else log_fail "Profiler module"; fi
if [ -f "src/kernel/debugger.c" ]; then log_pass "Serial debugger"; else log_fail "Serial debugger"; fi
if [ -f "src/kernel/gdbstub.c" ]; then log_pass "GDB stub"; else log_fail "GDB stub"; fi

# Test 2: Check if profiler infrastructure exists
log_test "Profiler infrastructure"
if [ -f "crates/devine-perf-cpp/include/profiler.hpp" ]; then log_pass "Profiler header"; else log_fail "Profiler header"; fi
if [ -f "crates/devine-perf-cpp/src/profiler.cpp" ]; then log_pass "Profiler implementation"; else log_fail "Profiler implementation"; fi
if [ -f "src/include/profiler.h" ]; then log_pass "Profiler C header"; else log_fail "Profiler C header"; fi

# Test 3: Check if documentation exists
log_test "Documentation"
if [ -f "docs/DEBUGGING.md" ]; then log_pass "Debugging guide"; else log_fail "Debugging guide"; fi
if [ -f "docs/DRIVER_DEVELOPMENT.md" ]; then log_pass "Driver development guide"; else log_fail "Driver development guide"; fi

# Test 4: Check if test infrastructure exists
log_test "Test infrastructure"
if [ -f "tests/driver_stress.rs" ]; then log_pass "Driver stress tests"; else log_fail "Driver stress tests"; fi
if [ -f "tests/run_driver_stress.sh" ] && [ -x "tests/run_driver_stress.sh" ]; then 
    log_pass "Stress test runner"
else 
    log_fail "Stress test runner"
fi

# Test 5: Check if QEMU scripts are updated
log_test "QEMU scripts"
if grep -q "GDB" scripts/qemu-x86_64.sh; then log_pass "x86_64 script supports GDB"; else log_fail "x86_64 script supports GDB"; fi
if grep -q "GDB" scripts/qemu-arm64.sh; then log_pass "ARM64 script supports GDB"; else log_fail "ARM64 script supports GDB"; fi
if grep -q "NETWORK" scripts/qemu-x86_64.sh; then log_pass "x86_64 script supports network"; else log_fail "x86_64 script supports network"; fi
if grep -q "NETWORK" scripts/qemu-arm64.sh; then log_pass "ARM64 script supports network"; else log_fail "ARM64 script supports network"; fi

# Test 6: Check Makefile targets
log_test "Makefile targets"
if grep -q "test-drivers:" Makefile; then log_pass "test-drivers target"; else log_fail "test-drivers target"; fi
if grep -q "qemu-debug-x86_64:" Makefile; then log_pass "qemu-debug-x86_64 target"; else log_fail "qemu-debug-x86_64 target"; fi
if grep -q "qemu-debug-arm64:" Makefile; then log_pass "qemu-debug-arm64 target"; else log_fail "qemu-debug-arm64 target"; fi

# Test 7: Check device.c integration
log_test "Device subsystem integration"
if grep -q "vfs_create_device_node" src/drivers/device.c; then log_pass "VFS device node creation"; else log_fail "VFS device node creation"; fi
if grep -q "virtio_blk_init" src/drivers/device.c; then log_pass "VirtIO block init forward decl"; else log_fail "VirtIO block init forward decl"; fi
if grep -q "uart16550_init" src/drivers/device.c; then log_pass "UART 16550 init forward decl"; else log_fail "UART 16550 init forward decl"; fi

# Test 8: Check kmain.c integration
log_test "Kernel initialization integration"
if grep -q "device_init" src/boot/kmain.c; then log_pass "Device init call"; else log_fail "Device init call"; fi
if grep -q "pci_init" src/boot/kmain.c; then log_pass "PCI init call"; else log_fail "PCI init call"; fi
if grep -q "uart16550_init" src/boot/kmain.c; then log_pass "UART init call"; else log_fail "UART init call"; fi

# Test 9: Check VFS updates
log_test "VFS updates"
if grep -q "get_current_pid" src/vfs/vfs.c; then log_pass "VFS security stubs"; else log_fail "VFS security stubs"; fi

# Test 10: Check utils.c updates
log_test "Utility functions"
if grep -q "system_time" src/lib/utils.c; then log_pass "System time"; else log_fail "System time"; fi
if grep -q "^void\* malloc" src/lib/utils.c; then log_pass "malloc implementation"; else log_fail "malloc implementation"; fi
if grep -q "strncmp" src/lib/utils.c; then log_pass "String functions"; else log_fail "String functions"; fi

# Summary
echo
echo "==================================="
echo "Test Summary"
echo "==================================="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo

if [ $FAILED -eq 0 ]; then
    echo "✓ All tests passed!"
    exit 0
else
    echo "✗ Some tests failed"
    exit 1
fi
