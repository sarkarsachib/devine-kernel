/// Hardware information discovered by the bootloader
#[derive(Debug, Clone, Copy)]
pub struct MemoryRegion {
    pub base: u64,
    pub size: u64,
    pub region_type: u32,
}

#[derive(Debug, Clone, Copy)]
pub struct CpuInfo {
    pub vendor: [u8; 12],
    pub family: u32,
    pub model: u32,
    pub stepping: u32,
}

#[derive(Debug, Clone, Copy)]
pub struct HardwareInfo {
    pub memory_regions: &'static [MemoryRegion],
    pub memory_region_count: usize,
    pub cpu_info: CpuInfo,
    pub framebuffer_addr: u64,
    pub framebuffer_width: u32,
    pub framebuffer_height: u32,
    pub framebuffer_pitch: u32,
    pub framebuffer_format: u32,
}

impl HardwareInfo {
    pub fn new() -> Self {
        HardwareInfo {
            memory_regions: &[],
            memory_region_count: 0,
            cpu_info: CpuInfo {
                vendor: *b"            ",
                family: 0,
                model: 0,
                stepping: 0,
            },
            framebuffer_addr: 0,
            framebuffer_width: 0,
            framebuffer_height: 0,
            framebuffer_pitch: 0,
            framebuffer_format: 0,
        }
    }
}
