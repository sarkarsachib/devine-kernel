/* PL011 UART Driver for ARM64 */

#include "../../include/types.h"
#include "../../include/console.h"

#define PL011_DR        0x00
#define PL011_RSR       0x04
#define PL011_FR        0x18
#define PL011_ILPR      0x20
#define PL011_IBRD      0x24
#define PL011_FBRD      0x28
#define PL011_LCRH      0x2C
#define PL011_CR        0x30
#define PL011_IFLS      0x34
#define PL011_IMSC      0x38
#define PL011_RIS       0x3C
#define PL011_MIS       0x40
#define PL011_ICR       0x44

#define PL011_FR_TXFF   (1 << 5)
#define PL011_FR_RXFE   (1 << 4)
#define PL011_FR_BUSY   (1 << 3)

#define PL011_CR_UARTEN (1 << 0)
#define PL011_CR_TXE    (1 << 8)
#define PL011_CR_RXE    (1 << 9)

#define PL011_LCRH_FEN  (1 << 4)
#define PL011_LCRH_WLEN_8 (3 << 5)

#define UART_BUFFER_SIZE 1024

typedef struct pl011_device {
    u64 base_addr;
    u8 rx_buffer[UART_BUFFER_SIZE];
    u32 rx_head;
    u32 rx_tail;
    u8 tx_buffer[UART_BUFFER_SIZE];
    u32 tx_head;
    u32 tx_tail;
    bool initialized;
} pl011_device_t;

static pl011_device_t* pl011_dev = NULL;

static inline void pl011_write32(u64 addr, u32 value) {
    *((volatile u32*)addr) = value;
}

static inline u32 pl011_read32(u64 addr) {
    return *((volatile u32*)addr);
}

static void pl011_write_reg(pl011_device_t* dev, u32 reg, u32 value) {
    pl011_write32(dev->base_addr + reg, value);
}

static u32 pl011_read_reg(pl011_device_t* dev, u32 reg) {
    return pl011_read32(dev->base_addr + reg);
}

static bool pl011_can_transmit(pl011_device_t* dev) {
    return (pl011_read_reg(dev, PL011_FR) & PL011_FR_TXFF) == 0;
}

static bool pl011_has_data(pl011_device_t* dev) {
    return (pl011_read_reg(dev, PL011_FR) & PL011_FR_RXFE) == 0;
}

static void pl011_write_byte(pl011_device_t* dev, u8 byte) {
    while (!pl011_can_transmit(dev)) {
        __asm__ volatile("yield");
    }
    pl011_write_reg(dev, PL011_DR, byte);
}

static u8 pl011_read_byte(pl011_device_t* dev) {
    while (!pl011_has_data(dev)) {
        __asm__ volatile("yield");
    }
    return (u8)pl011_read_reg(dev, PL011_DR);
}

static i32 pl011_read(void* device, u64 offset, u64 size, void* buffer) {
    pl011_device_t* dev = (pl011_device_t*)device;
    
    if (!dev || !buffer || !dev->initialized) {
        return ERR_INVALID;
    }
    
    u64 bytes_read = 0;
    u8* buf = (u8*)buffer;
    
    while (bytes_read < size) {
        if (dev->rx_head == dev->rx_tail) {
            if (pl011_has_data(dev)) {
                u8 byte = pl011_read_byte(dev);
                u32 idx = dev->rx_tail % UART_BUFFER_SIZE;
                dev->rx_buffer[idx] = byte;
                dev->rx_tail++;
            } else {
                break;
            }
        }
        
        if (dev->rx_head != dev->rx_tail) {
            u32 idx = dev->rx_head % UART_BUFFER_SIZE;
            buf[bytes_read++] = dev->rx_buffer[idx];
            dev->rx_head++;
        }
    }
    
    return (i32)bytes_read;
}

static i32 pl011_write(void* device, u64 offset, u64 size, const void* buffer) {
    pl011_device_t* dev = (pl011_device_t*)device;
    
    if (!dev || !buffer || !dev->initialized) {
        return ERR_INVALID;
    }
    
    const u8* buf = (const u8*)buffer;
    
    for (u64 i = 0; i < size; i++) {
        pl011_write_byte(dev, buf[i]);
    }
    
    return (i32)size;
}

static i32 pl011_ioctl(void* device, u32 cmd, void* arg) {
    return ERR_INVALID;
}

static device_ops_t pl011_ops = {
    .read = pl011_read,
    .write = pl011_write,
    .ioctl = pl011_ioctl,
    .read_block = NULL,
    .write_block = NULL,
    .ioctl_block = NULL,
    .open = NULL,
    .close = NULL,
    .probe = NULL,
    .remove = NULL
};

void pl011_init(void* dt_dev) {
    console_print("Initializing PL011 UART...\n");
    
    pl011_dev = (pl011_device_t*)malloc(sizeof(pl011_device_t));
    if (!pl011_dev) {
        console_print("  Failed to allocate device structure\n");
        return;
    }
    
    pl011_dev->base_addr = 0x09000000;
    pl011_dev->rx_head = 0;
    pl011_dev->rx_tail = 0;
    pl011_dev->tx_head = 0;
    pl011_dev->tx_tail = 0;
    pl011_dev->initialized = false;
    
    pl011_write_reg(pl011_dev, PL011_CR, 0);
    pl011_write_reg(pl011_dev, PL011_IBRD, 1);
    pl011_write_reg(pl011_dev, PL011_FBRD, 40);
    pl011_write_reg(pl011_dev, PL011_LCRH, PL011_LCRH_WLEN_8 | PL011_LCRH_FEN);
    pl011_write_reg(pl011_dev, PL011_CR, PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE);
    
    pl011_dev->initialized = true;
    
    i32 major = device_register("pl011", DEVICE_CHAR, &pl011_ops, pl011_dev);
    if (major < 0) {
        console_print("  Failed to register PL011 device\n");
        free(pl011_dev);
        pl011_dev = NULL;
        return;
    }
    
    console_print("  PL011 UART registered (major=");
    console_print_dec(major);
    console_print(", base=0x");
    console_print_hex(pl011_dev->base_addr);
    console_print(")\n");
    
    vfs_create_device_node("/dev/ttyAMA0", S_IFCHR | 0660, major, 0);
    vfs_create_device_node("/dev/tty", S_IFCHR | 0660, major, 0);
}
