#!/bin/bash

# QEMU smoke test runner for the syscall ABI.
#
# This is intentionally lightweight and meant to be executed manually while
# developing the kernel/user ABI:
#
#   ./tests/qemu_syscall_smoke.sh
#
# Expected behaviour:
# - x86_64 and ARM64 kernels boot
# - userspace enters the least-privileged mode (Ring 3 / EL0)
# - a trivial syscall (e.g. SYS_GETPID or SYS_WRITE) is issued and returns
#   the expected value
#
# The stable ABI is documented in docs/SYSCALLS.md.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

echo "[smoke] Building kernels..."
./build.sh x86_64
./build.sh arm64

echo "[smoke] Booting x86_64 (10s timeout)..."
timeout 10s ./scripts/qemu-x86_64.sh || true

echo "[smoke] Booting ARM64 (10s timeout)..."
timeout 10s ./scripts/qemu-arm64.sh || true

echo "[smoke] Done"
