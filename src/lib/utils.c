/* Kernel utility functions */

#include "../include/types.h"

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

usize strlen(const char* str) {
    const char* s;
    for (s = str; *s; ++s);
    return (usize)(s - str);
}

void* memset(void* ptr, int value, usize num) {
    u8* p = (u8*)ptr;
    while (num--) {
        *p++ = (u8)value;
    }
    return ptr;
}

// CPU state functions
void enable_interrupts(void) {
    __asm__ volatile("sti");
}

void disable_interrupts(void) {
    __asm__ volatile("cli");
}