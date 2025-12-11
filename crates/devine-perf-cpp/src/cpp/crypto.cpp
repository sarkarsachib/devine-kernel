#include "crypto.hpp"

namespace devine {
namespace crypto {
    uint64_t compute_hash(const uint8_t* data, size_t len) {
        uint64_t hash = 0xcbf29ce484222325ULL;
        const uint64_t prime = 0x100000001b3ULL;
        
        for (size_t i = 0; i < len; i++) {
            hash ^= data[i];
            hash *= prime;
        }
        
        return hash;
    }
}
}

extern "C" {
    uint64_t fast_hash(const uint8_t* data, size_t len) {
        return devine::crypto::compute_hash(data, len);
    }
    
    void fast_memcpy(uint8_t* dest, const uint8_t* src, size_t len) {
        #if defined(__x86_64__)
        asm volatile (
            "rep movsb"
            : "+D"(dest), "+S"(src), "+c"(len)
            :
            : "memory"
        );
        #elif defined(__aarch64__)
        for (size_t i = 0; i < len; i++) {
            dest[i] = src[i];
        }
        #else
        for (size_t i = 0; i < len; i++) {
            dest[i] = src[i];
        }
        #endif
    }
}

void* operator new(size_t, void* ptr) noexcept {
    return ptr;
}

void* operator new(size_t) noexcept {
    return nullptr;
}

void* operator new[](size_t) noexcept {
    return nullptr;
}

void operator delete(void*) noexcept {}
void operator delete(void*, size_t) noexcept {}
void operator delete[](void*) noexcept {}
void operator delete[](void*, size_t) noexcept {}

extern "C" void __cxa_pure_virtual() {
    while (1);
}
