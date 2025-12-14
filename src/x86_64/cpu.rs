/// x86_64 CPU Enumeration and Management
/// 
/// Handles:
/// - CPUID instruction parsing
/// - ACPI MADT table parsing (for APIC ID mapping)
/// - AP processor startup
/// - LAPIC operations

use core::arch::asm;

/// CPUID results
#[derive(Debug, Clone, Copy)]
pub struct CpuidResult {
    pub eax: u32,
    pub ebx: u32,
    pub ecx: u32,
    pub edx: u32,
}

pub fn cpuid(leaf: u32, subleaf: u32) -> CpuidResult {
    let mut eax = leaf;
    let mut ecx = subleaf;
    let ebx: u32;
    let edx: u32;
    
    unsafe {
        asm!(
            "push rbx",
            "cpuid",
            "mov {0:e}, ebx",
            "pop rbx",
            out(reg) ebx,
            inlateout("eax") eax,
            inlateout("ecx") ecx,
            out("edx") edx,
            options(nomem, nostack, preserves_flags)
        );
    }
    CpuidResult { eax, ebx, ecx, edx }
}

/// Get CPU vendor string
pub fn get_vendor_string() -> [u8; 12] {
    let result = cpuid(0, 0);
    let mut vendor = [0u8; 12];
    
    // Store vendor string from EBX, EDX, ECX
    vendor[0..4].copy_from_slice(&result.ebx.to_le_bytes());
    vendor[4..8].copy_from_slice(&result.edx.to_le_bytes());
    vendor[8..12].copy_from_slice(&result.ecx.to_le_bytes());
    
    vendor
}

/// Get CPU features
pub fn get_features() -> (u32, u32) {
    let result = cpuid(1, 0);
    (result.ecx, result.edx)
}

/// Check if CPUID leaf exists
pub fn has_cpuid_leaf(leaf: u32) -> bool {
    let result = cpuid(0, 0);
    result.eax >= leaf
}

/// Read Local APIC ID from MSR
pub fn read_apic_id() -> u32 {
    let mut apic_id: u32;
    unsafe {
        asm!(
            "mov ecx, 0x1b",  // IA32_APIC_BASE MSR
            "rdmsr",
            out("eax") apic_id,
            options(nomem, nostack, preserves_flags)
        );
    }
    (apic_id >> 24) & 0xFF
}

/// Send IPI (Inter-Processor Interrupt)
pub fn send_ipi(dest_apic_id: u32, vector: u32) {
    let lapic_base = get_lapic_base();
    
    // ICR high register (destination)
    let icr_high = lapic_base as *mut u32;
    unsafe {
        *icr_high.offset(0x10) = dest_apic_id << 24;
    }
    
    // ICR low register (vector and command)
    let icr_low = lapic_base as *mut u32;
    unsafe {
        *icr_low.offset(0x08) = vector & 0xFF;
    }
}

/// Get LAPIC base address from MSR
pub fn get_lapic_base() -> u64 {
    let mut base: u64;
    unsafe {
        asm!(
            "mov ecx, 0x1b",  // IA32_APIC_BASE MSR
            "rdmsr",
            out("eax") base,
            options(nomem, nostack, preserves_flags)
        );
    }
    base & 0xFFFFF000
}

/// Write MSR
pub fn write_msr(msr: u32, value: u64) {
    let low = (value & 0xFFFFFFFF) as u32;
    let high = ((value >> 32) & 0xFFFFFFFF) as u32;
    unsafe {
        asm!(
            "mov ecx, {msr:e}",
            "mov eax, {low:e}",
            "mov edx, {high:e}",
            "wrmsr",
            msr = in(reg) msr,
            low = in(reg) low,
            high = in(reg) high,
            options(nomem, nostack, preserves_flags)
        );
    }
}

/// Read MSR
pub fn read_msr(msr: u32) -> u64 {
    let mut low: u32;
    let mut high: u32;
    unsafe {
        asm!(
            "mov ecx, {msr:e}",
            "rdmsr",
            msr = in(reg) msr,
            out("eax") low,
            out("edx") high,
            options(nomem, nostack, preserves_flags)
        );
    }
    ((high as u64) << 32) | (low as u64)
}

/// AP startup code address (will be set during initialization)
pub static mut AP_STARTUP_VECTOR: u64 = 0;

/// Maximum number of AP CPUs we can support
pub const MAX_APS: u32 = 255;

/// Enumerate CPUs via CPUID
pub fn enumerate_cpus() -> u32 {
    // Try to determine CPU count via CPUID leaf 0x0B
    if has_cpuid_leaf(0x0B) {
        let result = cpuid(0x0B, 0);
        // Bits 31:16 contain the number of logical processors
        if result.ebx > 0 {
            return result.ebx;
        }
    }
    
    // Fallback: try leaf 0x04 for cache info
    if has_cpuid_leaf(0x04) {
        let result = cpuid(0x04, 0);
        // Bits 31:26 contain the number of cores - 1
        if result.eax > 0 {
            return ((result.eax >> 26) & 0x3F) + 1;
        }
    }
    
    // Default to 1 if we can't determine
    1
}

/// Basic ACPI MADT parsing for CPU enumeration
/// This is simplified - full implementation would parse MADT properly
pub struct MadtEntry {
    pub apic_id: u32,
    pub processor_id: u32,
}

pub fn parse_madt(_madt_address: u64) -> [MadtEntry; 0] {
    // Placeholder - full MADT parsing would go here
    // For now, we'll rely on other methods
    []
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cpuid() {
        let vendor = get_vendor_string();
        // Should be non-zero for real hardware
        assert!(vendor[0] != 0 || vendor[1] != 0);
    }
}
