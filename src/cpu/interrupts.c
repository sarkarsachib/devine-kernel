/* Interrupt Handler Implementation */

#include "../../include/types.h"
#include "../../include/console.h"

// Global interrupt controller
typedef struct {
    u64 pic_master_command;
    u64 pic_master_data;
    u64 pic_slave_command;
    u64 pic_slave_data;
} pic_t;

static pic_t* pic = (pic_t*)0x20;

// Forward declarations
void isr_handler(u8 int_no);
void irq_handler(u8 int_no);

// Exception names
static const char* exception_names[] = {
    "#DE Divide Error",
    "#DB Debug",
    "NMI Interrupt", 
    "#BP Breakpoint",
    "#OF Overflow",
    "#BR Bound Range Exceeded",
    "#UD Invalid Opcode",
    "#NM Device Not Available",
    "#DF Double Fault",
    "Coprocessor Segment Overrun",
    "#TS Invalid TSS",
    "#NP Segment Not Present",
    "#SS Stack-Segment Fault",
    "#GP General Protection",
    "#PF Page Fault",
    "Reserved",
    "#MF x87 FPU",
    "#AC Alignment Check",
    "#MC Machine Check",
    "#XM SIMD Floating Point",
    "#VE Virtualization",
    "Reserved",
    "Reserved", 
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "#SX Security",
    "Reserved"
};

// ISR handler
void isr_handler(u8 int_no) {
    if (int_no < 32) {
        console_print("Exception: ");
        console_print(exception_names[int_no]);
        console_print(" (");
        console_print_hex(int_no);
        console_print(")\n");
        
        // For now, halt on exception
        if (int_no >= 8) {  // Double fault and above are fatal
            console_print("FATAL EXCEPTION - System Halted\n");
            asm volatile("hlt");
            for(;;) { asm volatile("hlt"); }
        }
    } else {
        console_print("Unexpected interrupt: ");
        console_print_hex(int_no);
        console_print("\n");
    }
}

// IRQ handler
void irq_handler(u8 int_no) {
    // Send End of Interrupt (EOI) to PIC
    if (int_no >= 8) {
        // Send to slave PIC
        pic->pic_slave_command = 0x20;
    }
    // Send to master PIC
    pic->pic_master_command = 0x20;
    
    // Handle specific interrupts
    switch (int_no) {
        case 32:  // Timer
            timer_interrupt();
            break;
        case 33:  // Keyboard
            keyboard_interrupt();
            break;
        case 34:  // Cascade
            break;
        case 35:  // COM2
            break;
        case 36:  // COM1
            break;
        case 37:  // LPT2
            break;
        case 38:  // Floppy
            break;
        case 39:  // LPT1
            break;
        case 40:  // CMOS
            break;
        case 41:  // Free
            break;
        case 42:  // Free
            break;
        case 43:  // Free
            break;
        case 44:  // PS2 Mouse
            break;
        case 45:  // FPU
            break;
        case 46:  // Primary ATA
            break;
        case 47:  // Secondary ATA
            break;
        default:
            console_print("Unhandled IRQ: ");
            console_print_hex(int_no);
            console_print("\n");
            break;
    }
}

// Timer interrupt handler
void timer_interrupt(void) {
    // Update system time and schedule next task
    scheduler_tick();
}

// Keyboard interrupt handler  
void keyboard_interrupt(void) {
    // Read keyboard scancode and process
    u8 scancode = 0x60;
    console_print("Key: ");
    console_print_hex(scancode);
    console_print("\n");
}

// Initialize PIC
void pic_init(void) {
    // ICW1: Initialize PIC
    pic->pic_master_command = 0x11;
    pic->pic_slave_command = 0x11;
    
    // ICW2: Set IRQ base addresses
    pic->pic_masterdata = 0x20;  // IRQ 0-7 at 0x20-0x27
    pic->pic_slavedata = 0x28;   // IRQ 8-15 at 0x28-0x2F
    
    // ICW3: Configure cascading
    pic->pic_masterdata = 0x04;  // IRQ2 connected to slave
    pic->pic_slavedata = 0x02;   // Connected to master IRQ2
    
    // ICW4: Set 8086 mode
    pic->pic_masterdata = 0x01;
    pic->pic_slavedata = 0x01;
    
    // Disable all IRQs except timer and keyboard
    pic->pic_masterdata = 0xFC;  // Enable IRQ0 (timer) and IRQ1 (keyboard)
    pic->pic_slavedata = 0xFF;   // Disable all slave IRQs
}