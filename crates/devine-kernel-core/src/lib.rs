#![no_std]
#![feature(abi_x86_interrupt)]
#![feature(const_mut_refs)]

extern crate alloc;

pub mod console;
pub mod memory;
pub mod panic;

use devine_arch::ArchOps;

pub fn kernel_main<A: ArchOps>() -> ! {
    A::init();
    
    console::println!("Devine Kernel v{}", env!("CARGO_PKG_VERSION"));
    console::println!("Architecture: {}", A::name());
    
    console::println!("Kernel initialized successfully!");
    
    A::halt_loop();
}
