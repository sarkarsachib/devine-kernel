use core::arch::asm;

pub fn init() {
    init_gdt();
    init_idt();
    init_pic();
    enable_interrupts();
}

pub fn init_gdt() {
    
}

pub fn init_idt() {
    
}

pub fn init_pic() {
    
}

pub fn enable_interrupts() {
    unsafe {
        asm!("sti", options(nomem, nostack));
    }
}

pub fn disable_interrupts() {
    unsafe {
        asm!("cli", options(nomem, nostack));
    }
}

pub fn halt() {
    unsafe {
        asm!("hlt", options(nomem, nostack));
    }
}

pub fn read_rsp() -> u64 {
    let rsp: u64;
    unsafe {
        asm!("mov {}, rsp", out(reg) rsp, options(nomem, nostack));
    }
    rsp
}

pub fn read_cr3() -> u64 {
    let cr3: u64;
    unsafe {
        asm!("mov {}, cr3", out(reg) cr3, options(nomem, nostack));
    }
    cr3
}

pub fn write_cr3(value: u64) {
    unsafe {
        asm!("mov cr3, {}", in(reg) value, options(nostack));
    }
}

pub fn io_wait() {
    unsafe {
        asm!("out 0x80, al", in("al") 0u8, options(nomem, nostack));
    }
}
