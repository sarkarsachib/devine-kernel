/* 16550 UART Driver for x86_64 - Enhanced with TTY Support */

#include "../../include/types.h"
#include "../../include/console.h"
#include "../../include/tty.h"

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

#define UART_INT_ID        0x0A
#define UART_INT_ID_THR_EMPTY  0x02
#define UART_INT_ID_DATA_READY 0x04
#define UART_INT_ID_LINE_STATUS 0x06

#define UART_FIFO_SIZE     16

typedef struct uart16550_device {
    u16 port;
    
    // TTY integration
    tty_t* tty;
    bool use_tty;
    
    // Enhanced buffers
    u8 rx_buffer[UART_BUFFER_SIZE];
    u32 rx_head;
    u32 rx_tail;
    u8 tx_buffer[UART_BUFFER_SIZE];
    u32 tx_head;
    u32 tx_tail;
    
    // Hardware configuration
    u32 baud_rate;
    u8 data_bits;
    u8 stop_bits;
    u8 parity;
    bool flow_control;
    
    // Interrupt handling
    bool interrupt_enabled;
    u8 interrupt_mask;
    
    // Capabilities
    u32 capabilities;
    
    bool initialized;
} uart16550_device_t;

static uart16550_device_t* uart_dev = NULL;

// Static functions for hardware access
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

// Interrupt handler
static void uart16550_interrupt_handler(uart16550_device_t* dev) {
    u8 int_id = uart_read_reg(dev, UART_INT_ID);
    
    switch (int_id & 0x0F) {
        case UART_INT_ID_DATA_READY:
            // Handle received data
            while (uart_has_data(dev)) {
                u8 byte = uart_read_byte(dev);
                if (dev->use_tty && dev->tty) {
                    tty_handle_input_interrupt(dev->tty, byte);
                } else {
                    // Legacy buffer handling
                    u32 idx = dev->rx_tail % UART_BUFFER_SIZE;
                    dev->rx_buffer[idx] = byte;
                    dev->rx_tail++;
                }
            }
            break;
            
        case UART_INT_ID_THR_EMPTY:
            // Handle transmit ready
            if (dev->use_tty && dev->tty) {
                tty_handle_output_interrupt(dev->tty);
            } else {
                // Legacy TX handling
                while (uart_can_transmit(dev) && (dev->tx_head != dev->tx_tail)) {
                    u32 idx = dev->tx_head % UART_BUFFER_SIZE;
                    uart_write_reg(dev, UART_DATA, dev->tx_buffer[idx]);
                    dev->tx_head++;
                }
            }
            break;
            
        case UART_INT_ID_LINE_STATUS:
            // Handle line status (errors)
            uart_read_reg(dev, UART_LINE_STATUS);
            break;
            
        default:
            // Unknown interrupt
            break;
    }
}

// Enhanced hardware configuration
static void uart_configure(uart16550_device_t* dev, u32 baud, u8 data, u8 stop, u8 parity, bool flow) {
    // Calculate baud rate divisor (assuming 1.8432 MHz clock)
    u16 divisor = 115200 / baud;
    
    // Enable DLAB for baud rate programming
    uart_write_reg(dev, UART_LINE_CTRL, 0x80);
    
    // Set baud rate
    uart_write_reg(dev, UART_DATA, divisor & 0xFF);
    uart_write_reg(dev, UART_INT_ENABLE, (divisor >> 8) & 0xFF);
    
    // Configure data format
    u8 lcr = 0;
    switch (data) {
        case 5: lcr |= 0x00; break;
        case 6: lcr |= 0x01; break;
        case 7: lcr |= 0x02; break;
        case 8: lcr |= 0x03; break;
    }
    
    if (stop == 2) {
        lcr |= 0x04;
    }
    
    if (parity != 0) {
        lcr |= 0x08;
        if (parity == 'O' || parity == 'o') {
            lcr |= 0x10;  // Odd parity
        }
    }
    
    uart_write_reg(dev, UART_LINE_CTRL, lcr);
    
    // Store configuration
    dev->baud_rate = baud;
    dev->data_bits = data;
    dev->stop_bits = stop;
    dev->parity = parity;
    dev->flow_control = flow;
    
    // Configure FIFO
    uart_write_reg(dev, UART_FIFO_CTRL, 0xC7);  // Enable FIFO, clear RX/TX, 14-byte trigger
    
    // Configure interrupts
    if (dev->interrupt_enabled) {
        u8 ier = 0;
        if (dev->use_tty) {
            ier |= 0x01;  // Data available interrupt
            ier |= 0x02;  // THR empty interrupt
        }
        uart_write_reg(dev, UART_INT_ENABLE, ier);
    }
    
    // Configure modem control
    u8 mcr = 0x0B;  // DTR, RTS, OUT2
    if (flow) {
        mcr |= 0x20;  // Enable loopback for flow control testing
    }
    uart_write_reg(dev, UART_MODEM_CTRL, mcr);
}

