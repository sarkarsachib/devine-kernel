pub mod x86_64;
pub mod arm64;
pub mod interrupts;
pub mod ipi;

#[cfg(target_arch = "x86_64")]
pub use x86_64::*;

pub fn init() {
    #[cfg(target_arch = "x86_64")]
    x86_64::init();
}
