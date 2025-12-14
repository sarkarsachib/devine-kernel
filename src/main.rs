#![no_std]
#![no_main]

extern crate core;
extern crate kernel;

use core::panic::PanicInfo;
use kernel::hwinfo;

// Panic handler is in kernel library

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
    kernel::drivers::serial::SERIAL1.lock().init();

    let msg = b"Kernel: Serial Initialized.\n";
    {
        let mut serial = kernel::drivers::serial::SERIAL1.lock();
        for &b in msg {
            serial.send(b);
        }
    }

    // TODO: Initialize drivers, filesystem, etc.

    hlt_loop()
}
