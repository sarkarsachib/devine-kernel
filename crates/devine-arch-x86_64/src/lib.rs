#![no_std]
#![feature(abi_x86_interrupt)]

use devine_arch::ArchOps;

pub mod gdt;
pub mod idt;
pub mod pic;
pub mod serial;
pub mod vga;

pub struct X86_64;

impl ArchOps for X86_64 {
    fn name() -> &'static str {
        "x86_64"
    }

    fn init() {
        gdt::init();
        idt::init();
        pic::init();
        unsafe {
            Self::enable_interrupts();
        }
    }

    fn halt_loop() -> ! {
        loop {
            unsafe {
                core::arch::asm!("hlt");
            }
        }
    }

    fn enable_interrupts() {
        unsafe {
            core::arch::asm!("sti");
        }
    }

    fn disable_interrupts() {
        unsafe {
            core::arch::asm!("cli");
        }
    }

    fn wait_for_interrupt() {
        unsafe {
            core::arch::asm!("hlt");
        }
    }
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    X86_64::init();
    devine_perf_cpp::init_cpp_modules();
    loop {
        X86_64::wait_for_interrupt();
    }
}
