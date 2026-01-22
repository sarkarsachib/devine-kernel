/* Kernel utility functions */

#include "../include/types.h"

// Global system time
u64 system_time = 0;

// String functions
char* strcpy(char* dest, const char* src) {
    char* original = dest;
    while ((*dest++ = *src++) != '\0');
    return original;
}

char* strncpy(char* dest, const char* src, usize n) {
    char* original = dest;
    while (n && (*dest++ = *src++) != '\0')
        n--;
    while (n--)
        *dest++ = '\0';
    return original;
}

i32 strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(u8*)s1 - *(u8*)s2;
}

i32 strncmp(const char* s1, const char* s2, usize n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(u8*)s1 - *(u8*)s2;
}

usize strlen(const char* str) {
    const char* s;
    for (s = str; *s; ++s);
    return (usize)(s - str);
}

char* strchr(const char* str, int c) {
    while (*str) {
        if (*str == (char)c) return (char*)str;
        str++;
    }
    return NULL;
}

char* strrchr(const char* str, int c) {
    const char* last = NULL;
    while (*str) {
        if (*str == (char)c) last = str;
        str++;
    }
    return (char*)last;
}

char* strtok(char* str, const char* delim) {
    static char* next = NULL;
    
    if (str) next = str;
    if (!next) return NULL;
    
    while (*next && strchr(delim, *next)) next++;
    if (!*next) return NULL;
    
    char* token = next;
    while (*next && !strchr(delim, *next)) next++;
    if (*next) *next++ = '\0';
    
    return token;
}

void* memset(void* ptr, int value, usize num) {
    u8* p = (u8*)ptr;
    while (num--) {
        *p++ = (u8)value;
    }
    return ptr;
}

void* memcpy(void* dest, const void* src, usize n) {
    u8* d = (u8*)dest;
    const u8* s = (const u8*)src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

// Simple bump allocator for kernel heap
#define HEAP_SIZE (1024 * 1024)  // 1MB heap
static u8 heap[HEAP_SIZE];
static usize heap_pos = 0;

void* malloc(usize size) {
    if (heap_pos + size > HEAP_SIZE) {
        return NULL;
    }
    
    void* ptr = &heap[heap_pos];
    heap_pos += size;
    heap_pos = ALIGN_UP(heap_pos, 16);
    
    return ptr;
}

void free(void* ptr) {
    // Simple bump allocator doesn't support free
    (void)ptr;
}

// CPU state functions
void enable_interrupts(void) {
    __asm__ volatile("sti");
}

void disable_interrupts(void) {
    __asm__ volatile("cli");
}