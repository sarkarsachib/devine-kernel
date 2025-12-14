/* String utility functions for TTY subsystem */

#include "../../include/types.h"
#include "../../include/console.h"

// Basic string comparison
i32 strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

// String length
u64 strlen(const char* s) {
    const char* p = s;
    while (*p) {
        p++;
    }
    return p - s;
}

// String copy with bounds checking
char* strncpy(char* dest, const char* src, u64 n) {
    char* original_dest = dest;
    
    while (n && *src) {
        *dest++ = *src++;
        n--;
    }
    
    // Pad with nulls if needed
    while (n--) {
        *dest++ = '\0';
    }
    
    return original_dest;
}

// Basic snprintf implementation
i32 snprintf(char* str, u64 size, const char* format, ...) {
    if (!str || size == 0) {
        return 0;
    }
    
    // Very basic implementation - just copy format string
    // In a real implementation, you'd parse format specifiers
    u64 len = strlen(format);
    u64 to_copy = (len < size - 1) ? len : size - 1;
    
    strncpy(str, format, to_copy);
    str[to_copy] = '\0';
    
    return (i32)to_copy;
}

// Memory utilities
void* memset(void* s, u8 c, u64 n) {
    u8* p = (u8*)s;
    while (n--) {
        *p++ = c;
    }
    return s;
}

void* memcpy(void* dest, const void* src, u64 n) {
    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}