/* 16550 UART Driver for x86_64 */

#include "../../include/types.h"
#include "../../include/console.h"

#define UART_PORT_COM1 0x3F8
#define UART_PORT_COM2 0x2F8

#define UART_DATA          0
#define UART_INT_ENABLE    1
#define UART_FIFO_CTRL     2
#define UART_LINE_CTRL     3
#define UART_MODEM_CTRL    4
#define UART_LINE_STATUS   5
#define UART_MODEM_STATUS  6
#define UART_SCRATCH       7

#define UART_LSR_DATA_READY    0x01
#define UART_LSR_OVERRUN       0x02
#define UART_LSR_PARITY_ERROR  0x04
#define UART_LSR_FRAME_ERROR   0x08
#define UART_LSR_BREAK_INT     0x10
#define UART_LSR_THR_EMPTY     0x20
#define UART_LSR_TRANS_EMPTY   0x40

#define UART_BUFFER_SIZE 1024

typedef struct uart16550_device {
    u16 port;
    u8 rx_buffer[UART_BUFFER_SIZE];
    u32 rx_head;
    u32 rx_tail;
    u8 tx_buffer[UART_BUFFER_SIZE];
    u32 tx_head;
    u32 tx_tail;
    bool initialized;
} uart16550_device_t;

static uart16550_device_t* uart_dev = NULL;

static inline void outb(u16 port, u8 val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void uart_write_reg(uart16550_device_t* dev, u8 reg, u8 value) {
    outb(dev->port + reg, value);
}

static u8 uart_read_reg(uart16550_device_t* dev, u8 reg) {
    return inb(dev->port + reg);
}

static bool uart_can_transmit(uart16550_device_t* dev) {
    return (uart_read_reg(dev, UART_LINE_STATUS) & UART_LSR_THR_EMPTY) != 0;
}

static bool uart_has_data(uart16550_device_t* dev) {
    return (uart_read_reg(dev, UART_LINE_STATUS) & UART_LSR_DATA_READY) != 0;
}

static void uart_write_byte(uart16550_device_t* dev, u8 byte) {
    while (!uart_can_transmit(dev)) {
        __asm__ volatile("pause");
    }
    uart_write_reg(dev, UART_DATA, byte);
}

static u8 uart_read_byte(uart16550_device_t* dev) {
    while (!uart_has_data(dev)) {
        __asm__ volatile("pause");
    }
    return uart_read_reg(dev, UART_DATA);
}

static i32 uart16550_read(void* device, u64 offset, u64 size, void* buffer) {
    uart16550_device_t* dev = (uart16550_device_t*)device;
    
    if (!dev || !buffer || !dev->initialized) {
        return ERR_INVALID;
    }
    
    u64 bytes_read = 0;
    u8* buf = (u8*)buffer;
    
    while (bytes_read < size) {
        if (dev->rx_head == dev->rx_tail) {
            if (uart_has_data(dev)) {
                u8 byte = uart_read_byte(dev);
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

static i32 uart16550_write(void* device, u64 offset, u64 size, const void* buffer) {
    uart16550_device_t* dev = (uart16550_device_t*)device;
    
    if (!dev || !buffer || !dev->initialized) {
        return ERR_INVALID;
    }
    
    const u8* buf = (const u8*)buffer;
    
    for (u64 i = 0; i < size; i++) {
        uart_write_byte(dev, buf[i]);
    }
    
    return (i32)size;
}

static i32 uart16550_ioctl(void* device, u32 cmd, void* arg) {
    return ERR_INVALID;
}

static device_ops_t uart16550_ops = {
    .read = uart16550_read,
    .write = uart16550_write,
    .ioctl = uart16550_ioctl,
    .read_block = NULL,
    .write_block = NULL,
    .ioctl_block = NULL,
    .open = NULL,
    .close = NULL,
    .probe = NULL,
    .remove = NULL
};

void uart16550_init(void) {
    console_print("Initializing 16550 UART...\n");
    
    uart_dev = (uart16550_device_t*)malloc(sizeof(uart16550_device_t));
    if (!uart_dev) {
        console_print("  Failed to allocate device structure\n");
        return;
    }
    
    uart_dev->port = UART_PORT_COM1;
    uart_dev->rx_head = 0;
    uart_dev->rx_tail = 0;
    uart_dev->tx_head = 0;
    uart_dev->tx_tail = 0;
    uart_dev->initialized = false;
    
    uart_write_reg(uart_dev, UART_INT_ENABLE, 0x00);
    uart_write_reg(uart_dev, UART_LINE_CTRL, 0x80);
    uart_write_reg(uart_dev, UART_DATA, 0x03);
    uart_write_reg(uart_dev, UART_INT_ENABLE, 0x00);
    uart_write_reg(uart_dev, UART_LINE_CTRL, 0x03);
    uart_write_reg(uart_dev, UART_FIFO_CTRL, 0xC7);
    uart_write_reg(uart_dev, UART_MODEM_CTRL, 0x0B);
    uart_write_reg(uart_dev, UART_INT_ENABLE, 0x01);
    
    uart_dev->initialized = true;
    
    i32 major = device_register("uart16550", DEVICE_CHAR, &uart16550_ops, uart_dev);
    if (major < 0) {
        console_print("  Failed to register UART device\n");
        free(uart_dev);
        uart_dev = NULL;
        return;
    }
    
    console_print("  16550 UART registered (major=");
    console_print_dec(major);
    console_print(", port=0x");
    console_print_hex(uart_dev->port);
    console_print(")\n");
    
    vfs_create_device_node("/dev/ttyS0", S_IFCHR | 0660, major, 0);
    vfs_create_device_node("/dev/tty", S_IFCHR | 0660, major, 0);
}
