# Userspace Support

This repository ships a tiny userspace toolchain that is built entirely from the `userspace/` tree. The goal of these utilities is not to be feature complete; they are designed to provide deterministic fixtures for the kernel ELF loader, syscalls, and integration tests.

## Layout

```
userspace/
├── apps/          # Small C programs (init, shell, cat, ls, stress)
├── libc/          # Minimal syscall wrapper and crt0
├── prebuilt/      # Architecture specific ELF binaries used by the kernel
├── rootfs/        # Staging area used when producing disk images
└── build.sh       # Convenience wrapper that builds everything end-to-end
```

## Building the C runtime and programs

1. Build the libc and crt objects:

   ```bash
   ./userspace/build.sh x86_64
   ```

   The script automatically invokes `make` inside `userspace/libc` and `userspace/apps`, copies the resulting binaries into `userspace/prebuilt/<arch>/`, and assembles a simple rootfs hierarchy under `userspace/rootfs/<arch>/`.

2. Optional: create an Ext2 image. If `genext2fs` is available it is used automatically. Otherwise a tarball is produced under `userspace/build/<arch>/rootfs.tar` which can be loop-mounted or unpacked into an existing disk image.

3. The kernel embeds the prebuilt binaries via `include_bytes!`. These files must exist before running `cargo build`. Use `userspace/build.sh` whenever the userland sources change.

## Adding new programs

1. Drop a new C file under `userspace/apps/src/` and add the binary name to the `PROGRAMS` variable inside `userspace/apps/Makefile`.
2. Rebuild via `./userspace/build.sh <arch>`.
3. Register the new binary path inside `src/userspace/mod.rs` so that `sys_exec` can resolve it via the builtin registry.

## libc / syscall ABI

The minimal libc exposes the following syscalls:

| Symbol | Syscall Number | Notes |
|--------|----------------|-------|
| `write` | 9 | Writes into the kernel console buffer |
| `read`  | 10 | Reads from the synthetic stdin queue used by tests |
| `fork`  | 1 | Copies the current process |
| `exec`  | 2 | Loads a new ELF via the in-kernel loader |
| `wait`  | 3 | Blocks on child exit |
| `exit`  | 0 | Terminates the current thread |
| `getpid` | 4 | Returns the numeric PID |

`crt0` installs `_start` for both x86_64 and AArch64 and jumps into `kuser_entry`, which in turn calls `main(argc, argv)` and exits with its return value.

## Integration expectations

* `/sbin/init` is started automatically once the kernel enables user mode. It spawns `/bin/sh` and waits for it to complete.
* The shell reads from STDIN, writes prompts to STDOUT, and can optionally attempt to execute the stress utility.
* `cat`, `ls`, and `stress` are stub programs that exercise the syscall ABI but do not depend on filesystems.
* Tests can drive the shell by calling `syscall::feed_stdin` and inspect user output through `syscall::take_stdout`.

## Rebuilding for other architectures

The build script accepts `x86_64` and `arm64`. Building the AArch64 binaries requires an `aarch64-linux-gnu-*` toolchain to be installed. When the toolchain is not available the script can still be invoked for `x86_64`; prebuilt ARM64 binaries that match the ELF loader expectations ship with the repository.
