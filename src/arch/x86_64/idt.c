/* x86_64 Interrupt Descriptor Table (IDT) Implementation */

#include "../../include/types.h"

// IDT entry structure
typedef struct {
    u16 offset_low;     // Offset bits 0-15
    u16 selector;       // Code segment selector
    u8  ist;           // IST index (bits 0-2), reserved (bits 3-7)
    u8  type_attr;     // Type and attributes
    u16 offset_mid;    // Offset bits 16-31
    u32 offset_high;   // Offset bits 32-63
    u16 zero;          // Reserved
} __attribute__((packed)) idt_entry_t;

// IDT pointer structure
typedef struct {
    u16 limit;
    u64 base;
} __attribute__((packed)) idt_pointer_t;

// Global IDT
static idt_entry_t idt[256];
static idt_pointer_t idt_ptr;

// Set an IDT entry
static void idt_set_entry(u8 index, u64 base, u16 selector, u8 type_attr, u8 ist) {
    idt[index].offset_low = base & 0xFFFF;
    idt[index].offset_mid = (base >> 16) & 0xFFFF;
    idt[index].offset_high = (base >> 32) & 0xFFFFFFFF;
    idt[index].selector = selector;
    idt[index].ist = ist & 0x7;
    idt[index].type_attr = type_attr;
    idt[index].zero = 0;
}

// Initialize IDT
void idt_init(void) {
    // Setup IDT pointer
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (u64)idt;

    // Clear IDT
    memset(idt, 0, sizeof(idt));

    // Setup a simple IDT with default handlers
    for (u32 i = 0; i < 256; i++) {
        idt_set_entry(i, (u64)0x1000, 0x08, 0x8E, 0);  // Simple default handler
    }

    // Load IDT
    __asm__ volatile("lidt %0" : : "m"(idt_ptr));
}

// Enable/disable interrupts
void enable_interrupts(void) {
    __asm__ volatile("sti");
}

void disable_interrupts(void) {
    __asm__ volatile("cli");
}