// Capability detection and reporting
static void uart_detect_capabilities(uart16550_device_t* dev) {
    dev->capabilities = TTY_CAP_HAVE_CTS | TTY_CAP_HAVE_DSR | TTY_CAP_HAVE_DCD;
    
    // Detect FIFO capabilities
    uart_write_reg(dev, UART_FIFO_CTRL, 0x00);
    uart_write_reg(dev, UART_FIFO_CTRL, 0xC7);
    
    // Check if FIFO is actually present by reading back
    u8 test = uart_read_reg(dev, UART_FIFO_CTRL);
    if ((test & 0xC0) == 0xC0) {
        dev->capabilities |= TTY_CAP_HARDWARE_SPEED;
    }
    
    // Detect hardware flow control capabilities
    dev->capabilities |= TTY_CAP_HAVE_RTSCTS;
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

// TTY driver operations
static i32 uart16550_open(tty_t* tty) {
    uart16550_device_t* dev = (uart16550_device_t*)tty->driver_data;
    if (!dev) return ERR_INVALID;
    
    dev->use_tty = true;
    dev->tty = tty;
    
    // Configure with default settings
    uart_configure(dev, 9600, 8, 1, 0, false);
    uart_detect_capabilities(dev);
    
    return 0;
}

static i32 uart16550_write_tty(tty_t* tty, const u8* buf, u32 count) {
    uart16550_device_t* dev = (uart16550_device_t*)tty->driver_data;
    if (!dev || !buf || count == 0) return ERR_INVALID;
    
    // Add to TX buffer and start transmission if needed
    for (u32 i = 0; i < count; i++) {
        if ((dev->tx_tail - dev->tx_head) < UART_BUFFER_SIZE) {
            u32 idx = dev->tx_tail % UART_BUFFER_SIZE;
            dev->tx_buffer[idx] = buf[i];
            dev->tx_tail++;
        } else {
            break;  // Buffer full
        }
    }
    
    // Start transmission if not already running
    if (dev->interrupt_enabled && (dev->tx_head != dev->tx_tail)) {
        u8 byte = dev->tx_buffer[dev->tx_head % UART_BUFFER_SIZE];
        dev->tx_head++;
        uart_write_reg(dev, UART_DATA, byte);
    }
    
    return count;
}

static i32 uart16550_set_termios(tty_t* tty, struct termios* termios) {
    uart16550_device_t* dev = (uart16550_device_t*)tty->driver_data;
    if (!dev || !termios) return ERR_INVALID;
    
    // Extract configuration from termios
    u32 baud = B9600;
    u8 data_bits = 8;
    u8 stop_bits = 1;
    u8 parity = 0;
    bool flow_control = false;
    
    // Parse baud rate
    switch (termios->c_cflag & CBAUD) {
        case B50: baud = 50; break;
        case B75: baud = 75; break;
        case B110: baud = 110; break;
        case B134: baud = 134; break;
        case B150: baud = 150; break;
        case B200: baud = 200; break;
        case B300: baud = 300; break;
        case B600: baud = 600; break;
        case B1200: baud = 1200; break;
        case B1800: baud = 1800; break;
        case B2400: baud = 2400; break;
        case B4800: baud = 4800; break;
        case B9600: baud = 9600; break;
        case B19200: baud = 19200; break;
        case B38400: baud = 38400; break;
    }
    
    // Parse data bits
    switch (termios->c_cflag & CSIZE) {
        case CS5: data_bits = 5; break;
        case CS6: data_bits = 6; break;
        case CS7: data_bits = 7; break;
        case CS8: data_bits = 8; break;
    }
    
    // Parse stop bits
    if (termios->c_cflag & CSTOPB) {
        stop_bits = 2;
    }
    
    // Parse parity
    if (termios->c_cflag & PARENB) {
        parity = (termios->c_cflag & PARODD) ? 'O' : 'E';
    }
    
    // Parse flow control
    if (termios->c_iflag & IXON || termios->c_iflag & IXOFF) {
        flow_control = true;
    }
    
    // Configure hardware
    uart_configure(dev, baud, data_bits, stop_bits, parity, flow_control);
    
    return 0;
}

// Legacy device operations (for compatibility)
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

// Legacy read/write functions for compatibility
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
    // Legacy ioctl - not used with TTY subsystem
    return ERR_INVALID;
}

