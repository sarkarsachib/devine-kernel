#![no_std]

pub mod multiboot2;

#[derive(Debug, Clone, Copy)]
pub struct BootInfo {
    pub memory_map_addr: u64,
    pub memory_map_len: u64,
    pub framebuffer_addr: u64,
    pub framebuffer_width: u32,
    pub framebuffer_height: u32,
}

impl BootInfo {
    pub const fn new() -> Self {
        Self {
            memory_map_addr: 0,
            memory_map_len: 0,
            framebuffer_addr: 0,
            framebuffer_width: 0,
            framebuffer_height: 0,
        }
    }
}
