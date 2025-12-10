use core::alloc::{GlobalAlloc, Layout};
use core::ptr::NonNull;
use spin::Mutex;

const HEAP_START: usize = 0x_4444_4444_0000;
const HEAP_SIZE: usize = 100 * 1024;

#[repr(C)]
struct FreeBlock {
    size: usize,
    next: Option<NonNull<FreeBlock>>,
}

pub struct LinkedListAllocator {
    head: Option<NonNull<FreeBlock>>,
}

unsafe impl Send for LinkedListAllocator {}
unsafe impl Sync for LinkedListAllocator {}

impl LinkedListAllocator {
    pub const fn new() -> Self {
        LinkedListAllocator { head: None }
    }

    pub unsafe fn init(&mut self, heap_start: usize, heap_size: usize) {
        let block = heap_start as *mut FreeBlock;
        (*block).size = heap_size;
        (*block).next = None;
        self.head = NonNull::new(block);
    }

    fn alloc_from_region(&mut self, layout: Layout) -> Option<NonNull<u8>> {
        let mut current = self.head?;
        let mut prev: Option<NonNull<FreeBlock>> = None;

        loop {
            let block = unsafe { current.as_ref() };
            let align = layout.align();
            let size = layout.size();

            let block_start = current.as_ptr() as usize;
            let aligned_start = (block_start + align - 1) & !(align - 1);
            let padding = aligned_start - block_start;

            if block.size >= size + padding {
                let new_size = block.size - size - padding;
                
                if new_size > core::mem::size_of::<FreeBlock>() {
                    let next_block_addr = aligned_start + size;
                    let next_block = unsafe { &mut *(next_block_addr as *mut FreeBlock) };
                    next_block.size = new_size;
                    next_block.next = block.next;

                    if let Some(mut prev_block) = prev {
                        unsafe { prev_block.as_mut().next = NonNull::new(next_block); }
                    } else {
                        self.head = NonNull::new(next_block);
                    }
                } else {
                    if let Some(mut prev_block) = prev {
                        unsafe { prev_block.as_mut().next = block.next; }
                    } else {
                        self.head = block.next;
                    }
                }

                return NonNull::new(aligned_start as *mut u8);
            }

            prev = Some(current);
            current = block.next?;
        }
    }

    unsafe fn dealloc_to_region(&mut self, ptr: *mut u8, layout: Layout) {
        let block = ptr as *mut FreeBlock;
        (*block).size = layout.size();

        if let Some(head) = self.head {
            if block < head.as_ptr() {
                (*block).next = Some(head);
                self.head = NonNull::new(block);
            } else {
                let mut current = head;
                loop {
                    let current_block = current.as_mut();
                    if let Some(next) = current_block.next {
                        if block < next.as_ptr() {
                            (*block).next = Some(next);
                            current_block.next = NonNull::new(block);
                            break;
                        }
                        current = next;
                    } else {
                        (*block).next = None;
                        current_block.next = NonNull::new(block);
                        break;
                    }
                }
            }
        } else {
            (*block).next = None;
            self.head = NonNull::new(block);
        }

        self.merge_adjacent_blocks();
    }

    fn merge_adjacent_blocks(&mut self) {
        if let Some(mut current) = self.head {
            unsafe {
                while let Some(next) = current.as_ref().next {
                    let current_end = current.as_ptr() as usize + current.as_ref().size;
                    let next_start = next.as_ptr() as usize;

                    if current_end == next_start {
                        current.as_mut().size += next.as_ref().size;
                        current.as_mut().next = next.as_ref().next;
                    } else {
                        current = next;
                    }
                }
            }
        }
    }
}

unsafe impl GlobalAlloc for LockedHeap {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let mut allocator = self.0.lock();
        allocator
            .alloc_from_region(layout)
            .map_or(core::ptr::null_mut(), |p| p.as_ptr())
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        let mut allocator = self.0.lock();
        allocator.dealloc_to_region(ptr, layout);
    }
}

pub struct LockedHeap(Mutex<LinkedListAllocator>);

impl LockedHeap {
    pub const fn new() -> Self {
        LockedHeap(Mutex::new(LinkedListAllocator::new()))
    }

    pub unsafe fn init(&self, heap_start: usize, heap_size: usize) {
        self.0.lock().init(heap_start, heap_size);
    }
}

#[cfg(not(test))]
#[global_allocator]
static ALLOCATOR: LockedHeap = LockedHeap::new();

#[cfg(test)]
static ALLOCATOR: LockedHeap = LockedHeap::new();

pub fn init_heap() {
    unsafe {
        ALLOCATOR.init(HEAP_START, HEAP_SIZE);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    extern crate std;
    use std::vec::Vec;

    #[test]
    fn test_heap_allocation() {
        let mut v = Vec::new();
        v.push(1);
        v.push(2);
        v.push(3);
        assert_eq!(v.len(), 3);
    }
}
