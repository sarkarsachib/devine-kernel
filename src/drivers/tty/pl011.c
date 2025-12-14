/* PL011 UART Driver for ARM64 - Enhanced with TTY Support */

#include "../../include/types.h"
#include "../../include/console.h"
#include "../../include/tty.h"

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
#define PL011_CR_SIREN  (1 << 1)
#define PL011_CR_SIRLP  (1 << 2)

#define PL011_LCRH_FEN  (1 << 4)
#define PL011_LCRH_WLEN_8 (3 << 5)
#define PL011_LCRH_STP2 (1 << 3)
#define PL011_LCRH_PEN  (1 << 1)
#define PL011_LCRH_EPS  (1 << 2)
#define PL011_LCRH_BRK  (1 << 0)

#define PL011_IMSC_RXIM (1 << 4)
#define PL011_IMSC_TXIM (1 << 5)
#define PL011_IMSC_RTIM (1 << 6)
#define PL011_IMSC_FEIM (1 << 7)
#define PL011_IMSC_PEIM (1 << 8)
#define PL011_IMSC_BEIM (1 << 9)
#define PL011_IMSC_OEIM (1 << 10)

#define PL011_DR_DATA_MASK 0xFF
#define PL011_DR_FE        (1 << 8)
#define PL011_DR_PE        (1 << 9)
#define PL011_DR_BE        (1 << 10)
#define PL011_DR_OE        (1 << 11)

#define PL011_FR_CTS       (1 << 0)
#define PL011_FR_DSR       (1 << 1)
#define PL011_FR_DCD       (1 << 2)
#define PL011_FR_BUSY      (1 << 3)
#define PL011_FR_RXFE      (1 << 4)
#define PL011_FR_TXFF      (1 << 5)
#define PL011_FR_RXFF      (1 << 6)
#define PL011_FR_TXFE      (1 << 7)

#define UART_BUFFER_SIZE 1024

typedef struct pl011_device {
    u64 base_addr;
    
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
    
    // DMA support (optional)
    bool dma_enabled;
    u32 rx_dma_channel;
    u32 tx_dma_channel;
    
    // Interrupt handling
    bool interrupt_enabled;
    u32 interrupt_mask;
    
    // Capabilities
    u32 capabilities;
    
    bool initialized;
} pl011_device_t;

static pl011_device_t* pl011_dev = NULL;

// Static functions for hardware access
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

// Enhanced interrupt handler
static void pl011_interrupt_handler(pl011_device_t* dev) {
    u32 mis = pl011_read_reg(dev, PL011_MIS);
    
    if (mis & PL011_IMSC_RXIM) {
        // RX interrupt - data available
        while (pl011_has_data(dev)) {
            u32 dr = pl011_read_reg(dev, PL011_DR);
            if (!(dr & (PL011_DR_FE | PL011_DR_PE | PL011_DR_BE | PL011_DR_OE))) {
                u8 byte = dr & PL011_DR_DATA_MASK;
                if (dev->use_tty && dev->tty) {
                    tty_handle_input_interrupt(dev->tty, byte);
                } else {
                    // Legacy buffer handling
                    u32 idx = dev->rx_tail % UART_BUFFER_SIZE;
                    dev->rx_buffer[idx] = byte;
                    dev->rx_tail++;
                }
            }
        }
        // Clear interrupt
        pl011_write_reg(dev, PL011_ICR, PL011_IMSC_RXIM);
    }
    
    if (mis & PL011_IMSC_TXIM) {
        // TX interrupt - transmit ready
        if (dev->use_tty && dev->tty) {
            tty_handle_output_interrupt(dev->tty);
        } else {
            // Legacy TX handling
            if (dev->tx_head != dev->tx_tail) {
                u32 idx = dev->tx_head % UART_BUFFER_SIZE;
                pl011_write_reg(dev, PL011_DR, dev->tx_buffer[idx]);
                dev->tx_head++;
            }
        }
        // Clear interrupt
        pl011_write_reg(dev, PL011_ICR, PL011_IMSC_TXIM);
    }
    
    if (mis & (PL011_IMSC_FEIM | PL011_IMSC_PEIM | PL011_IMSC_BEIM | PL011_IMSC_OEIM)) {
        // Error interrupt - clear it
        pl011_write_reg(dev, PL011_ICR, PL011_IMSC_FEIM | PL011_IMSC_PEIM | PL011_IMSC_BEIM | PL011_IMSC_OEIM);
    }
}

