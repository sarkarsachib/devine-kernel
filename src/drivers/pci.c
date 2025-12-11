/* PCI/PCIe Bus Walker for x86_64 */

#include "../include/types.h"
#include "../include/console.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_VENDOR_ID      0x00
#define PCI_DEVICE_ID      0x02
#define PCI_COMMAND        0x04
#define PCI_STATUS         0x06
#define PCI_REVISION_ID    0x08
#define PCI_CLASS_CODE     0x0B
#define PCI_SUBCLASS       0x0A
#define PCI_PROG_IF        0x09
#define PCI_HEADER_TYPE    0x0E
#define PCI_BAR0           0x10
#define PCI_BAR1           0x14
#define PCI_BAR2           0x18
#define PCI_BAR3           0x1C
#define PCI_BAR4           0x20
#define PCI_BAR5           0x24
#define PCI_INTERRUPT_LINE 0x3C
#define PCI_INTERRUPT_PIN  0x3D

typedef struct pci_device {
    u8 bus;
    u8 device;
    u8 function;
    u16 vendor_id;
    u16 device_id;
    u8 class_code;
    u8 subclass;
    u8 prog_if;
    u8 revision;
    u8 header_type;
    u8 interrupt_line;
    u8 interrupt_pin;
    u64 bar[6];
    u64 bar_size[6];
    struct pci_device* next;
} pci_device_t;

static pci_device_t* pci_devices = NULL;

