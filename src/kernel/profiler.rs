#![no_std]

use core::sync::atomic::{AtomicBool, Ordering};

static PROFILER_ENABLED: AtomicBool = AtomicBool::new(false);

pub fn init() {
    let env_enabled = option_env!("PROFILER_ENABLED");
    if env_enabled == Some("1") || env_enabled == Some("true") {
        PROFILER_ENABLED.store(true, Ordering::Relaxed);
    }
}

pub fn is_enabled() -> bool {
    PROFILER_ENABLED.load(Ordering::Relaxed)
}

pub fn enable() {
    PROFILER_ENABLED.store(true, Ordering::Relaxed);
}

pub fn disable() {
    PROFILER_ENABLED.store(false, Ordering::Relaxed);
}

#[macro_export]
macro_rules! profile_start {
    ($name:expr) => {
        #[cfg(feature = "profiling")]
        {
            if $crate::kernel::profiler::is_enabled() {
                devine_perf_cpp::start_timer($name);
            }
        }
    };
}

#[macro_export]
macro_rules! profile_end {
    ($name:expr) => {
        #[cfg(feature = "profiling")]
        {
            if $crate::kernel::profiler::is_enabled() {
                devine_perf_cpp::end_timer($name);
            }
        }
    };
}

#[macro_export]
macro_rules! profile_count {
    ($name:expr) => {
        #[cfg(feature = "profiling")]
        {
            if $crate::kernel::profiler::is_enabled() {
                devine_perf_cpp::increment_counter($name);
            }
        }
    };
}

#[inline(always)]
pub fn rdtsc() -> u64 {
    #[cfg(target_arch = "x86_64")]
    unsafe {
        let mut lo: u32;
        let mut hi: u32;
        core::arch::asm!(
            "rdtsc",
            out("eax") lo,
            out("edx") hi,
            options(nomem, nostack)
        );
        ((hi as u64) << 32) | (lo as u64)
    }
    
    #[cfg(target_arch = "aarch64")]
    unsafe {
        let mut val: u64;
        core::arch::asm!(
            "mrs {}, cntvct_el0",
            out(reg) val,
            options(nomem, nostack)
        );
        val
    }
    
    #[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
    0
}

pub struct Timer<'a> {
    name: &'a [u8],
    start: u64,
}

impl<'a> Timer<'a> {
    pub fn new(name: &'a [u8]) -> Self {
        let start = rdtsc();
        if is_enabled() {
            #[cfg(feature = "profiling")]
            devine_perf_cpp::start_timer(name);
        }
        Timer { name, start }
    }
    
    pub fn elapsed(&self) -> u64 {
        rdtsc() - self.start
    }
}

impl<'a> Drop for Timer<'a> {
    fn drop(&mut self) {
        if is_enabled() {
            #[cfg(feature = "profiling")]
            devine_perf_cpp::end_timer(self.name);
        }
    }
}