// Enhanced hardware configuration
static void pl011_configure(pl011_device_t* dev, u32 baud, u8 data, u8 stop, u8 parity, bool flow) {
    // Calculate baud rate divisors
    u32 clock = 24000000;  // 24 MHz typical for PL011
    u32 ibrd = clock / (16 * baud);
    u32 fbrd = (clock % (16 * baud)) * 64 / (16 * baud);
    
    // Disable UART during configuration
    pl011_write_reg(dev, PL011_CR, 0);
    
    // Configure baud rate
    pl011_write_reg(dev, PL011_IBRD, ibrd);
    pl011_write_reg(dev, PL011_FBRD, fbrd);
    
    // Configure line format
    u32 lcrh = 0;
    switch (data) {
        case 5: lcrh |= (0 << 5); break;
        case 6: lcrh |= (1 << 5); break;
        case 7: lcrh |= (2 << 5); break;
        case 8: lcrh |= (3 << 5); break;
    }
    
    if (stop == 2) {
        lcrh |= PL011_LCRH_STP2;
    }
    
    if (parity != 0) {
        lcrh |= PL011_LCRH_PEN;
        if (parity == 'O' || parity == 'o') {
            // Odd parity (EPS=0)
        } else {
            // Even parity
            lcrh |= PL011_LCRH_EPS;
        }
    }
    
    lcrh |= PL011_LCRH_FEN;  // Enable FIFO
    
    pl011_write_reg(dev, PL011_LCRH, lcrh);
    
    // Store configuration
    dev->baud_rate = baud;
    dev->data_bits = data;
    dev->stop_bits = stop;
    dev->parity = parity;
    dev->flow_control = flow;
    
    // Configure interrupts
    if (dev->interrupt_enabled) {
        u32 imsc = PL011_IMSC_RXIM | PL011_IMSC_TXIM;
        if (flow) {
            imsc |= PL011_IMSC_RTIM;  // Receive timeout interrupt
        }
        pl011_write_reg(dev, PL011_IMSC, imsc);
    }
    
    // Re-enable UART
    pl011_write_reg(dev, PL011_CR, PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE);
}

// DMA support functions (optional)
static void pl011_enable_dma(pl011_device_t* dev) {
    // DMA would be configured here
    dev->dma_enabled = true;
    console_print("  PL011 DMA enabled\n");
}

static void pl011_disable_dma(pl011_device_t* dev) {
    // DMA would be disabled here
    dev->dma_enabled = false;
}

// Capability detection and reporting
static void pl011_detect_capabilities(pl011_device_t* dev) {
    dev->capabilities = TTY_CAP_HAVE_CTS | TTY_CAP_HAVE_DSR | TTY_CAP_HAVE_DCD | TTY_CAP_HAVE_RTSCTS;
    
    // Detect FIFO capabilities
    u32 fr = pl011_read_reg(dev, PL011_FR);
    if (fr & (PL011_FR_TXFF | PL011_FR_RXFE)) {
        dev->capabilities |= TTY_CAP_HARDWARE_SPEED;
    }
    
    // Detect DMA capabilities (would check DMA controller)
    dev->capabilities |= TTY_CAP_SOFTWARE_FLOW;
    
    console_print("  PL011 capabilities: 0x");
    console_print_hex(dev->capabilities);
    console_print("\n");
}

// Hardware access functions
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
    return (u8)(pl011_read_reg(dev, PL011_DR) & PL011_DR_DATA_MASK);
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
    // Legacy ioctl - not used with TTY subsystem
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

