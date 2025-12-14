# Devine Kernel

A modern, cross-architecture kernel written in Rust. Supports x86_64 and ARM64/aarch64 architectures.

## Features

- **Multi-architecture support**: x86_64 and aarch64 targets
- **SMP Support**: Multi-core scheduling and execution
- **Userspace**: Basic shell and utilities (`ls`, `cat`, `stress`)
- **Memory Management**: Paging, Heap, NUMA-aware allocation
- **Process Management**: Processes, Threads, Scheduler

## Documentation

- [Shell Reference](docs/SHELL.md)
- [Userspace Guide](docs/USERSPACE.md)
- [Syscall ABI](docs/SYSCALLS.md)

## Building

```bash
./build.sh x86_64
# or
./build.sh arm64
```

## Userspace

The userspace environment includes a `libc`, `init`, and `shell`.
To build userspace:
```bash
./userspace/build.sh
```

## Running

```bash
make qemu-x86_64
```

## Project Structure

- `src/`: Kernel source code
- `userspace/`: Userspace applications and libc
- `tests/`: Integration tests
- `docs/`: Documentation
