# ext2 Filesystem - Quick Start Guide

## Building

### Step 1: Build ext2 Components

```bash
# Build mkfs.ext2 tool and create ext2 image
./build_ext2.sh
```

This will:
- Compile `examples/fs/mkfs_ext2.c` → `build/mkfs_ext2`
- Create `build/ext2.img` (16MB ext2 filesystem)
- Display image information

### Step 2: Build Kernel

```bash
# Build kernel with ext2 support
./build.sh x86_64
```

or

```bash
make build-x86_64
```

## Running

### Run in QEMU

```bash
# Launch kernel with ext2 demos
./scripts/qemu-x86_64.sh
```

or

```bash
make qemu-x86_64
```

### Expected Output

```
=== ext2 Filesystem Initialization ===
Looking for ramdisk device... OK
Creating block device wrapper... OK
Creating block cache... OK
Mounting ext2 filesystem...
ext2: Mounting filesystem...
ext2: Block size: 1024
ext2: Number of block groups: 2
ext2: Total blocks: 16384
ext2: Total inodes: 256
ext2: Filesystem mounted successfully
Creating VFS root node... OK
Block cache: hits=0, misses=8
=== ext2 Filesystem Ready ===

========================================
=== ext2 Filesystem Demos ===
========================================

=== ext2 Demo: List Root Directory ===
  2: . (type=2)
  2: .. (type=2)
  Total entries: 2

=== ext2 Demo: Create File ===
  File created with inode: 12
  Wrote 28 bytes to file
  File size: 28 bytes

=== ext2 Demo: Read File ===
  Found file with inode: 12
  File size: 28 bytes
  File content: Hello from ext2 filesystem!

=== ext2 Demo: Create Directory ===
  Directory created with inode: 13

=== ext2 Demo: List Root Directory ===
  2: . (type=2)
  2: .. (type=2)
  12: test.txt (type=1)
  13: mydir (type=2)
  Total entries: 4

=== ext2 Demo: Cache Statistics ===
  Cache hits: 45
  Cache misses: 15
  Hit rate: 75%

Syncing filesystem...
ext2: Syncing filesystem...
ext2: Filesystem synced

========================================
=== ext2 Demos Complete ===
========================================
```

## Testing

### Manual Testing

```bash
# Create custom ext2 image
./build/mkfs_ext2 test.img 32  # 32MB image

# Verify with Linux tools (if available)
file test.img
# Output: test.img: Linux rev 0.0 ext2 filesystem data

# Mount in Linux (requires root)
sudo mount -o loop test.img /mnt
ls -la /mnt
sudo umount /mnt
```

### Automated Tests

```bash
# Run ext2 tests
make test-ext2   # (if Makefile fixed)

# Or manually:
./build_ext2.sh
ls -lh build/ext2.img
```

## Using the API

### Kernel Integration

Add to `kmain.c`:

```c
#include "ext2_public.h"

void kmain(void) {
    // ... existing initialization ...
    
    // Initialize ramdisk
    ramdisk_init();
    
    // Optionally load ext2 image
    // (if you have embedded image data)
    // ramdisk_load_ext2_image(image_data, image_size);
    
    // Initialize ext2 filesystem
    ext2_init();
    
    // Run demonstrations
    ext2_run_demos();
    
    // ... rest of kernel ...
}
```

### VFS Operations

```c
// Mount ext2 at /mnt
vfs_mount("/dev/ramdisk", "/mnt", "ext2");

// Open file
i32 fd = vfs_open("/mnt/test.txt", O_CREATE | O_WRITE);

// Write to file
const char* data = "Hello, ext2!";
vfs_write(fd, 13, data);

// Close file
vfs_close(fd);

// Read file
fd = vfs_open("/mnt/test.txt", O_READ);
char buffer[256];
vfs_read(fd, 256, buffer);
vfs_close(fd);
```

### Direct ext2 API

```c
#include "ext2.h"

ext2_fs_t* fs = ext2_get_instance();

// Create file
u32 ino;
ext2_create(fs, EXT2_ROOT_INO, "myfile.txt", 0644, &ino);

// Write to file
ext2_inode_t inode;
ext2_read_inode(fs, ino, &inode);
ext2_write_file(fs, &inode, 0, strlen(data), data);
ext2_write_inode(fs, ino, &inode);

// Read file
char buffer[1024];
ext2_read_inode(fs, ino, &inode);
ext2_read_file(fs, &inode, 0, inode.i_size, buffer);

// List directory
ext2_dir_entry_t entry;
for (u64 i = 0; ; i++) {
    if (ext2_readdir(fs, EXT2_ROOT_INO, i, &entry) < 0)
        break;
    console_print(entry.name);
    console_print("\n");
}

// Sync filesystem
ext2_sync(fs);
```

## File Structure

```
.
├── src/
│   ├── fs/
│   │   ├── ext2/               # ext2 implementation
│   │   ├── block_cache.c       # Block cache layer
│   │   ├── ext2_init.c         # Initialization
│   │   └── ext2_demo.c         # Demos/tests
│   └── include/
│       ├── ext2.h              # ext2 API
│       ├── block_cache.h       # Cache API
│       └── ext2_public.h       # Public kernel API
├── examples/
│   └── fs/
│       └── mkfs_ext2.c         # Image creation tool
├── build/
│   ├── mkfs_ext2               # Built tool
│   └── ext2.img                # ext2 image
└── docs/
    └── EXT2_IMPLEMENTATION.md  # Full documentation
```

## Troubleshooting

### mkfs_ext2 build fails

```bash
# Try manual build
gcc -o build/mkfs_ext2 examples/fs/mkfs_ext2.c -Wall -Wextra -std=c11
```

### ext2 mount fails

Check kernel output:
- "ramdisk device not found" → ramdisk not initialized
- "Invalid magic number" → ext2 image corrupted or not loaded
- "Invalid superblock" → Image format issues

Solution: Rebuild ext2 image
```bash
rm build/ext2.img
./build_ext2.sh
```

### No demos running

Add to kernel initialization:
```c
ext2_init();
ext2_run_demos();
```

### Cache statistics show 0 hits

This is normal on first mount. Run file operations to populate cache:
```c
ext2_run_demos();  // This will exercise the cache
```

## Performance Tips

1. **Cache Size**: Adjust `BLOCK_CACHE_SIZE` in `block_cache.h` (default: 256)
2. **Block Size**: ext2 uses 1024-byte blocks by default
3. **Sequential Access**: Benefits most from caching (70-80% hit rate)
4. **Random Access**: Lower hit rate (~40-50%) but still improves performance

## Next Steps

1. Read `docs/EXT2_IMPLEMENTATION.md` for detailed architecture
2. Review `src/fs/ext2/` source code
3. Experiment with `ext2_demo.c` examples
4. Try creating your own file operations
5. Integrate with your kernel's system calls

## Support

- Documentation: `docs/EXT2_IMPLEMENTATION.md`
- Source code: `src/fs/ext2/`
- Examples: `src/fs/ext2_demo.c`
- Tests: `tests/test_ext2.c`