// TTY driver operations for PL011
static i32 pl011_open(tty_t* tty) {
    pl011_device_t* dev = (pl011_device_t*)tty->driver_data;
    if (!dev) return ERR_INVALID;
    
    dev->use_tty = true;
    dev->tty = tty;
    
    // Configure with default settings
    pl011_configure(dev, 9600, 8, 1, 0, false);
    pl011_detect_capabilities(dev);
    
    return 0;
}

static i32 pl011_write_tty(tty_t* tty, const u8* buf, u32 count) {
    pl011_device_t* dev = (pl011_device_t*)tty->driver_data;
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
        pl011_write_reg(dev, PL011_DR, byte);
    }
    
    return count;
}

static i32 pl011_set_termios(tty_t* tty, struct termios* termios) {
    pl011_device_t* dev = (pl011_device_t*)tty->driver_data;
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
    pl011_configure(dev, baud, data_bits, stop_bits, parity, flow_control);
    
    return 0;
}

// Legacy device operations (for compatibility)
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
    console_print("Initializing enhanced PL011 UART with TTY support...\n");
    
    pl011_dev = (pl011_device_t*)malloc(sizeof(pl011_device_t));
    if (!pl011_dev) {
        console_print("  Failed to allocate device structure\n");
        return;
    }
    
    // Initialize device structure
    pl011_dev->base_addr = 0x09000000;
    pl011_dev->tty = NULL;
    pl011_dev->use_tty = false;
    pl011_dev->rx_head = 0;
    pl011_dev->rx_tail = 0;
    pl011_dev->tx_head = 0;
    pl011_dev->tx_tail = 0;
    pl011_dev->baud_rate = 9600;
    pl011_dev->data_bits = 8;
    pl011_dev->stop_bits = 1;
    pl011_dev->parity = 0;
    pl011_dev->flow_control = false;
    pl011_dev->dma_enabled = false;
    pl011_dev->rx_dma_channel = 0;
    pl011_dev->tx_dma_channel = 0;
    pl011_dev->interrupt_enabled = true;
    pl011_dev->interrupt_mask = 0x0F;
    pl011_dev->capabilities = 0;
    pl011_dev->initialized = false;
    
    // Basic PL011 initialization
    pl011_write_reg(pl011_dev, PL011_CR, 0);
    pl011_write_reg(pl011_dev, PL011_IBRD, 1);
    pl011_write_reg(pl011_dev, PL011_FBRD, 40);
    pl011_write_reg(pl011_dev, PL011_LCRH, PL011_LCRH_WLEN_8 | PL011_LCRH_FEN);
    pl011_write_reg(pl011_dev, PL011_CR, PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE);
    
    // Detect capabilities
    pl011_detect_capabilities(pl011_dev);
    
    pl011_dev->initialized = true;
    
    // Register as TTY driver
    console_print("  Registering TTY driver for PL011 UART...\n");
    
    // Allocate TTY device
    tty_t* tty = tty_allocate_driver(1, 0);
    if (!tty) {
        console_print("  Failed to allocate TTY device\n");
        free(pl011_dev);
        pl011_dev = NULL;
        return;
    }
    
    // Set up TTY device
    tty->driver_data = pl011_dev;
    tty->capabilities = pl011_dev->capabilities;
    tty->open = pl011_open;
    tty->write = pl011_write_tty;
    tty->set_termios = pl011_set_termios;
    
    // Register TTY device
    i32 result = tty_register_device(tty, 0);
    if (result < 0) {
        console_print("  Failed to register TTY device\n");
        free(tty);
        free(pl011_dev);
        pl011_dev = NULL;
        return;
    }
    
    console_print("  PL011 UART registered as TTY device (major=");
    console_print_dec(result);
    console_print(", base=0x");
    console_print_hex(pl011_dev->base_addr);
    console_print(", capabilities=0x");
    console_print_hex(pl011_dev->capabilities);
    console_print(")\n");
    
    // Create device nodes
    vfs_create_device_node("/dev/ttyAMA0", S_IFCHR | 0660, result, 0);
    vfs_create_device_node("/dev/tty", S_IFCHR | 0660, result, 0);
}
