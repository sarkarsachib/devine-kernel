/* Block Cache Layer - LRU Cache Implementation */

#include "../include/types.h"
#include "../include/block_cache.h"
#include "../include/console.h"

extern void* malloc(u64 size);
extern void free(void* ptr);
extern void* memset(void* ptr, int value, u64 num);
extern void* memcpy(void* dest, const void* src, u64 num);
extern u64 system_time;

// Helper to get block device operations
static inline block_device_ops_t* get_device_ops(void* device) {
    if (!device) return NULL;
    block_device_t* bd = (block_device_t*)device;
    return bd->ops;
}

static inline void* get_device_data(void* device) {
    if (!device) return NULL;
    block_device_t* bd = (block_device_t*)device;
    return bd->device_data;
}

// Remove entry from LRU list
static void lru_remove(block_cache_t* cache, block_cache_entry_t* entry) {
    if (entry->prev) {
        entry->prev->next = entry->next;
    } else {
        cache->lru_head = entry->next;
    }
    
    if (entry->next) {
        entry->next->prev = entry->prev;
    } else {
        cache->lru_tail = entry->prev;
    }
    
    entry->prev = NULL;
    entry->next = NULL;
}

// Add entry to front of LRU list (most recently used)
static void lru_add_front(block_cache_t* cache, block_cache_entry_t* entry) {
    entry->prev = NULL;
    entry->next = cache->lru_head;
    
    if (cache->lru_head) {
        cache->lru_head->prev = entry;
    } else {
        cache->lru_tail = entry;
    }
    
    cache->lru_head = entry;
}

// Find cache entry for a block number
static block_cache_entry_t* cache_find(block_cache_t* cache, u64 block_num) {
    for (u32 i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (cache->entries[i].valid && cache->entries[i].block_num == block_num) {
            return &cache->entries[i];
        }
    }
    return NULL;
}

// Find least recently used entry
static block_cache_entry_t* cache_find_lru(block_cache_t* cache) {
    return cache->lru_tail;
}

// Flush a single cache entry
static i32 cache_flush_entry(block_cache_t* cache, block_cache_entry_t* entry) {
    if (!entry->dirty || !entry->valid) {
        return ERR_SUCCESS;
    }
    
    block_device_ops_t* ops = get_device_ops(cache->block_device);
    if (!ops || !ops->write_block) {
        return ERR_INVALID;
    }
    
    void* device = get_device_data(cache->block_device);
    i32 result = ops->write_block(device, entry->block_num, entry->data);
    if (result >= 0) {
        entry->dirty = false;
    }
    
    return result;
}

// Create a new block cache
block_cache_t* block_cache_create(void* block_device, u64 block_size) {
    block_cache_t* cache = (block_cache_t*)malloc(sizeof(block_cache_t));
    if (!cache) {
        return NULL;
    }
    
    memset(cache, 0, sizeof(block_cache_t));
    cache->block_device = block_device;
    cache->block_size = block_size;
    cache->hits = 0;
    cache->misses = 0;
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    
    // Initialize cache entries
    for (u32 i = 0; i < BLOCK_CACHE_SIZE; i++) {
        cache->entries[i].data = (u8*)malloc(block_size);
        if (!cache->entries[i].data) {
            // Cleanup on failure
            for (u32 j = 0; j < i; j++) {
                free(cache->entries[j].data);
            }
            free(cache);
            return NULL;
        }
        
        cache->entries[i].valid = false;
        cache->entries[i].dirty = false;
        cache->entries[i].block_num = 0;
        cache->entries[i].last_access = 0;
        cache->entries[i].next = NULL;
        cache->entries[i].prev = NULL;
    }
    
    return cache;
}

// Destroy block cache
void block_cache_destroy(block_cache_t* cache) {
    if (!cache) return;
    
    // Flush all dirty blocks
    block_cache_flush(cache);
    
    // Free cache entries
    for (u32 i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (cache->entries[i].data) {
            free(cache->entries[i].data);
        }
    }
    
    free(cache);
}

