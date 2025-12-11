#![no_std]

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("devine-perf-cpp/include/crypto.hpp");
        
        fn fast_hash(data: &[u8]) -> u64;
        fn fast_memcpy(dest: *mut u8, src: *const u8, len: usize);
    }
}

pub fn init_cpp_modules() {
}

pub fn hash_data(data: &[u8]) -> u64 {
    ffi::fast_hash(data)
}

pub unsafe fn fast_copy(dest: *mut u8, src: *const u8, len: usize) {
    ffi::fast_memcpy(dest, src, len);
}