static inline void outl(u16 port, u32 val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 inl(u16 port) {
    u32 ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(u16 port, u8 val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static u32 pci_config_read(u8 bus, u8 device, u8 function, u8 offset) {
    u32 address = (u32)(
        ((u32)bus << 16) |
        ((u32)device << 11) |
        ((u32)function << 8) |
        (offset & 0xFC) |
        0x80000000
    );
    
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_config_write(u8 bus, u8 device, u8 function, u8 offset, u32 value) {
    u32 address = (u32)(
        ((u32)bus << 16) |
        ((u32)device << 11) |
        ((u32)function << 8) |
        (offset & 0xFC) |
        0x80000000
    );
    
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

static u16 pci_read_vendor_id(u8 bus, u8 device, u8 function) {
    u32 config = pci_config_read(bus, device, function, PCI_VENDOR_ID);
    return (u16)(config & 0xFFFF);
}

static u16 pci_read_device_id(u8 bus, u8 device, u8 function) {
    u32 config = pci_config_read(bus, device, function, PCI_DEVICE_ID);
    return (u16)(config & 0xFFFF);
}

static u8 pci_read_class(u8 bus, u8 device, u8 function) {
    u32 config = pci_config_read(bus, device, function, PCI_CLASS_CODE - 1);
    return (u8)((config >> 24) & 0xFF);
}

static u8 pci_read_subclass(u8 bus, u8 device, u8 function) {
    u32 config = pci_config_read(bus, device, function, PCI_SUBCLASS - 1);
    return (u8)((config >> 16) & 0xFF);
}

static u8 pci_read_prog_if(u8 bus, u8 device, u8 function) {
    u32 config = pci_config_read(bus, device, function, PCI_PROG_IF - 1);
    return (u8)((config >> 8) & 0xFF);
}

static u8 pci_read_header_type(u8 bus, u8 device, u8 function) {
    u32 config = pci_config_read(bus, device, function, PCI_HEADER_TYPE - 1);
    return (u8)((config >> 16) & 0xFF);
}

static u64 pci_read_bar(u8 bus, u8 device, u8 function, u8 bar_num, u64* size) {
    if (bar_num >= 6) return 0;
    
    u8 bar_offset = PCI_BAR0 + (bar_num * 4);
    u32 bar = pci_config_read(bus, device, function, bar_offset);
    
    if (bar == 0 || bar == 0xFFFFFFFF) {
        *size = 0;
        return 0;
    }
    
    pci_config_write(bus, device, function, bar_offset, 0xFFFFFFFF);
    u32 size_mask = pci_config_read(bus, device, function, bar_offset);
    pci_config_write(bus, device, function, bar_offset, bar);
    
    if (bar & 0x1) {
        *size = ~(size_mask & 0xFFFFFFFC) + 1;
        return bar & 0xFFFFFFFC;
    } else {
        if ((bar & 0x6) == 0x4) {
            u32 bar_high = pci_config_read(bus, device, function, bar_offset + 4);
            u64 bar64 = ((u64)bar_high << 32) | (bar & 0xFFFFFFF0);
            *size = ~((u64)size_mask & 0xFFFFFFF0) + 1;
            return bar64;
        } else {
            *size = ~(size_mask & 0xFFFFFFF0) + 1;
            return bar & 0xFFFFFFF0;
        }
    }
}

static void pci_probe_device(u8 bus, u8 device, u8 function) {
    u16 vendor_id = pci_read_vendor_id(bus, device, function);
    
    if (vendor_id == 0xFFFF) {
        return;
    }
    
    pci_device_t* pci_dev = (pci_device_t*)malloc(sizeof(pci_device_t));
    if (!pci_dev) {
        return;
    }
    
    pci_dev->bus = bus;
    pci_dev->device = device;
    pci_dev->function = function;
    pci_dev->vendor_id = vendor_id;
    pci_dev->device_id = pci_read_device_id(bus, device, function);
    pci_dev->class_code = pci_read_class(bus, device, function);
    pci_dev->subclass = pci_read_subclass(bus, device, function);
    pci_dev->prog_if = pci_read_prog_if(bus, device, function);
    pci_dev->header_type = pci_read_header_type(bus, device, function);
    
    u32 interrupt_info = pci_config_read(bus, device, function, PCI_INTERRUPT_LINE);
    pci_dev->interrupt_line = (u8)(interrupt_info & 0xFF);
    pci_dev->interrupt_pin = (u8)((interrupt_info >> 8) & 0xFF);
    
    for (u8 i = 0; i < 6; i++) {
        pci_dev->bar[i] = pci_read_bar(bus, device, function, i, &pci_dev->bar_size[i]);
    }
    
    pci_dev->next = pci_devices;
    pci_devices = pci_dev;
    
    console_print("  PCI ");
    console_print_dec(bus);
    console_print(":");
    console_print_dec(device);
    console_print(":");
    console_print_dec(function);
    console_print(" - Vendor: 0x");
    console_print_hex(vendor_id);
    console_print(" Device: 0x");
    console_print_hex(pci_dev->device_id);
    console_print(" Class: 0x");
    console_print_hex(pci_dev->class_code);
    console_print("\n");
    
    device_register_pci(pci_dev);
}

void pci_scan_bus(u8 bus) {
    for (u8 device = 0; device < 32; device++) {
        u16 vendor_id = pci_read_vendor_id(bus, device, 0);
        if (vendor_id == 0xFFFF) {
            continue;
        }
        
        pci_probe_device(bus, device, 0);
        
        u8 header_type = pci_read_header_type(bus, device, 0);
        if (header_type & 0x80) {
            for (u8 function = 1; function < 8; function++) {
                u16 func_vendor = pci_read_vendor_id(bus, device, function);
                if (func_vendor != 0xFFFF) {
                    pci_probe_device(bus, device, function);
                }
            }
        }
    }
}

void pci_init(void) {
    console_print("Scanning PCI bus...\n");
    
    for (u16 bus = 0; bus < 256; bus++) {
        pci_scan_bus((u8)bus);
    }
    
    console_print("PCI scan complete\n");
}

pci_device_t* pci_find_device(u16 vendor_id, u16 device_id) {
    pci_device_t* dev = pci_devices;
    while (dev) {
        if (dev->vendor_id == vendor_id && dev->device_id == device_id) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

pci_device_t* pci_find_class(u8 class_code, u8 subclass) {
    pci_device_t* dev = pci_devices;
    while (dev) {
        if (dev->class_code == class_code && dev->subclass == subclass) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

void device_register_pci(pci_device_t* pci_dev) {
    if (pci_dev->class_code == 0x01 && pci_dev->subclass == 0x00) {
        console_print("    Found VirtIO block device\n");
        virtio_blk_init(pci_dev);
    } else if (pci_dev->class_code == 0x02 && pci_dev->subclass == 0x00) {
        console_print("    Found VirtIO network device\n");
        virtio_net_init(pci_dev);
    } else if (pci_dev->vendor_id == 0x1AF4 && pci_dev->device_id >= 0x1000 && pci_dev->device_id <= 0x103F) {
        console_print("    Found VirtIO device (vendor-specific)\n");
        if (pci_dev->device_id == 0x1001) {
            virtio_blk_init(pci_dev);
        } else if (pci_dev->device_id == 0x1000) {
            virtio_net_init(pci_dev);
        }
    }
}
