#![no_std]
#![no_main]

extern crate core;

mod hwinfo;
mod x86_64;
mod arm64;

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {
        hlt_loop();
    }
}

#[inline]
fn hlt_loop() -> ! {
    loop {
        #[cfg(target_arch = "x86_64")]
        unsafe {
            core::arch::asm!("hlt");
        }
        #[cfg(target_arch = "aarch64")]
        unsafe {
            core::arch::asm!("wfi");
        }
    }
}

/// Entry point for the kernel, called from assembly stubs
/// This function should never return.
#[no_mangle]
pub extern "C" fn kmain(hw_info: *const hwinfo::HardwareInfo) -> ! {
    let _hw_info = if hw_info.is_null() {
        hwinfo::HardwareInfo::new()
    } else {
        unsafe { *hw_info }
    };

    // Initialize the system
    // TODO: Initialize drivers, filesystem, etc.

    hlt_loop()
}
