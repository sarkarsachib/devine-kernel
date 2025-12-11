#![no_std]

use devine_arch::ArchOps;

pub mod gic;
pub mod mmu;
pub mod uart;

pub struct AArch64;

impl ArchOps for AArch64 {
    fn name() -> &'static str {
        "aarch64"
    }

    fn init() {
        uart::init();
        gic::init();
        unsafe {
            Self::enable_interrupts();
        }
    }

    fn halt_loop() -> ! {
        loop {
            unsafe {
                core::arch::asm!("wfi");
            }
        }
    }

    fn enable_interrupts() {
        unsafe {
            core::arch::asm!("msr daifclr, #2");
        }
    }

    fn disable_interrupts() {
        unsafe {
            core::arch::asm!("msr daifset, #2");
        }
    }

    fn wait_for_interrupt() {
        unsafe {
            core::arch::asm!("wfi");
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn _start() -> ! {
    AArch64::init();
    devine_perf_cpp::init_cpp_modules();
    loop {
        AArch64::wait_for_interrupt();
    }
}
