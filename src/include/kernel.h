/* Main header file for the OS kernel */

#ifndef KERNEL_H
#define KERNEL_H

#include "types.h"
#include "console.h"

// System time
extern u64 system_time;

// Forward declarations
void kmain(void);

// Hardware initialization
void gdt_init(void);
void idt_init(void);
void apic_init(void);
void pic_init(void);

// System functions
void enable_interrupts(void);
void disable_interrupts(void);

// Utility functions
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, usize n);
i32 strcmp(const char* s1, const char* s2);
usize strlen(const char* str);
void* memset(void* ptr, int value, usize num);

#endif // KERNEL_H