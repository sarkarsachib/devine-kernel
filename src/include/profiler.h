#pragma once

#include "types.h"

#ifdef PROFILER_ENABLED

extern void profiler_start_timer(const char* name);
extern void profiler_end_timer(const char* name);
extern void profiler_increment_counter(const char* name);
extern u64 profiler_get_counter(const char* name);

#define PROFILE_START(name) profiler_start_timer(name)
#define PROFILE_END(name) profiler_end_timer(name)
#define PROFILE_COUNT(name) profiler_increment_counter(name)

#else

#define PROFILE_START(name) ((void)0)
#define PROFILE_END(name) ((void)0)
#define PROFILE_COUNT(name) ((void)0)

#endif
