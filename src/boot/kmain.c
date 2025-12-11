/* Kernel Main Entry Point */

#include "../include/kernel.h"

// Forward declarations for driver subsystems
extern void device_init(void);
extern void pci_init(void);
extern void dt_init(void* dtb_addr);
extern void uart16550_init(void);
extern void pl011_init(void* dt_dev);

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
    
    // Initialize device subsystem
    console_print("\n=== Device Initialization ===\n");
    device_init();
    
#ifdef __x86_64__
    // Initialize PCI bus for x86_64
    pci_init();
    
    // Initialize UART for console
    uart16550_init();
#elif __aarch64__
    // Initialize Device Tree for ARM64
    dt_init(NULL);
    
    // Initialize PL011 UART for console
    pl011_init(NULL);
#endif
    
    console_print("=== Kernel Ready ===\n");
    console_print("System initialized successfully!\n");
    console_print("All drivers loaded and devices enumerated.\n");
    
    // Main kernel loop
    for(;;) {
        // System idle loop - halt CPU
        __asm__ volatile("hlt");
    }
}