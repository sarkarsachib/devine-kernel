/* Simple x86_64 GDT Implementation */

#include "../../include/types.h"

// GDT entry structure
typedef struct {
    u16 limit_low;     // Limit (bits 0-15)
    u16 base_low;      // Base (bits 0-15)
    u8  base_mid;      // Base (bits 16-23)
    u8  access;        // Access byte
    u8  granularity;   // Granularity (bits 0-3) + limit (bits 16-19)
    u8  base_high;     // Base (bits 24-31)
} __attribute__((packed)) gdt_entry_t;

// GDT pointer structure
typedef struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) gdt_pointer_t;

// GDT entries
static gdt_entry_t gdt[3];
static gdt_pointer_t gdt_ptr;

// Set a GDT entry
static void gdt_set_entry(u16 index, u32 base, u32 limit, u8 access, u8 granularity) {
    gdt[index].base_low = base & 0xFFFF;
    gdt[index].base_mid = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    
    gdt[index].limit_low = limit & 0xFFFF;
    gdt[index].granularity = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
    
    gdt[index].access = access;
}

// Initialize GDT
void gdt_init(void) {
    // Setup GDT pointer
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (u64)gdt;
    
    // Clear GDT
    memset(gdt, 0, sizeof(gdt));
    
    // Null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);
    
    // Kernel code segment
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);
    
    // Kernel data segment
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xA0);
    
    // Load GDT
    __asm__ volatile("lgdt %0" : : "m"(gdt_ptr));
    
    // Load segment registers
    __asm__ volatile("mov %0, %%ds" : : "r"((u16)0x10));
    __asm__ volatile("mov %0, %%es" : : "r"((u16)0x10));
    __asm__ volatile("mov %0, %%fs" : : "r"((u16)0x10));
    __asm__ volatile("mov %0, %%gs" : : "r"((u16)0x10));
    __asm__ volatile("mov %0, %%ss" : : "r"((u16)0x10));
}

// Dummy functions for compatibility
void idt_init(void) {
    // Empty implementation
}

void apic_init(void) {
    // Empty implementation
}

void pic_init(void) {
    // Empty implementation
}