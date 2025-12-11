#![no_std]

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("devine-perf-cpp/include/crypto.hpp");
        include!("devine-perf-cpp/include/profiler.hpp");
        
        fn fast_hash(data: &[u8]) -> u64;
        fn fast_memcpy(dest: *mut u8, src: *const u8, len: usize);
        
        fn profiler_rdtsc() -> u64;
        fn profiler_start_timer(name: *const u8);
        fn profiler_end_timer(name: *const u8);
        fn profiler_increment_counter(name: *const u8);
        fn profiler_get_counter(name: *const u8) -> u64;
        fn profiler_reset();
        fn profiler_dump();
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

pub fn rdtsc() -> u64 {
    unsafe { ffi::profiler_rdtsc() }
}

pub fn start_timer(name: &[u8]) {
    unsafe { ffi::profiler_start_timer(name.as_ptr()) }
}

pub fn end_timer(name: &[u8]) {
    unsafe { ffi::profiler_end_timer(name.as_ptr()) }
}

pub fn increment_counter(name: &[u8]) {
    unsafe { ffi::profiler_increment_counter(name.as_ptr()) }
}

pub fn get_counter(name: &[u8]) -> u64 {
    unsafe { ffi::profiler_get_counter(name.as_ptr()) }
}

pub fn reset_profiler() {
    unsafe { ffi::profiler_reset() }
}

pub fn dump_profiler() {
    unsafe { ffi::profiler_dump() }
}
