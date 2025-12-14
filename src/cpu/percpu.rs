/// Per-CPU Data Structure and Management
/// 
/// Tracks per-CPU state including:
/// - CPU ID and APIC ID
/// - Kernel stack
/// - Scheduler run queue
/// - Interrupt statistics
/// - Cycle counters for profiling

use crate::lib::spinlock::Spinlock;
use core::mem::MaybeUninit;

#[derive(Debug, Clone, Copy)]
pub struct CpuInfo {
    pub cpu_id: u32,
    pub apic_id: u32,
    pub is_bsp: bool,
    pub online: bool,
}

pub struct PerCpuData {
    pub info: CpuInfo,
    pub kernel_stack: u64,
    pub stack_size: u64,
    pub interrupts_disabled: bool,
    
    // Statistics
    pub irq_count: u64,
    pub timer_ticks: u64,
    pub context_switches: u64,
    
    // Profiling
    pub cycle_counter: u64,
    pub last_sample_time: u64,
}

pub const MAX_CPUS: usize = 256;

pub struct CpuManager {
    cpus: [Option<Spinlock<PerCpuData>>; MAX_CPUS],
    count: usize,
    current_cpu: usize,
}

pub static mut CPU_MANAGER: Option<CpuManager> = None;

impl CpuManager {
    pub fn new() -> Self {
        // We need to use unsafe to initialize a large array
        // Create an uninitialized array and fill it
        const NONE_VALUE: Option<Spinlock<PerCpuData>> = None;
        let cpus = [NONE_VALUE; MAX_CPUS];
        
        CpuManager {
            cpus,
            count: 0,
            current_cpu: 0,
        }
    }

    pub fn register_cpu(&mut self, info: CpuInfo, kernel_stack: u64, stack_size: u64) -> u32 {
        if self.count >= MAX_CPUS {
            return u32::MAX; // Error: too many CPUs
        }

        let per_cpu = PerCpuData {
            info,
            kernel_stack,
            stack_size,
            interrupts_disabled: false,
            irq_count: 0,
            timer_ticks: 0,
            context_switches: 0,
            cycle_counter: 0,
            last_sample_time: 0,
        };

        self.cpus[self.count] = Some(Spinlock::new(per_cpu));
        let id = self.count as u32;
        self.count += 1;
        
        id
    }

    pub fn get_cpu(&self, id: u32) -> Option<&Spinlock<PerCpuData>> {
        if (id as usize) < self.count {
            self.cpus[id as usize].as_ref()
        } else {
            None
        }
    }

    pub fn cpu_count(&self) -> usize {
        self.count
    }

    pub fn set_current_cpu(&mut self, id: u32) {
        if (id as usize) < self.count {
            self.current_cpu = id as usize;
        }
    }

    pub fn current_cpu_id(&self) -> u32 {
        self.current_cpu as u32
    }
}

pub fn init_cpu_manager() {
    unsafe {
        CPU_MANAGER = Some(CpuManager::new());
    }
}

pub fn register_cpu(info: CpuInfo, kernel_stack: u64, stack_size: u64) -> u32 {
    unsafe {
        if let Some(ref mut mgr) = CPU_MANAGER {
            mgr.register_cpu(info, kernel_stack, stack_size)
        } else {
            u32::MAX
        }
    }
}

pub fn get_cpu_manager() -> Option<&'static CpuManager> {
    unsafe {
        CPU_MANAGER.as_ref()
    }
}

pub fn get_current_cpu_info() -> Option<CpuInfo> {
    unsafe {
        if let Some(ref mgr) = CPU_MANAGER {
            let cpu_id = mgr.current_cpu_id();
            if let Some(cpu) = mgr.get_cpu(cpu_id) {
                let data = cpu.lock();
                Some(data.info)
            } else {
                None
            }
        } else {
            None
        }
    }
}

pub fn get_current_cpu_id() -> u32 {
    unsafe {
        if let Some(ref mgr) = CPU_MANAGER {
            mgr.current_cpu_id()
        } else {
            0
        }
    }
}

pub fn increment_irq_count(cpu_id: u32) {
    unsafe {
        if let Some(ref mgr) = CPU_MANAGER {
            if let Some(cpu) = mgr.get_cpu(cpu_id) {
                let mut data = cpu.lock();
                data.irq_count += 1;
            }
        }
    }
}

pub fn increment_timer_ticks(cpu_id: u32) {
    unsafe {
        if let Some(ref mgr) = CPU_MANAGER {
            if let Some(cpu) = mgr.get_cpu(cpu_id) {
                let mut data = cpu.lock();
                data.timer_ticks += 1;
            }
        }
    }
}

pub fn increment_context_switches(cpu_id: u32) {
    unsafe {
        if let Some(ref mgr) = CPU_MANAGER {
            if let Some(cpu) = mgr.get_cpu(cpu_id) {
                let mut data = cpu.lock();
                data.context_switches += 1;
            }
        }
    }
}
