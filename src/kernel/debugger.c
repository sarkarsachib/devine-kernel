/* Serial Debugger Command Parser */

#include "../include/types.h"
#include "../include/console.h"

#define CMD_BUFFER_SIZE 128

typedef struct {
    char buffer[CMD_BUFFER_SIZE];
    u32 pos;
    bool enabled;
} debugger_state_t;

static debugger_state_t debugger_state = {
    .buffer = {0},
    .pos = 0,
    .enabled = false
};

void debugger_init(void) {
    debugger_state.enabled = true;
    console_print("Serial debugger initialized. Type 'help' for commands.\n");
}

void debugger_prompt(void) {
    if (!debugger_state.enabled) return;
    console_print("> ");
}

static void cmd_help(void) {
    console_print("Available commands:\n");
    console_print("  help     - Show this help\n");
    console_print("  regs     - Display CPU registers\n");
    console_print("  mem      - Display memory (mem <addr> <len>)\n");
    console_print("  break    - Set breakpoint (break <addr>)\n");
    console_print("  cont     - Continue execution\n");
    console_print("  step     - Single-step execution\n");
    console_print("  bt       - Show backtrace\n");
    console_print("  info     - Show system information\n");
    console_print("  devices  - List registered devices\n");
}

static void cmd_regs(void) {
    console_print("CPU Registers:\n");
    
#ifdef __x86_64__
    u64 rax, rbx, rcx, rdx, rsi, rdi, rsp, rbp;
    u64 r8, r9, r10, r11, r12, r13, r14, r15;
    u64 rip, rflags;
    
    __asm__ volatile(
        "mov %%rax, %0\n"
        "mov %%rbx, %1\n"
        "mov %%rcx, %2\n"
        "mov %%rdx, %3\n"
        "mov %%rsi, %4\n"
        "mov %%rdi, %5\n"
        "mov %%rsp, %6\n"
        "mov %%rbp, %7\n"
        : "=r"(rax), "=r"(rbx), "=r"(rcx), "=r"(rdx),
          "=r"(rsi), "=r"(rdi), "=r"(rsp), "=r"(rbp)
    );
    
    console_print("  RAX: 0x"); console_print_hex(rax); console_print("\n");
    console_print("  RBX: 0x"); console_print_hex(rbx); console_print("\n");
    console_print("  RCX: 0x"); console_print_hex(rcx); console_print("\n");
    console_print("  RDX: 0x"); console_print_hex(rdx); console_print("\n");
    console_print("  RSI: 0x"); console_print_hex(rsi); console_print("\n");
    console_print("  RDI: 0x"); console_print_hex(rdi); console_print("\n");
    console_print("  RSP: 0x"); console_print_hex(rsp); console_print("\n");
    console_print("  RBP: 0x"); console_print_hex(rbp); console_print("\n");
#elif __aarch64__
    u64 x0, x1, x2, x3, sp, lr, pc;
    
    __asm__ volatile(
        "mov %0, x0\n"
        "mov %1, x1\n"
        "mov %2, x2\n"
        "mov %3, x3\n"
        "mov %4, sp\n"
        "mov %5, x30\n"
        : "=r"(x0), "=r"(x1), "=r"(x2), "=r"(x3),
          "=r"(sp), "=r"(lr)
    );
    
    console_print("  X0:  0x"); console_print_hex(x0); console_print("\n");
    console_print("  X1:  0x"); console_print_hex(x1); console_print("\n");
    console_print("  X2:  0x"); console_print_hex(x2); console_print("\n");
    console_print("  X3:  0x"); console_print_hex(x3); console_print("\n");
    console_print("  SP:  0x"); console_print_hex(sp); console_print("\n");
    console_print("  LR:  0x"); console_print_hex(lr); console_print("\n");
#else
    console_print("  Register dump not supported on this architecture\n");
#endif
}

static void cmd_mem(const char* args) {
    console_print("Memory dump: ");
    console_print(args);
    console_print("\n");
    console_print("(Not yet implemented)\n");
}

static void cmd_break(const char* args) {
    console_print("Set breakpoint at: ");
    console_print(args);
    console_print("\n");
    console_print("(Not yet implemented)\n");
}

static void cmd_cont(void) {
    console_print("Continuing execution...\n");
}

static void cmd_step(void) {
    console_print("Single-stepping...\n");
    console_print("(Not yet implemented)\n");
}

static void cmd_bt(void) {
    console_print("Backtrace:\n");
    console_print("(Not yet implemented)\n");
}

static void cmd_info(void) {
    console_print("System Information:\n");
    console_print("  Kernel: Devine OS\n");
#ifdef __x86_64__
    console_print("  Architecture: x86_64\n");
#elif __aarch64__
    console_print("  Architecture: ARM64\n");
#else
    console_print("  Architecture: Unknown\n");
#endif
}

static void cmd_devices(void) {
    console_print("Registered Devices:\n");
    console_print("(Device list will be shown here)\n");
}

static bool str_starts_with(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str != *prefix) return false;
        str++;
        prefix++;
    }
    return true;
}

static const char* skip_whitespace(const char* str) {
    while (*str == ' ' || *str == '\t') str++;
    return str;
}

void debugger_handle_command(const char* cmd) {
    if (!debugger_state.enabled) return;
    
    const char* args = cmd;
    while (*args && *args != ' ' && *args != '\t') args++;
    args = skip_whitespace(args);
    
    if (str_starts_with(cmd, "help")) {
        cmd_help();
    } else if (str_starts_with(cmd, "regs")) {
        cmd_regs();
    } else if (str_starts_with(cmd, "mem")) {
        cmd_mem(args);
    } else if (str_starts_with(cmd, "break")) {
        cmd_break(args);
    } else if (str_starts_with(cmd, "cont")) {
        cmd_cont();
    } else if (str_starts_with(cmd, "step")) {
        cmd_step();
    } else if (str_starts_with(cmd, "bt")) {
        cmd_bt();
    } else if (str_starts_with(cmd, "info")) {
        cmd_info();
    } else if (str_starts_with(cmd, "devices")) {
        cmd_devices();
    } else if (cmd[0] != '\0') {
        console_print("Unknown command: ");
        console_print(cmd);
        console_print("\n");
        console_print("Type 'help' for available commands.\n");
    }
}

void debugger_input_char(char c) {
    if (!debugger_state.enabled) return;
    
    if (c == '\n' || c == '\r') {
        console_print("\n");
        debugger_state.buffer[debugger_state.pos] = '\0';
        debugger_handle_command(debugger_state.buffer);
        debugger_state.pos = 0;
        debugger_prompt();
    } else if (c == '\b' || c == 127) {
        if (debugger_state.pos > 0) {
            debugger_state.pos--;
            console_print("\b \b");
        }
    } else if (debugger_state.pos < CMD_BUFFER_SIZE - 1) {
        debugger_state.buffer[debugger_state.pos++] = c;
        console_putchar(c);
    }
}

bool debugger_is_enabled(void) {
    return debugger_state.enabled;
}

void debugger_enable(void) {
    debugger_state.enabled = true;
}

void debugger_disable(void) {
    debugger_state.enabled = false;
}
