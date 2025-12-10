/* Kernel Main Entry Point */

#include "../include/kernel.h"

// System time - moved to utils.c
// u64 system_time = 0;

// Main kernel function
void kmain(void) {
    // Clear screen and show banner
    console_clear();
    console_print("=== OS Kernel Starting ===\n");
    console_print("Initializing system components...\n");
    
    // Initialize hardware abstraction
    console_print("Initializing GDT... ");
    gdt_init();
    
    console_print("Initializing IDT... ");
    idt_init();
    
    console_print("Initializing APIC... ");
    apic_init();
    
    // Enable interrupts
    console_print("Enabling Interrupts... ");
    enable_interrupts();
    console_print("OK\n");
    
    console_print("=== Kernel Ready ===\n");
    console_print("System initialized successfully!\n");
    
    // Main kernel loop
    for(;;) {
        // System idle loop - halt CPU
        __asm__ volatile("hlt");
    }
}