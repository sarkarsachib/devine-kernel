use crate::process::scheduler;

const PIT_FREQUENCY: u32 = 1193182;
const TIMER_HZ: u32 = 100;

pub fn init_timer() {
    let divisor = PIT_FREQUENCY / TIMER_HZ;
    
    unsafe {
        outb(0x43, 0x36);
        outb(0x40, (divisor & 0xFF) as u8);
        outb(0x40, ((divisor >> 8) & 0xFF) as u8);
    }
}

pub extern "C" fn timer_interrupt_handler() {
    scheduler::tick();
    
    unsafe {
        outb(0x20, 0x20);
    }
    
    if let Some(_next_tid) = scheduler::schedule() {
        
    }
}

unsafe fn outb(port: u16, value: u8) {
    core::arch::asm!(
        "out dx, al",
        in("dx") port,
        in("al") value,
        options(nomem, nostack)
    );
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct InterruptStackFrame {
    pub instruction_pointer: u64,
    pub code_segment: u64,
    pub cpu_flags: u64,
    pub stack_pointer: u64,
    pub stack_segment: u64,
}

pub type InterruptHandler = extern "C" fn(&mut InterruptStackFrame);

#[repr(C)]
#[derive(Clone, Copy)]
pub struct IdtEntry {
    offset_low: u16,
    selector: u16,
    ist: u8,
    type_attr: u8,
    offset_mid: u16,
    offset_high: u32,
    reserved: u32,
}

impl IdtEntry {
    pub const fn missing() -> Self {
        IdtEntry {
            offset_low: 0,
            selector: 0,
            ist: 0,
            type_attr: 0,
            offset_mid: 0,
            offset_high: 0,
            reserved: 0,
        }
    }

    pub fn set_handler(&mut self, handler: u64) {
        self.offset_low = handler as u16;
        self.offset_mid = (handler >> 16) as u16;
        self.offset_high = (handler >> 32) as u32;
        self.selector = 0x08;
        self.type_attr = 0x8E;
        self.ist = 0;
        self.reserved = 0;
    }
}

#[repr(C, packed)]
pub struct IdtDescriptor {
    limit: u16,
    base: u64,
}

pub static mut IDT: [IdtEntry; 256] = [IdtEntry::missing(); 256];

pub fn init_idt() {
    unsafe {
        IDT[32].set_handler(timer_interrupt_wrapper as u64);
        
        let idt_descriptor = IdtDescriptor {
            limit: (core::mem::size_of::<[IdtEntry; 256]>() - 1) as u16,
            base: IDT.as_ptr() as u64,
        };
        
        core::arch::asm!(
            "lidt [{}]",
            in(reg) &idt_descriptor,
            options(readonly, nostack)
        );
    }
}

extern "C" fn timer_interrupt_wrapper() {
    timer_interrupt_handler();
}
