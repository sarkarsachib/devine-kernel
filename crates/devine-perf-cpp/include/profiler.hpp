#pragma once

#include <cstdint>
#include <cstddef>

extern "C" {
    uint64_t profiler_rdtsc();
    void profiler_start_timer(const char* name);
    void profiler_end_timer(const char* name);
    void profiler_increment_counter(const char* name);
    uint64_t profiler_get_counter(const char* name);
    void profiler_reset();
    void profiler_dump();
}

namespace devine {
namespace profiler {
    uint64_t read_timestamp_counter();
    void start_measurement(const char* name);
    void end_measurement(const char* name);
    void increment(const char* name);
    uint64_t get_count(const char* name);
    void reset_all();
    void dump_stats();
}
}
