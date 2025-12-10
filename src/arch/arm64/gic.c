/* ARM64 Global Interrupt Controller (GIC) Support */

#include "../../include/types.h"

// GICv3 register definitions
#define GIC_DISTRIBUTOR_BASE 0x08000000
#define GIC_REDISTRIBUTOR_BASE 0x080A0000
#define GIC_CPU_INTERFACE_BASE 0x08010000

// Distributor registers
#define GICD_CTLR (GIC_DISTRIBUTOR_BASE + 0x000)
#define GICD_TYPER (GIC_DISTRIBUTOR_BASE + 0x004)
#define GICD_ISENABLERn (GIC_DISTRIBUTOR_BASE + 0x100)
#define GICD_ISPENDRn (GIC_DISTRIBUTOR_BASE + 0x200)
#define GICD_ICFGRn (GIC_DISTRIBUTOR_BASE + 0xC00)

// CPU Interface registers
#define GICC_CTLR (GIC_CPU_INTERFACE_BASE + 0x000)
#define GICC_PMR (GIC_CPU_INTERFACE_BASE + 0x004)
#define GICC_BPR (GIC_CPU_INTERFACE_BASE + 0x008)
#define GICC_IAR (GIC_CPU_INTERFACE_BASE + 0x00C)
#define GICC_EOIR (GIC_CPU_INTERFACE_BASE + 0x010)
#define GICC_RPR (GIC_CPU_INTERFACE_BASE + 0x014)

// Initialize GIC
void gic_init(void) {
    // Configure Distributor
    volatile u32* gicd_ctlr = (volatile u32*)GICD_CTLR;
    *gicd_ctlr = 0x0; // Disable distributor for configuration
    
    // Set SPI priorities (IRQ 32+)
    for (u32 i = 32; i < 1020; i += 4) {
        volatile u32* prio_reg = (volatile u32*)(GIC_DISTRIBUTOR_BASE + 0x400 + (i / 4) * 4);
        *prio_reg = 0x80808080; // Set priorities
    }
    
    // Enable SGIs (0-15) and PPIs (16-31) for this CPU
    volatile u32* enable_sgi = (volatile u32*)GICD_ISENABLERn;
    *enable_sgi = 0xFFFFFFFF; // Enable all SGIs and PPIs
    
    // Enable SPIs (32+)
    for (u32 i = 32; i < 160; i += 32) {
        volatile u32* enable_spi = (volatile u32*)(GIC_DISTRIBUTOR_BASE + 0x100 + (i / 32) * 4);
        *enable_spi = 0xFFFFFFFF; // Enable SPIs
    }
    
    // Enable distributor
    *gicd_ctlr = 0x1; // Enable distributor
    
    // Configure CPU Interface
    volatile u32* gicc_ctlr = (volatile u32*)GICC_CTLR;
    *gicc_ctlr = 0x0; // Disable CPU interface for configuration
    
    // Set priority mask (allow all priorities)
    volatile u32* gicc_pmr = (volatile u32*)GICC_PMR;
    *gicc_pmr = 0xFF; // Priority threshold
    
    // Set binary point register
    volatile u32* gicc_bpr = (volatile u32*)GICC_BPR;
    *gicc_bpr = 0x7; // No preemption
    
    // Enable CPU interface
    *gicc_ctlr = 0x1; // Enable CPU interface
}

// Send End of Interrupt
void gic_send_eoi(u32 interrupt_id) {
    volatile u32* gicc_eoir = (volatile u32*)GICC_EOIR;
    *gicc_eoir = interrupt_id;
}

// Read Interrupt Acknowledge Register
u32 gic_read_iar(void) {
    volatile u32* gicc_iar = (volatile u32*)GICC_IAR;
    return *gicc_iar;
}

// Check if interrupt is pending
bool gic_is_pending(u32 interrupt_id) {
    if (interrupt_id < 16) {
        // SGI - check SGIs pending register
        volatile u32* pend_reg = (volatile u32*)(GIC_DISTRIBUTOR_BASE + 0x200 + (interrupt_id / 32) * 4);
        return (*pend_reg >> (interrupt_id % 32)) & 1;
    } else if (interrupt_id < 32) {
        // PPI - check PPIs pending register
        volatile u32* pend_reg = (volatile u32*)(GIC_DISTRIBUTOR_BASE + 0x200 + (interrupt_id / 32) * 4);
        return (*pend_reg >> (interrupt_id % 32)) & 1;
    } else {
        // SPI - check SPIs pending register
        volatile u32* pend_reg = (volatile u32*)(GIC_DISTRIBUTOR_BASE + 0x200 + (interrupt_id / 32) * 4);
        return (*pend_reg >> (interrupt_id % 32)) & 1;
    }
}

// Enable interrupt
void gic_enable_interrupt(u32 interrupt_id) {
    u32 reg_offset = interrupt_id / 32;
    u32 bit_offset = interrupt_id % 32;
    
    volatile u32* enable_reg = (volatile u32*)(GIC_DISTRIBUTOR_BASE + 0x100 + reg_offset * 4);
    *enable_reg = (1 << bit_offset);
}

// Disable interrupt
void gic_disable_interrupt(u32 interrupt_id) {
    u32 reg_offset = interrupt_id / 32;
    u32 bit_offset = interrupt_id % 32;
    
    volatile u32* disable_reg = (volatile u32*)(GIC_DISTRIBUTOR_BASE + 0x180 + reg_offset * 4);
    *disable_reg = (1 << bit_offset);
}