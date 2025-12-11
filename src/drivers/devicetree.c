/* Device Tree Parser for ARM64 */

#include "../include/types.h"
#include "../include/console.h"

#define FDT_MAGIC 0xD00DFEED
#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE   0x00000002
#define FDT_PROP       0x00000003
#define FDT_NOP        0x00000004
#define FDT_END        0x00000009

typedef struct fdt_header {
    u32 magic;
    u32 totalsize;
    u32 off_dt_struct;
    u32 off_dt_strings;
    u32 off_mem_rsvmap;
    u32 version;
    u32 last_comp_version;
    u32 boot_cpuid_phys;
    u32 size_dt_strings;
    u32 size_dt_struct;
} fdt_header_t;

typedef struct dt_device {
    char name[MAX_STRING_LEN];
    char compatible[MAX_STRING_LEN];
    u64 reg_base;
    u64 reg_size;
    u32 interrupt;
    struct dt_device* next;
} dt_device_t;

static dt_device_t* dt_devices = NULL;
static void* fdt_base = NULL;

static u32 fdt32_to_cpu(u32 x) {
    return __builtin_bswap32(x);
}

static u64 fdt64_to_cpu(u64 x) {
    return __builtin_bswap64(x);
}

static const char* fdt_string(u32 offset) {
    if (!fdt_base) return NULL;
    fdt_header_t* header = (fdt_header_t*)fdt_base;
    return (const char*)((u8*)fdt_base + fdt32_to_cpu(header->off_dt_strings) + offset);
}

static void* fdt_struct_base(void) {
    if (!fdt_base) return NULL;
    fdt_header_t* header = (fdt_header_t*)fdt_base;
    return (void*)((u8*)fdt_base + fdt32_to_cpu(header->off_dt_struct));
}

static void dt_parse_node(void** ptr, const char* node_path) {
    u32* current = (u32*)*ptr;
    u32 token;
    
    char node_name[MAX_STRING_LEN] = {0};
    char compatible[MAX_STRING_LEN] = {0};
    u64 reg_base = 0;
    u64 reg_size = 0;
    u32 interrupt = 0;
    bool has_reg = false;
    bool has_interrupt = false;
    
    while (1) {
        token = fdt32_to_cpu(*current);
        current++;
        
        if (token == FDT_BEGIN_NODE) {
            const char* name = (const char*)current;
            while (*current) current = (u32*)((u8*)current + 1);
            current = (u32*)ALIGN_UP((u64)current, 4);
            
            strncpy(node_name, name, MAX_STRING_LEN - 1);
            
        } else if (token == FDT_PROP) {
            u32 len = fdt32_to_cpu(*current++);
            u32 nameoff = fdt32_to_cpu(*current++);
            const char* prop_name = fdt_string(nameoff);
            void* prop_data = current;
            
            if (strcmp(prop_name, "compatible") == 0 && len > 0) {
                strncpy(compatible, (const char*)prop_data, MAX_STRING_LEN - 1);
            } else if (strcmp(prop_name, "reg") == 0 && len >= 8) {
                u32* reg_data = (u32*)prop_data;
                reg_base = fdt32_to_cpu(reg_data[0]);
                reg_size = fdt32_to_cpu(reg_data[1]);
                has_reg = true;
            } else if (strcmp(prop_name, "interrupts") == 0 && len >= 4) {
                u32* int_data = (u32*)prop_data;
                interrupt = fdt32_to_cpu(int_data[0]);
                has_interrupt = true;
            }
            
            current = (u32*)ALIGN_UP((u64)current + len, 4);
            
        } else if (token == FDT_END_NODE) {
            if (strlen(node_name) > 0 && strlen(compatible) > 0) {
                dt_device_t* dev = (dt_device_t*)malloc(sizeof(dt_device_t));
                if (dev) {
                    strncpy(dev->name, node_name, MAX_STRING_LEN - 1);
                    strncpy(dev->compatible, compatible, MAX_STRING_LEN - 1);
                    dev->reg_base = reg_base;
                    dev->reg_size = reg_size;
                    dev->interrupt = interrupt;
                    dev->next = dt_devices;
                    dt_devices = dev;
                    
                    console_print("  DT Device: ");
                    console_print(node_name);
                    console_print(" (");
                    console_print(compatible);
                    console_print(")\n");
                    
                    device_register_dt(dev);
                }
            }
            break;
            
        } else if (token == FDT_NOP) {
            continue;
            
        } else if (token == FDT_END) {
            break;
        }
    }
    
    *ptr = current;
}

void dt_init(void* dtb_addr) {
    if (!dtb_addr) {
        console_print("No device tree provided\n");
        return;
    }
    
    fdt_base = dtb_addr;
    fdt_header_t* header = (fdt_header_t*)fdt_base;
    
    if (fdt32_to_cpu(header->magic) != FDT_MAGIC) {
        console_print("Invalid device tree magic\n");
        return;
    }
    
    console_print("Parsing device tree...\n");
    console_print("  Version: ");
    console_print_dec(fdt32_to_cpu(header->version));
    console_print("\n");
    
    void* struct_ptr = fdt_struct_base();
    u32* current = (u32*)struct_ptr;
    
    while (1) {
        u32 token = fdt32_to_cpu(*current);
        
        if (token == FDT_END) {
            break;
        } else if (token == FDT_BEGIN_NODE) {
            dt_parse_node((void**)&current, "");
        } else {
            current++;
        }
    }
    
    console_print("Device tree parsing complete\n");
}

dt_device_t* dt_find_device(const char* compatible) {
    dt_device_t* dev = dt_devices;
    while (dev) {
        if (strncmp(dev->compatible, compatible, MAX_STRING_LEN) == 0) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

void device_register_dt(dt_device_t* dt_dev) {
    if (strncmp(dt_dev->compatible, "virtio,mmio", 11) == 0) {
        console_print("    Found VirtIO MMIO device\n");
        virtio_mmio_init(dt_dev);
    } else if (strncmp(dt_dev->compatible, "arm,pl011", 9) == 0) {
        console_print("    Found PL011 UART\n");
        pl011_init(dt_dev);
    }
}
