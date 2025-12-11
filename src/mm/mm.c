/* Memory Management Unit - Virtual Memory and Page Allocation */

#include "../../include/types.h"

// Page directory and page table entries
typedef struct {
    u64 present     : 1;   // Page present in memory
    u64 rw          : 1;   // Read/write
    u64 user        : 1;   // User/supervisor
    u64 pwt         : 1;   // Write-through
    u64 pcd         : 1;   // Cache disable
    u64 accessed    : 1;   // Accessed
    u64 dirty       : 1;   // Dirty
    u64 pat         : 1;   // Page attribute table
    u64 global      : 1;   // Global page
    u64 reserved    : 3;   // Reserved
    u64 frame       : 40;  // Physical frame address
} __attribute__((packed)) page_entry_t;

// Memory management constants
#define PAGE_SIZE 4096
#define PAGE_ALIGNMENT 4096
#define ENTRIES_PER_PAGE (PAGE_SIZE / 8)
#define PD_INDEX(vaddr) ((vaddr >> 22) & 0x3FF)
#define PT_INDEX(vaddr) ((vaddr >> 12) & 0x3FF)

// Physical memory layout
#define KERNEL_PHYS_START 0x00100000
#define KERNEL_VIRT_START 0xFFFF800000000000UL
#define MEMORY_MAP_START  0x00001000
#define PAGE_ALLOC_START  0x00100000

// Memory map entry structure
typedef struct {
    u64 base;
    u64 length;
    u32 type;
} __attribute__((packed)) memory_map_entry_t;

// Memory types
#define MEMORY_AVAILABLE     1
#define MEMORY_RESERVED      2
#define MEMORY_ACPI_RECLAIM  3
#define MEMORY_ACPI_NVS      4

// Global page directory
static page_entry_t* page_dir = (page_entry_t*)0xFFFFF000;
static page_entry_t* page_tables[1024];

// Memory map
static memory_map_entry_t memory_map[256];
static u32 memory_map_count = 0;

// Free page list
static page_entry_t* free_pages = NULL;
static u64 total_pages = 0;
static u64 free_pages_count = 0;

// Initialize memory management
void mm_init(void) {
    // Setup physical memory map
    mm_setup_memory_map();
    
    // Initialize page allocator
    mm_init_page_allocator();
    
    // Setup kernel virtual memory
    mm_setup_kernel_vm();
    
    // Enable paging
    mm_enable_paging();
}

// Setup memory map from bootloader information
void mm_setup_memory_map(void) {
    // For now, create a simple memory map
    // In real implementation, this would parse multiboot memory map
    
    memory_map[0].base = 0x00000000;
    memory_map[0].length = 0x00001000;
    memory_map[0].type = MEMORY_RESERVED; // BIOS/UEFI
    
    memory_map[1].base = 0x00001000;
    memory_map[1].length = 0x0009F000;
    memory_map[1].type = MEMORY_AVAILABLE; // Available low memory
    
    memory_map[2].base = 0x00100000;
    memory_map[2].length = 0x07FF0000;
    memory_map[2].type = MEMORY_AVAILABLE; // Available memory
    
    memory_map[3].base = 0x08000000;
    memory_map[3].length = 0x077F0000;
    memory_map[3].type = MEMORY_RESERVED; // Reserved memory
    
    memory_map_count = 4;
    
    // Count total available pages
    total_pages = 0;
    for (u32 i = 0; i < memory_map_count; i++) {
        if (memory_map[i].type == MEMORY_AVAILABLE) {
            total_pages += memory_map[i].length / PAGE_SIZE;
        }
    }
}

// Initialize page allocator
void mm_init_page_allocator(void) {
    free_pages = NULL;
    free_pages_count = total_pages;
    
    // Add all available pages to free list
    for (u32 i = 0; i < memory_map_count; i++) {
        if (memory_map[i].type == MEMORY_AVAILABLE) {
            u64 pages = memory_map[i].length / PAGE_SIZE;
            for (u64 j = 0; j < pages; j++) {
                u64 page_addr = memory_map[i].base + (j * PAGE_SIZE);
                if (page_addr >= PAGE_ALLOC_START) {
                    mm_free_page((void*)page_addr);
                }
            }
        }
    }
}

