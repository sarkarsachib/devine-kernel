#![no_std]
#![no_main]

use core::panic::PanicInfo;

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    loop {}
}

#[no_mangle]
pub extern "C" fn _start() -> ! {
    test_block_driver();
    test_network_driver();
    test_tty_driver();
    
    loop {}
}

fn test_block_driver() {
    const BLOCK_SIZE: usize = 512;
    const NUM_BLOCKS: u64 = 1000;
    
    let mut buffer = [0u8; BLOCK_SIZE];
    
    for block in 0..NUM_BLOCKS {
        for i in 0..BLOCK_SIZE {
            buffer[i] = ((block + i as u64) & 0xFF) as u8;
        }
    }
    
    for block in 0..NUM_BLOCKS {
        for i in 0..BLOCK_SIZE {
            if buffer[i] != ((block + i as u64) & 0xFF) as u8 {
                return;
            }
        }
    }
}

fn test_network_driver() {
    const PACKET_SIZE: usize = 1500;
    const NUM_PACKETS: usize = 100;
    
    let mut packet = [0u8; PACKET_SIZE];
    
    for i in 0..NUM_PACKETS {
        for j in 0..PACKET_SIZE {
            packet[j] = ((i + j) & 0xFF) as u8;
        }
    }
}

fn test_tty_driver() {
    const MESSAGE: &[u8] = b"Hello, TTY!\n";
    const NUM_ITERATIONS: usize = 100;
    
    for _ in 0..NUM_ITERATIONS {
        for &byte in MESSAGE {
            let _ = byte;
        }
    }
}
