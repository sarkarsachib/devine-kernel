/* GDB Remote Debugging Stub */

#include "../include/types.h"
#include "../include/console.h"

#define GDB_BUFFER_SIZE 1024

typedef struct {
    char buffer[GDB_BUFFER_SIZE];
    u32 pos;
    bool enabled;
    bool connected;
} gdbstub_state_t;

static gdbstub_state_t gdb_state = {
    .buffer = {0},
    .pos = 0,
    .enabled = false,
    .connected = false
};

static u8 hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static char int_to_hex(u8 val) {
    if (val < 10) return '0' + val;
    return 'a' + (val - 10);
}

static u8 compute_checksum(const char* data, u32 len) {
    u8 checksum = 0;
    for (u32 i = 0; i < len; i++) {
        checksum += (u8)data[i];
    }
    return checksum;
}

static void gdb_send_packet(const char* data) {
    u32 len = 0;
    while (data[len]) len++;
    
    u8 checksum = compute_checksum(data, len);
    
    console_putchar('$');
    console_print(data);
    console_putchar('#');
    console_putchar(int_to_hex((checksum >> 4) & 0xF));
    console_putchar(int_to_hex(checksum & 0xF));
}

static void gdb_send_ok(void) {
    gdb_send_packet("OK");
}

static void gdb_send_error(u8 error_code) {
    char buf[8];
    buf[0] = 'E';
    buf[1] = int_to_hex((error_code >> 4) & 0xF);
    buf[2] = int_to_hex(error_code & 0xF);
    buf[3] = '\0';
    gdb_send_packet(buf);
}

static void gdb_handle_query(const char* query) {
    if (query[0] == 'S' && query[1] == 'u' && query[2] == 'p') {
        gdb_send_packet("");
    } else if (query[0] == 'C') {
        gdb_send_packet("QC1");
    } else {
        gdb_send_packet("");
    }
}

static void gdb_handle_read_registers(void) {
    char buf[512];
    u32 pos = 0;
    
#ifdef __x86_64__
    u64 regs[16] = {0};
    
    for (u32 i = 0; i < 16; i++) {
        for (u32 j = 0; j < 16; j++) {
            buf[pos++] = int_to_hex((regs[i] >> (j * 4)) & 0xF);
        }
    }
#elif __aarch64__
    u64 regs[32] = {0};
    
    for (u32 i = 0; i < 32; i++) {
        for (u32 j = 0; j < 16; j++) {
            buf[pos++] = int_to_hex((regs[i] >> (j * 4)) & 0xF);
        }
    }
#endif
    
    buf[pos] = '\0';
    gdb_send_packet(buf);
}

static void gdb_handle_read_memory(const char* args) {
    gdb_send_packet("00000000");
}

static void gdb_handle_continue(void) {
    console_print("[GDB] Continue requested\n");
    gdb_state.connected = false;
}

static void gdb_handle_step(void) {
    console_print("[GDB] Step requested\n");
    gdb_send_packet("S05");
}

static void gdb_handle_packet(const char* packet) {
    if (!packet || !packet[0]) return;
    
    switch (packet[0]) {
        case '?':
            gdb_send_packet("S05");
            break;
            
        case 'q':
            gdb_handle_query(packet + 1);
            break;
            
        case 'g':
            gdb_handle_read_registers();
            break;
            
        case 'm':
            gdb_handle_read_memory(packet + 1);
            break;
            
        case 'c':
            gdb_handle_continue();
            break;
            
        case 's':
            gdb_handle_step();
            break;
            
        case 'k':
            console_print("[GDB] Kill requested\n");
            break;
            
        case 'D':
            gdb_send_ok();
            gdb_state.connected = false;
            break;
            
        default:
            gdb_send_packet("");
            break;
    }
}

void gdbstub_init(void) {
    gdb_state.enabled = true;
    gdb_state.connected = false;
    console_print("GDB stub initialized. Waiting for connection on QEMU serial...\n");
}

void gdbstub_input_char(char c) {
    if (!gdb_state.enabled) return;
    
    if (c == '$') {
        gdb_state.pos = 0;
        gdb_state.connected = true;
    } else if (c == '#') {
        gdb_state.buffer[gdb_state.pos] = '\0';
        
        gdb_handle_packet(gdb_state.buffer);
        
        gdb_state.pos = 0;
    } else if (c == '+') {
        // ACK received
    } else if (c == '-') {
        // NACK received - resend last packet
    } else if (gdb_state.pos < GDB_BUFFER_SIZE - 1) {
        gdb_state.buffer[gdb_state.pos++] = c;
    }
}

bool gdbstub_is_enabled(void) {
    return gdb_state.enabled;
}

bool gdbstub_is_connected(void) {
    return gdb_state.connected;
}

void gdbstub_breakpoint(void) {
    if (!gdb_state.enabled) return;
    
    console_print("[GDB] Breakpoint hit\n");
    gdb_send_packet("S05");
    
    gdb_state.connected = true;
    while (gdb_state.connected) {
        __asm__ volatile("nop");
    }
}
