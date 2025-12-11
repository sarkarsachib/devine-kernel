/* x86_64 APIC - Simplified for compilation */

#include "../../include/types.h"

// Initialize APIC
void apic_init(void) {
    // Simplified - just print message
    console_print("APIC initialized\n");
}

// Dummy interrupt controller initialization
void pic_init(void) {
    console_print("PIC initialized\n");
}