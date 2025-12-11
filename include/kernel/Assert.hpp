#pragma once
#include "Log.hpp"

#define K_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            kernel::panic(message); \
        } \
    } while (0)
