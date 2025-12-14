#![cfg_attr(not(test), no_std)]

#[cfg(not(test))]
extern crate alloc;

pub mod memory;
pub mod process;
pub mod arch;
pub mod syscall;
pub mod security;
pub mod userspace;
pub mod hwinfo;
pub mod drivers;
#[cfg(target_arch = "x86_64")]
pub mod x86_64;
#[cfg(target_arch = "aarch64")]
pub mod arm64;

pub mod lib {
    pub mod spinlock {
        /// Spinlock Implementation for Kernel
        /// Used for synchronizing access to shared resources across CPUs

        use core::sync::atomic::{AtomicBool, Ordering};
        use core::cell::UnsafeCell;
        use core::ops::{Deref, DerefMut};
        use core::fmt;

        pub struct Spinlock<T> {
            locked: AtomicBool,
            data: UnsafeCell<T>,
        }

        pub struct SpinlockGuard<'a, T> {
            lock: &'a Spinlock<T>,
        }

        impl<T> Spinlock<T> {
            pub const fn new(data: T) -> Self {
                Spinlock {
                    locked: AtomicBool::new(false),
                    data: UnsafeCell::new(data),
                }
            }

            pub fn lock(&self) -> SpinlockGuard<T> {
                loop {
                    // Try to acquire the lock
                    match self.locked.compare_exchange(
                        false,
                        true,
                        Ordering::Acquire,
                        Ordering::Relaxed,
                    ) {
                        Ok(_) => break,
                        Err(_) => {
                            // Spin while locked
                            #[cfg(target_arch = "x86_64")]
                            unsafe {
                                core::arch::asm!("pause", options(nomem, nostack));
                            }
                            #[cfg(target_arch = "aarch64")]
                            unsafe {
                                core::arch::asm!("yield", options(nomem, nostack));
                            }
                        }
                    }
                }
                SpinlockGuard { lock: self }
            }

            pub fn try_lock(&self) -> Option<SpinlockGuard<T>> {
                match self.locked.compare_exchange(
                    false,
                    true,
                    Ordering::Acquire,
                    Ordering::Relaxed,
                ) {
                    Ok(_) => Some(SpinlockGuard { lock: self }),
                    Err(_) => None,
                }
            }

            pub fn get_mut(&mut self) -> &mut T {
                unsafe { &mut *self.data.get() }
            }
        }

        impl<T> Deref for SpinlockGuard<'_, T> {
            type Target = T;

            fn deref(&self) -> &T {
                unsafe { &*self.lock.data.get() }
            }
        }

        impl<T> DerefMut for SpinlockGuard<'_, T> {
            fn deref_mut(&mut self) -> &mut T {
                unsafe { &mut *self.lock.data.get() }
            }
        }

        impl<T> Drop for SpinlockGuard<'_, T> {
            fn drop(&mut self) {
                self.lock.locked.store(false, Ordering::Release);
            }
        }

        impl<T: fmt::Debug> fmt::Debug for Spinlock<T> {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                write!(f, "Spinlock {{ ... }}")
            }
        }

        unsafe impl<T: Send> Send for Spinlock<T> {}
        unsafe impl<T: Send> Sync for Spinlock<T> {}

        /// CPU-level interrupt control using atomic operations
        pub struct InterruptGuard;

        impl InterruptGuard {
            /// Disable interrupts (platform-specific)
            pub fn disable() -> Self {
                #[cfg(target_arch = "x86_64")]
                unsafe {
                    core::arch::asm!("cli", options(nomem, nostack));
                }
                #[cfg(target_arch = "aarch64")]
                unsafe {
                    core::arch::asm!("msr daifset, #2", options(nomem, nostack));
                }
                InterruptGuard
            }

            /// Check if interrupts are enabled
            pub fn are_enabled() -> bool {
                #[cfg(target_arch = "x86_64")]
                unsafe {
                    let flags: u64;
                    core::arch::asm!("pushfq; popq {}", out(reg) flags, options(nomem, nostack));
                    (flags & 0x200) != 0
                }
                #[cfg(target_arch = "aarch64")]
                unsafe {
                    let daif: u64;
                    core::arch::asm!("mrs {}, daif", out(reg) daif, options(nomem, nostack));
                    (daif & 0x80) == 0
                }
                #[cfg(not(any(target_arch = "x86_64", target_arch = "aarch64")))]
                true
            }
        }

        impl Drop for InterruptGuard {
            fn drop(&mut self) {
                #[cfg(target_arch = "x86_64")]
                unsafe {
                    core::arch::asm!("sti", options(nomem, nostack));
                }
                #[cfg(target_arch = "aarch64")]
                unsafe {
                    core::arch::asm!("msr daifclr, #2", options(nomem, nostack));
                }
            }
        }
    }
}

pub mod cpu;

#[cfg(not(test))]
use core::panic::PanicInfo;

#[cfg(not(test))]
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[cfg(test)]
mod tests {
    #[test]
    fn test_basic() {
        assert_eq!(2 + 2, 4);
    }
}