void uart16550_init(void) {
    console_print("Initializing enhanced 16550 UART with TTY support...\n");
    
    uart_dev = (uart16550_device_t*)malloc(sizeof(uart16550_device_t));
    if (!uart_dev) {
        console_print("  Failed to allocate device structure\n");
        return;
    }
    
    // Initialize device structure
    uart_dev->port = UART_PORT_COM1;
    uart_dev->tty = NULL;
    uart_dev->use_tty = false;
    uart_dev->rx_head = 0;
    uart_dev->rx_tail = 0;
    uart_dev->tx_head = 0;
    uart_dev->tx_tail = 0;
    uart_dev->baud_rate = 9600;
    uart_dev->data_bits = 8;
    uart_dev->stop_bits = 1;
    uart_dev->parity = 0;
    uart_dev->flow_control = false;
    uart_dev->interrupt_enabled = true;
    uart_dev->interrupt_mask = 0x0F;
    uart_dev->capabilities = 0;
    uart_dev->initialized = false;
    
    // Basic UART initialization
    uart_write_reg(uart_dev, UART_INT_ENABLE, 0x00);
    uart_write_reg(uart_dev, UART_LINE_CTRL, 0x80);
    uart_write_reg(uart_dev, UART_DATA, 0x03);
    uart_write_reg(uart_dev, UART_INT_ENABLE, 0x00);
    uart_write_reg(uart_dev, UART_LINE_CTRL, 0x03);
    uart_write_reg(uart_dev, UART_FIFO_CTRL, 0xC7);
    uart_write_reg(uart_dev, UART_MODEM_CTRL, 0x0B);
    uart_write_reg(uart_dev, UART_INT_ENABLE, 0x01);
    
    // Detect capabilities
    uart_detect_capabilities(uart_dev);
    
    uart_dev->initialized = true;
    
    // Register as TTY driver
    console_print("  Registering TTY driver for 16550 UART...\n");
    
    // Create TTY driver structure
    static tty_driver_t uart_driver = {
        .major = 0,  // Will be assigned by TTY subsystem
        .minor_start = 0,
        .num = 1,
        .name = "uart16550",
        .type = TTY_DRIVER_TYPE_SERIAL,
        .subtype = 0,
        .flags = 0,
        .lookup = NULL,
        .install = NULL,
        .remove = NULL,
        .open_fn = NULL,
        .close_fn = NULL,
        .write_fn = NULL,
        .put_char_fn = NULL,
        .write_room_fn = NULL,
        .set_termios_fn = NULL,
        .stop_fn = NULL,
        .start_fn = NULL,
        .hangup_fn = NULL,
        .driver_state = uart_dev,
        .ttys = NULL,
        .num_ttys = 1
    };
    
    // Allocate TTY device
    tty_t* tty = tty_allocate_driver(1, 0);
    if (!tty) {
        console_print("  Failed to allocate TTY device\n");
        free(uart_dev);
        uart_dev = NULL;
        return;
    }
    
    // Set up TTY device
    tty->driver_data = uart_dev;
    tty->capabilities = uart_dev->capabilities;
    tty->open = uart16550_open;
    tty->write = uart16550_write_tty;
    tty->set_termios = uart16550_set_termios;
    
    // Register TTY device
    i32 result = tty_register_device(tty, 0);
    if (result < 0) {
        console_print("  Failed to register TTY device\n");
        free(tty);
        free(uart_dev);
        uart_dev = NULL;
        return;
    }
    
    console_print("  16550 UART registered as TTY device (major=");
    console_print_dec(result);
    console_print(", port=0x");
    console_print_hex(uart_dev->port);
    console_print(", capabilities=0x");
    console_print_hex(uart_dev->capabilities);
    console_print(")\n");
    
    // Create device nodes
    vfs_create_device_node("/dev/ttyS0", S_IFCHR | 0660, result, 0);
    vfs_create_device_node("/dev/tty", S_IFCHR | 0660, result, 0);
}