// Setup kernel virtual memory mapping
void mm_setup_kernel_vm(void) {
    // Map kernel code and data
    for (u64 i = 0; i < 512; i++) { // Map first 2GB physical to kernel virtual
        mm_map_page((void*)(i * PAGE_SIZE), 
                   (void*)(KERNEL_VIRT_START + i * PAGE_SIZE), 
                   true,  // Kernel space
                   false  // Not read-only
        );
    }
    
    // Identity map first 1MB for early boot
    for (u64 i = 0; i < 256; i++) {
        mm_map_page((void*)(i * PAGE_SIZE), 
                   (void*)(i * PAGE_SIZE), 
                   false, // User space (temporarily)
                   false  // Not read-only
        );
    }
}

// Enable paging
void mm_enable_paging(void) {
    // Set up page directory pointer register
    asm volatile("mov %0, %%cr3" : : "r"((u64)page_dir - KERNEL_VIRT_START));
    
    // Enable paging and write protection
    u64 cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= (1 << 31) | (1 << 16); // Enable paging, write protection
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

// Map a physical page to virtual address
void mm_map_page(void* phys_addr, void* virt_addr, bool user, bool readonly) {
    u64 pd_index = PD_INDEX((u64)virt_addr);
    u64 pt_index = PT_INDEX((u64)virt_addr);
    
    // Create page table if needed
    if (page_dir[pd_index].present == 0) {
        page_dir[pd_index].frame = (u64)mm_alloc_page() >> 12;
        page_dir[pd_index].present = 1;
        page_dir[pd_index].rw = 1; // Read/write
        page_dir[pd_index].user = user ? 1 : 0;
        
        // Clear page table
        page_entry_t* pt = (page_entry_t*)((pd_index * 0x400000) + KERNEL_VIRT_START);
        memset(pt, 0, PAGE_SIZE);
    }
    
    // Get page table
    page_entry_t* pt = (page_entry_t*)((pd_index * 0x400000) + KERNEL_VIRT_START);
    
    // Set page table entry
    pt[pt_index].frame = (u64)phys_addr >> 12;
    pt[pt_index].present = 1;
    pt[pt_index].rw = readonly ? 0 : 1;
    pt[pt_index].user = user ? 1 : 0;
    pt[pt_index].accessed = 1;
    
    // Invalidate TLB entry
    asm volatile("invlpg (%0)" : : "r"(virt_addr));
}

// Allocate a physical page
void* mm_alloc_page(void) {
    if (free_pages == NULL) {
        return NULL; // Out of memory
    }
    
    page_entry_t* page = free_pages;
    free_pages = (page_entry_t*)((u64)page & ~0xFFF);
    free_pages_count--;
    
    // Clear page
    memset(page, 0, PAGE_SIZE);
    
    return page;
}

// Free a physical page
void mm_free_page(void* page_addr) {
    page_entry_t* page = (page_entry_t*)page_addr;
    page->frame = 0; // Mark as free
    page->present = 0;
    
    // Add to free list
    page_entry_t* old_free = free_pages;
    free_pages = page;
    page->rw = (u64)old_free >> 12; // Store old free list in reserved bits
    free_pages_count++;
}

// Get physical address from virtual address
void* mm_virt_to_phys(void* virt_addr) {
    u64 addr = (u64)virt_addr;
    if (addr >= KERNEL_VIRT_START) {
        // Kernel virtual address
        return (void*)(addr - KERNEL_VIRT_START + KERNEL_PHYS_START);
    }
    
    // User virtual address - need to walk page tables
    u64 pd_index = PD_INDEX(addr);
    u64 pt_index = PT_INDEX(addr);
    
    if (page_dir[pd_index].present == 0) {
        return NULL; // Page not mapped
    }
    
    page_entry_t* pt = (page_entry_t*)((pd_index * 0x400000) + KERNEL_VIRT_START);
    if (pt[pt_index].present == 0) {
        return NULL; // Page not mapped
    }
    
    return (void*)((pt[pt_index].frame << 12) | (addr & 0xFFF));
}