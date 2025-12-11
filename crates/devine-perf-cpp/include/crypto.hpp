#pragma once

#include <cstdint>
#include <cstddef>

extern "C" {
    uint64_t fast_hash(const uint8_t* data, size_t len);
    void fast_memcpy(uint8_t* dest, const uint8_t* src, size_t len);
}

namespace devine {
namespace crypto {
    uint64_t compute_hash(const uint8_t* data, size_t len);
}
}
