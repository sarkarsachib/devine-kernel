/* ext2 Filesystem Tests */

#include "../src/include/types.h"
#include "../src/include/ext2.h"
#include "../src/include/block_cache.h"
#include "../src/include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);
extern void* memset(void* ptr, int value, u64 num);
extern i32 strcmp(const char* s1, const char* s2);

// Mock block device for testing
typedef struct test_block_device {
    u8* data;
    u64 size;
    u64 block_size;
} test_block_device_t;

static i32 test_device_read_block(void* device, u64 block_num, void* buffer) {
    test_block_device_t* dev = (test_block_device_t*)device;
    u64 offset = block_num * dev->block_size;
    
    if (offset >= dev->size) {
        return ERR_INVALID;
    }
    
    memcpy(buffer, dev->data + offset, dev->block_size);
    return dev->block_size;
}

static i32 test_device_write_block(void* device, u64 block_num, const void* buffer) {
    test_block_device_t* dev = (test_block_device_t*)device;
    u64 offset = block_num * dev->block_size;
    
    if (offset >= dev->size) {
        return ERR_INVALID;
    }
    
    memcpy(dev->data + offset, buffer, dev->block_size);
    return dev->block_size;
}

static i32 test_device_get_block_size(void* device) {
    test_block_device_t* dev = (test_block_device_t*)device;
    return dev->block_size;
}

static i32 test_device_get_num_blocks(void* device) {
    test_block_device_t* dev = (test_block_device_t*)device;
    return dev->size / dev->block_size;
}

static block_device_ops_t test_device_ops = {
    .read_block = test_device_read_block,
    .write_block = test_device_write_block,
    .get_block_size = test_device_get_block_size,
    .get_num_blocks = test_device_get_num_blocks,
};

// Test block cache
void test_block_cache(void) {
    console_print("Testing block cache...\n");
    
    test_block_device_t* dev = (test_block_device_t*)malloc(sizeof(test_block_device_t));
    dev->size = 1024 * 1024;
    dev->block_size = 1024;
    dev->data = (u8*)malloc(dev->size);
    memset(dev->data, 0, dev->size);
    
    block_device_t bd;
    bd.device_data = dev;
    bd.ops = &test_device_ops;
    
    block_cache_t* cache = block_cache_create(&bd, dev->block_size);
    if (!cache) {
        console_print("  FAIL: Failed to create block cache\n");
        return;
    }
    
    u8 write_buffer[1024];
    for (u32 i = 0; i < 1024; i++) {
        write_buffer[i] = i & 0xFF;
    }
    
    i32 result = block_cache_write(cache, 10, write_buffer);
    if (result < 0) {
        console_print("  FAIL: Cache write failed\n");
        return;
    }
    
    u8 read_buffer[1024];
    result = block_cache_read(cache, 10, read_buffer);
    if (result < 0) {
        console_print("  FAIL: Cache read failed\n");
        return;
    }
    
    for (u32 i = 0; i < 1024; i++) {
        if (read_buffer[i] != (i & 0xFF)) {
            console_print("  FAIL: Data mismatch\n");
            return;
        }
    }
    
    u64 hits, misses;
    block_cache_stats(cache, &hits, &misses);
    console_print("  Cache hits: ");
    console_print_dec(hits);
    console_print(", misses: ");
    console_print_dec(misses);
    console_print("\n");
    
    block_cache_flush(cache);
    block_cache_destroy(cache);
    
    free(dev->data);
    free(dev);
    
    console_print("  PASS: Block cache test passed\n");
}

// Test ext2 basic operations
void test_ext2_basic(void) {
    console_print("Testing ext2 basic operations...\n");
    console_print("  (This requires a pre-created ext2 image)\n");
}

// Run all tests
void run_ext2_tests(void) {
    console_print("=== ext2 Filesystem Tests ===\n");
    
    test_block_cache();
    test_ext2_basic();
    
    console_print("=== Tests Complete ===\n");
}
