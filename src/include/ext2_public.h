#pragma once

#include "types.h"

// Public ext2 API for kernel initialization

// Initialize ext2 filesystem
void ext2_init(void);

// Cleanup ext2 filesystem
void ext2_cleanup(void);

// Run ext2 demos
void ext2_run_demos(void);

// Initialize ramdisk
void ramdisk_init(void);

// Load ext2 image into ramdisk
void ramdisk_load_ext2_image(void* image_data, u64 image_size);
