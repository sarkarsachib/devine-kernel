#include "profiler.hpp"

#define MAX_PROFILER_ENTRIES 128
#define MAX_NAME_LEN 64

struct ProfilerEntry {
    char name[MAX_NAME_LEN];
    uint64_t start_time;
    uint64_t total_time;
    uint64_t count;
    bool active;
};

static ProfilerEntry entries[MAX_PROFILER_ENTRIES];
static int entry_count = 0;
static bool profiler_enabled = true;

static inline uint64_t read_tsc() {
#ifdef __x86_64__
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
#elif __aarch64__
    uint64_t val;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
#else
    return 0;
#endif
}

static int find_entry(const char* name) {
    for (int i = 0; i < entry_count; i++) {
        if (__builtin_strcmp(entries[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int create_entry(const char* name) {
    if (entry_count >= MAX_PROFILER_ENTRIES) {
        return -1;
    }
    
    int idx = entry_count++;
    __builtin_strncpy(entries[idx].name, name, MAX_NAME_LEN - 1);
    entries[idx].name[MAX_NAME_LEN - 1] = '\0';
    entries[idx].start_time = 0;
    entries[idx].total_time = 0;
    entries[idx].count = 0;
    entries[idx].active = false;
    
    return idx;
}

extern "C" {

uint64_t profiler_rdtsc() {
    return read_tsc();
}

void profiler_start_timer(const char* name) {
    if (!profiler_enabled || !name) return;
    
    int idx = find_entry(name);
    if (idx < 0) {
        idx = create_entry(name);
    }
    
    if (idx >= 0) {
        entries[idx].start_time = read_tsc();
        entries[idx].active = true;
    }
}

void profiler_end_timer(const char* name) {
    uint64_t end_time = read_tsc();
    
    if (!profiler_enabled || !name) return;
    
    int idx = find_entry(name);
    if (idx >= 0 && entries[idx].active) {
        uint64_t elapsed = end_time - entries[idx].start_time;
        entries[idx].total_time += elapsed;
        entries[idx].count++;
        entries[idx].active = false;
    }
}

void profiler_increment_counter(const char* name) {
    if (!profiler_enabled || !name) return;
    
    int idx = find_entry(name);
    if (idx < 0) {
        idx = create_entry(name);
    }
    
    if (idx >= 0) {
        entries[idx].count++;
    }
}

uint64_t profiler_get_counter(const char* name) {
    if (!name) return 0;
    
    int idx = find_entry(name);
    if (idx >= 0) {
        return entries[idx].count;
    }
    
    return 0;
}

void profiler_reset() {
    for (int i = 0; i < entry_count; i++) {
        entries[i].start_time = 0;
        entries[i].total_time = 0;
        entries[i].count = 0;
        entries[i].active = false;
    }
    entry_count = 0;
}

void profiler_dump() {
}

}

namespace devine {
namespace profiler {

uint64_t read_timestamp_counter() {
    return profiler_rdtsc();
}

void start_measurement(const char* name) {
    profiler_start_timer(name);
}

void end_measurement(const char* name) {
    profiler_end_timer(name);
}

void increment(const char* name) {
    profiler_increment_counter(name);
}

uint64_t get_count(const char* name) {
    return profiler_get_counter(name);
}

void reset_all() {
    profiler_reset();
}

void dump_stats() {
    profiler_dump();
}

}
}
