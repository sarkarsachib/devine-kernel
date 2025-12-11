use super::{PhysAddr, MemoryRegion, PAGE_SIZE};
use spin::Mutex;

#[cfg(not(test))]
use alloc::vec::Vec;

#[cfg(test)]
extern crate std;
#[cfg(test)]
use std::vec::Vec;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Frame {
    pub start_address: PhysAddr,
}

impl Frame {
    pub fn containing_address(address: PhysAddr) -> Frame {
        Frame {
            start_address: PhysAddr(address.0 & !((PAGE_SIZE as u64) - 1)),
        }
    }

    pub fn range_inclusive(start: Frame, end: Frame) -> FrameIter {
        FrameIter { start, end }
    }
}

pub struct FrameIter {
    start: Frame,
    end: Frame,
}

impl Iterator for FrameIter {
    type Item = Frame;

    fn next(&mut self) -> Option<Frame> {
        if self.start.start_address <= self.end.start_address {
            let frame = self.start;
            self.start.start_address.0 += PAGE_SIZE as u64;
            Some(frame)
        } else {
            None
        }
    }
}

pub trait FrameAllocator {
    fn allocate_frame(&mut self) -> Option<Frame>;
    fn deallocate_frame(&mut self, frame: Frame);
}

pub struct BitmapFrameAllocator {
    memory_regions: Vec<MemoryRegion>,
    bitmap: Vec<u64>,
    next_free: usize,
}

impl BitmapFrameAllocator {
    pub fn new() -> Self {
        Self {
            memory_regions: Vec::new(),
            bitmap: Vec::new(),
            next_free: 0,
        }
    }

    pub fn init(&mut self, regions: &[MemoryRegion]) {
        self.memory_regions.clear();
        for region in regions {
            self.memory_regions.push(*region);
        }

        let total_frames = regions.iter().map(|r| r.size / PAGE_SIZE).sum::<usize>();
        let bitmap_size = (total_frames + 63) / 64;
        
        self.bitmap.clear();
        self.bitmap.resize(bitmap_size, !0u64);
        self.next_free = 0;
    }

    fn frame_to_bit_index(&self, frame: Frame) -> Option<usize> {
        let mut offset = 0;
        for region in &self.memory_regions {
            if frame.start_address >= region.start && frame.start_address < region.end() {
                let region_offset = (frame.start_address.0 - region.start.0) as usize / PAGE_SIZE;
                return Some(offset + region_offset);
            }
            offset += region.size / PAGE_SIZE;
        }
        None
    }

    fn bit_index_to_frame(&self, index: usize) -> Option<Frame> {
        let mut offset = 0;
        for region in &self.memory_regions {
            let region_frames = region.size / PAGE_SIZE;
            if index < offset + region_frames {
                let region_offset = index - offset;
                let addr = region.start.0 + (region_offset * PAGE_SIZE) as u64;
                return Some(Frame {
                    start_address: PhysAddr(addr),
                });
            }
            offset += region_frames;
        }
        None
    }
}

impl FrameAllocator for BitmapFrameAllocator {
    fn allocate_frame(&mut self) -> Option<Frame> {
        let total_bits = self.bitmap.len() * 64;
        
        for i in 0..total_bits {
            let idx = (self.next_free + i) % total_bits;
            let word_idx = idx / 64;
            let bit_idx = idx % 64;
            
            if self.bitmap[word_idx] & (1u64 << bit_idx) != 0 {
                self.bitmap[word_idx] &= !(1u64 << bit_idx);
                self.next_free = (idx + 1) % total_bits;
                return self.bit_index_to_frame(idx);
            }
        }
        
        None
    }

    fn deallocate_frame(&mut self, frame: Frame) {
        if let Some(idx) = self.frame_to_bit_index(frame) {
            let word_idx = idx / 64;
            let bit_idx = idx % 64;
            self.bitmap[word_idx] |= 1u64 << bit_idx;
        }
    }
}

pub static FRAME_ALLOCATOR: Mutex<Option<BitmapFrameAllocator>> = Mutex::new(None);

pub fn init_frame_allocator(regions: &[MemoryRegion]) {
    let mut allocator = BitmapFrameAllocator::new();
    allocator.init(regions);
    *FRAME_ALLOCATOR.lock() = Some(allocator);
}

pub fn allocate_frame() -> Option<Frame> {
    FRAME_ALLOCATOR.lock().as_mut()?.allocate_frame()
}

pub fn deallocate_frame(frame: Frame) {
    if let Some(ref mut allocator) = *FRAME_ALLOCATOR.lock() {
        allocator.deallocate_frame(frame);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_frame_allocation() {
        let regions = [MemoryRegion::new(PhysAddr::new(0x100000), 0x100000)];
        let mut allocator = BitmapFrameAllocator::new();
        allocator.init(&regions);

        let frame1 = allocator.allocate_frame();
        assert!(frame1.is_some());

        let frame2 = allocator.allocate_frame();
        assert!(frame2.is_some());
        assert_ne!(frame1, frame2);

        allocator.deallocate_frame(frame1.unwrap());
        let frame3 = allocator.allocate_frame();
        assert!(frame3.is_some());
    }
}
