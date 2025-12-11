pub const MULTIBOOT2_MAGIC: u32 = 0x36d76289;

#[repr(C, align(8))]
pub struct Multiboot2Header {
    pub magic: u32,
    pub architecture: u32,
    pub header_length: u32,
    pub checksum: u32,
}

impl Multiboot2Header {
    pub const fn new() -> Self {
        const MAGIC: u32 = 0xe85250d6;
        const ARCH: u32 = 0;
        const LENGTH: u32 = core::mem::size_of::<Multiboot2Header>() as u32;
        
        Self {
            magic: MAGIC,
            architecture: ARCH,
            header_length: LENGTH,
            checksum: 0u32.wrapping_sub(MAGIC).wrapping_sub(ARCH).wrapping_sub(LENGTH),
        }
    }
}