// Read a block through cache
i32 block_cache_read(block_cache_t* cache, u64 block_num, void* buffer) {
    if (!cache || !buffer) {
        return ERR_INVALID;
    }
    
    // Check if block is in cache
    block_cache_entry_t* entry = cache_find(cache, block_num);
    
    if (entry) {
        // Cache hit
        cache->hits++;
        memcpy(buffer, entry->data, cache->block_size);
        
        // Update LRU
        entry->last_access = system_time;
        lru_remove(cache, entry);
        lru_add_front(cache, entry);
        
        return cache->block_size;
    }
    
    // Cache miss
    cache->misses++;
    
    // Find LRU entry to evict
    entry = cache_find_lru(cache);
    if (!entry) {
        // Find first invalid entry
        for (u32 i = 0; i < BLOCK_CACHE_SIZE; i++) {
            if (!cache->entries[i].valid) {
                entry = &cache->entries[i];
                break;
            }
        }
    }
    
    if (!entry) {
        return ERR_BUSY;
    }
    
    // Flush if dirty
    if (entry->valid && entry->dirty) {
        cache_flush_entry(cache, entry);
    }
    
    // Remove from LRU if valid
    if (entry->valid) {
        lru_remove(cache, entry);
    }
    
    // Read block from device
    block_device_ops_t* ops = get_device_ops(cache->block_device);
    if (!ops || !ops->read_block) {
        return ERR_INVALID;
    }
    
    void* device = get_device_data(cache->block_device);
    i32 result = ops->read_block(device, block_num, entry->data);
    if (result < 0) {
        return result;
    }
    
    // Update entry
    entry->block_num = block_num;
    entry->valid = true;
    entry->dirty = false;
    entry->last_access = system_time;
    
    // Add to front of LRU
    lru_add_front(cache, entry);
    
    // Copy to output buffer
    memcpy(buffer, entry->data, cache->block_size);
    
    return cache->block_size;
}

// Write a block through cache
i32 block_cache_write(block_cache_t* cache, u64 block_num, const void* buffer) {
    if (!cache || !buffer) {
        return ERR_INVALID;
    }
    
    // Check if block is in cache
    block_cache_entry_t* entry = cache_find(cache, block_num);
    
    if (!entry) {
        // Find LRU entry to evict
        entry = cache_find_lru(cache);
        if (!entry) {
            // Find first invalid entry
            for (u32 i = 0; i < BLOCK_CACHE_SIZE; i++) {
                if (!cache->entries[i].valid) {
                    entry = &cache->entries[i];
                    break;
                }
            }
        }
        
        if (!entry) {
            return ERR_BUSY;
        }
        
        // Flush if dirty
        if (entry->valid && entry->dirty) {
            cache_flush_entry(cache, entry);
        }
        
        // Remove from LRU if valid
        if (entry->valid) {
            lru_remove(cache, entry);
        }
        
        entry->block_num = block_num;
        entry->valid = true;
    } else {
        // Update LRU
        lru_remove(cache, entry);
    }
    
    // Write to cache
    memcpy(entry->data, buffer, cache->block_size);
    entry->dirty = true;
    entry->last_access = system_time;
    
    // Add to front of LRU
    lru_add_front(cache, entry);
    
    return cache->block_size;
}

// Flush all dirty blocks
i32 block_cache_flush(block_cache_t* cache) {
    if (!cache) {
        return ERR_INVALID;
    }
    
    i32 errors = 0;
    for (u32 i = 0; i < BLOCK_CACHE_SIZE; i++) {
        if (cache->entries[i].valid && cache->entries[i].dirty) {
            if (cache_flush_entry(cache, &cache->entries[i]) < 0) {
                errors++;
            }
        }
    }
    
    return errors > 0 ? ERR_INVALID : ERR_SUCCESS;
}

// Invalidate a specific block
i32 block_cache_invalidate(block_cache_t* cache, u64 block_num) {
    if (!cache) {
        return ERR_INVALID;
    }
    
    block_cache_entry_t* entry = cache_find(cache, block_num);
    if (entry) {
        // Flush if dirty
        if (entry->dirty) {
            cache_flush_entry(cache, entry);
        }
        
        // Remove from LRU
        lru_remove(cache, entry);
        entry->valid = false;
    }
    
    return ERR_SUCCESS;
}

// Get cache statistics
void block_cache_stats(block_cache_t* cache, u64* hits, u64* misses) {
    if (!cache) return;
    
    if (hits) *hits = cache->hits;
    if (misses) *misses = cache->misses;
}
