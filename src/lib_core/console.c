/* Simple Console Output for Kernel Debugging */

#include "../include/types.h"

// VGA text mode console
#define VGA_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

// VGA colors
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

static u16* vga_buffer = (u16*)VGA_MEMORY;
static u32 vga_row = 0;
static u32 vga_column = 0;

// Make a VGA entry
static u16 vga_entry(unsigned char ch, enum vga_color color) {
    return (u16)ch | ((u16)color << 8);
}

// Move cursor to next line
static void newline(void) {
    vga_column = 0;
    vga_row++;
    if (vga_row >= VGA_HEIGHT) {
        // Simple scroll - clear screen for now
        vga_row = 0;
        vga_column = 0;
    }
}

// Put a character on screen
void console_putchar(char c) {
    if (c == '\n') {
        newline();
        return;
    }
    
    if (c == '\r') {
        vga_column = 0;
        return;
    }
    
    if (vga_column >= VGA_WIDTH) {
        newline();
    }
    
    vga_buffer[vga_row * VGA_WIDTH + vga_column] = vga_entry(c, VGA_COLOR_LIGHT_GREY);
    vga_column++;
}

// Clear the screen
void console_clear(void) {
    for (u32 i = 0; i < VGA_HEIGHT; i++) {
        for (u32 j = 0; j < VGA_WIDTH; j++) {
            vga_buffer[i * VGA_WIDTH + j] = vga_entry(' ', VGA_COLOR_BLACK);
        }
    }
    vga_row = 0;
    vga_column = 0;
}

// Print a string
void console_print(const char* str) {
    while (*str) {
        console_putchar(*str);
        str++;
    }
}

// Print a hexadecimal number
void console_print_hex(u64 num) {
    char hex_chars[] = "0123456789ABCDEF";
    char buffer[32];
    u32 index = 0;
    
    if (num == 0) {
        console_print("0");
        return;
    }
    
    while (num > 0) {
        buffer[index++] = hex_chars[num % 16];
        num /= 16;
    }
    
    for (u32 i = index; i > 0; i--) {
        console_putchar(buffer[i - 1]);
    }
}

// Print a decimal number
void console_print_dec(u64 num) {
    char buffer[32];
    u32 index = 0;
    
    if (num == 0) {
        console_print("0");
        return;
    }
    
    while (num > 0) {
        buffer[index++] = '0' + (num % 10);
        num /= 10;
    }
    
    for (u32 i = index; i > 0; i--) {
        console_putchar(buffer[i - 1]);
    }
}