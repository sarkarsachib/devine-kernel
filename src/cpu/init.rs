/// CPU Initialization and SMP Setup
/// 
/// This module handles:
/// - CPU enumeration and detection
/// - Bringing up application processors
/// - Per-CPU state initialization
/// - Load balancing setup

use crate::cpu::percpu::{self, CpuInfo};
use crate::cpu::scheduler_smp;

#[cfg(target_arch = "x86_64")]
use crate::x86_64::cpu as x86_cpu;

#[cfg(target_arch = "aarch64")]
use crate::arch::arm64::cpu as arm_cpu;

/// Initialize the CPU subsystem
pub fn init() {
    // Initialize the CPU manager
    percpu::init_cpu_manager();
    
    // Initialize the scheduler
    scheduler_smp::init_scheduler();
    
    // Enumerate CPUs
    #[cfg(target_arch = "x86_64")]
    enumerate_cpus_x86_64();
    
    #[cfg(target_arch = "aarch64")]
    enumerate_cpus_arm64();
}

/// Boot all application processors
pub fn boot_aps() {
    #[cfg(target_arch = "x86_64")]
    boot_aps_x86_64();
    
    #[cfg(target_arch = "aarch64")]
    boot_aps_arm64();
}

#[cfg(target_arch = "x86_64")]
fn enumerate_cpus_x86_64() {
    // Get CPU count from CPUID
    let cpu_count = x86_cpu::enumerate_cpus();
    
    // Register BSP (Boot Strap Processor)
    let bsp_info = CpuInfo {
        cpu_id: 0,
        apic_id: x86_cpu::read_apic_id(),
        is_bsp: true,
        online: true,
    };
    
    // Allocate stack for BSP (already has one, but register it)
    let bsp_stack = 0xFFFF800000000000u64;  // Kernel stack base (should be from frame allocator)
    let stack_size = 0x4000u64;  // 16KB stack
    
    percpu::register_cpu(bsp_info, bsp_stack, stack_size);
    
    // For now, just register a few APs
    for i in 1..core::cmp::min(cpu_count, 16) {
        let ap_info = CpuInfo {
            cpu_id: i as u32,
            apic_id: i as u32,  // TODO: Get from ACPI MADT
            is_bsp: false,
            online: false,
        };
        
        // Allocate stack for AP
        let ap_stack = 0xFFFF800000000000u64 - (i as u64 * 0x10000u64);
        let stack_size = 0x4000u64;
        
        percpu::register_cpu(ap_info, ap_stack, stack_size);
    }
}

#[cfg(target_arch = "x86_64")]
fn boot_aps_x86_64() {
    use crate::x86_64::ap_boot;
    
    // Get AP startup code
    let entry_point = ap_boot::AP_BOOT_ADDRESS;
    
    // Boot all APs
    // This is simplified - full implementation would handle multiple APs
    for cpu_id in 1..4 {
        let _ = ap_boot::boot_ap(cpu_id, entry_point);
    }
}

#[cfg(target_arch = "aarch64")]
fn enumerate_cpus_arm64() {
    // Get CPU count from device tree or PSCI
    let cpu_count = arm_cpu::get_cpu_count();
    
    // Register BSP
    let bsp_info = CpuInfo {
        cpu_id: 0,
        apic_id: 0,  // ARM64 doesn't have APIC, use GIC redistributor ID
        is_bsp: true,
        online: true,
    };
    
    let bsp_stack = 0xFFFF800000000000u64;
    let stack_size = 0x4000u64;
    
    percpu::register_cpu(bsp_info, bsp_stack, stack_size);
    
    // Register APs
    for i in 1..cpu_count {
        let ap_info = CpuInfo {
            cpu_id: i as u32,
            apic_id: i as u32,
            is_bsp: false,
            online: false,
        };
        
        let ap_stack = 0xFFFF800000000000u64 - (i as u64 * 0x10000u64);
        let stack_size = 0x4000u64;
        
        percpu::register_cpu(ap_info, ap_stack, stack_size);
    }
}

#[cfg(target_arch = "aarch64")]
fn boot_aps_arm64() {
    use crate::arm64::ap_boot;
    
    // Use PSCI to bring up APs
    let entry_point = 0xFFFF800000000000u64;  // Kernel entry point
    
    let _booted = ap_boot::boot_all_aps(entry_point);
}

/// Get the number of online CPUs
pub fn cpu_count() -> u32 {
    unsafe {
        if let Some(ref mgr) = crate::cpu::percpu::CPU_MANAGER {
            mgr.cpu_count() as u32
        } else {
            1
        }
    }
}

/// Get current CPU ID
pub fn current_cpu() -> u32 {
    percpu::get_current_cpu_id()
}
