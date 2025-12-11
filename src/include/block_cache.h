#pragma once

#include "types.h"

#define BLOCK_CACHE_SIZE 256
#define BLOCK_CACHE_BLOCK_SIZE 1024

// Block cache entry
typedef struct block_cache_entry {
    u64 block_num;
    u8* data;
    bool dirty;
    bool valid;
    u64 last_access;
    struct block_cache_entry* next;
    struct block_cache_entry* prev;
} block_cache_entry_t;

// Block cache structure
typedef struct block_cache {
    block_cache_entry_t entries[BLOCK_CACHE_SIZE];
    block_cache_entry_t* lru_head;
    block_cache_entry_t* lru_tail;
    void* block_device;
    u64 hits;
    u64 misses;
    u64 block_size;
} block_cache_t;

// Block device interface for cache
typedef struct block_device_ops {
    i32 (*read_block)(void* device, u64 block_num, void* buffer);
    i32 (*write_block)(void* device, u64 block_num, const void* buffer);
    i32 (*get_block_size)(void* device);
    i32 (*get_num_blocks)(void* device);
} block_device_ops_t;

typedef struct block_device {
    void* device_data;
    block_device_ops_t* ops;
} block_device_t;

// Block cache operations
block_cache_t* block_cache_create(void* block_device, u64 block_size);
void block_cache_destroy(block_cache_t* cache);
i32 block_cache_read(block_cache_t* cache, u64 block_num, void* buffer);
i32 block_cache_write(block_cache_t* cache, u64 block_num, const void* buffer);
i32 block_cache_flush(block_cache_t* cache);
i32 block_cache_invalidate(block_cache_t* cache, u64 block_num);
void block_cache_stats(block_cache_t* cache, u64* hits, u64* misses);
