/* TTY Core Implementation - Comprehensive Terminal Subsystem */

#include "../../include/types.h"
#include "../../include/console.h"
#include "../../include/tty.h"
#include "../device.c"

// Global TTY subsystem state
static tty_t* tty_list = NULL;
static tty_driver_t* tty_drivers = NULL;
static tty_ldisc_t* tty_ldiscs = NULL;
static u32 tty_line_count = 0;

// Default termios settings
const struct termios tty_def_termios = {
    .c_iflag = TTY_DEF_IFLAG,
    .c_oflag = TTY_DEF_OFLAG,
    .c_cflag = TTY_DEF_CFLAG,
    .c_lflag = TTY_DEF_LFLAG,
    .c_cc = {
        [VINTR] = 0x7F,   // DEL
        [VQUIT] = '\\',   // Ctrl+\
        [VERASE] = 0x08,  // BS
        [VKILL] = 'U' & 0x1F, // Ctrl+U
        [VEOF] = 'D' & 0x1F,  // Ctrl+D
        [VTIME] = 0,
        [VMIN] = 1,
        [VSUSP] = 'Z' & 0x1F, // Ctrl+Z
        [VSTART] = 'S' & 0x1F, // Ctrl+S
        [VSTOP] = 'Q' & 0x1F  // Ctrl+Q
    },
    .c_ispeed = B9600,
    .c_ospeed = B9600
};

// Initialize TTY subsystem
void tty_init(void) {
    console_print("Initializing TTY core subsystem...\n");
    
    tty_list = NULL;
    tty_drivers = NULL;
    tty_ldiscs = NULL;
    tty_line_count = 0;
    
    // Register default line disciplines
    tty_register_default_ldiscs();
    
    console_print("  TTY core initialized\n");
    console_print("  Ready for TTY device registration\n");
    
    // Initialize terminal multiplexing
    tty_multiplex_init();
    
    // Initialize session management
    tty_session_init();
}

// Additional TTY ioctl operations
i32 tty_ioctl(tty_t* tty, i32 cmd, u64 arg) {
    if (!tty) {
        return ERR_INVALID;
    }
    
    switch (cmd) {
        case TCGETS:
            return tty_get_termios(tty, (struct termios*)arg);
            
        case TCSETS:
        case TCSETSW:
        case TCSETSF:
            return tty_set_termios(tty, (struct termios*)arg);
            
        case TIOCGWINSZ:
            return tty_get_winsize(tty, (struct winsize*)arg);
            
        case TIOCSWINSZ:
            return tty_set_winsize(tty, (struct winsize*)arg);
            
        case TIOCGPGRP:
            return tty_get_process_group(tty, (u32*)arg);
            
        case TIOCSPGRP:
            return tty_set_process_group(tty, (u32)arg);
            
        case TCSBRK:
            // Send break - would generate break signal on serial lines
            return 0;
            
        case TCXONC:
            // Flow control - would enable/disable XON/XOFF
            return 0;
            
        case TCFLSH:
            // Flush buffers
            if (arg == 0 || arg == 1) {
                tty->input_head = tty->input_tail = 0;
            }
            if (arg == 0 || arg == 2) {
                tty->output_head = tty->output_tail = 0;
            }
            return 0;
            
        default:
            // Pass to line discipline
            if (tty->ldisc && tty->ldisc->ioctl) {
                return tty->ldisc->ioctl(tty, cmd, arg);
            }
            return ERR_INVALID;
    }
}

// Session and process group management
void tty_session_set_leader(tty_t* tty, u32 pid) {
    if (tty) {
        tty->session_leader = pid;
        console_print("TTY session leader set to PID ");
        console_print_dec(pid);
        console_print("\n");
    }
}

void tty_foreground_set_group(tty_t* tty, u32 pgrp) {
    if (tty) {
        tty->foreground_group = pgrp;
        console_print("TTY foreground group set to PGRP ");
        console_print_dec(pgrp);
        console_print("\n");
    }
}

// Enhanced line discipline receive function
void tty_ldisc_receive(tty_t* tty, u8* buf, u32 count) {
    if (!tty || !buf || count == 0) {
        return;
    }
    
    // Add data to input buffer
    u32 available_space = TTY_INPUT_BUFFER - (tty->input_tail - tty->input_head);
    u32 to_copy = (count < available_space) ? count : available_space;
    
    for (u32 i = 0; i < to_copy; i++) {
        u32 idx = tty->input_tail % TTY_INPUT_BUFFER;
        tty->input_buffer[idx] = buf[i];
        tty->input_tail++;
    }
    
    // Line discipline processing
    if (tty->ldisc && tty->ldisc->receive_buf) {
        tty->ldisc->receive_buf(tty, buf, NULL, to_copy);
    }
}

void tty_ldisc_flush_buffer(tty_t* tty) {
    if (!tty) {
        return;
    }
    
    tty->input_head = tty->input_tail = 0;
    tty->output_head = tty->output_tail = 0;
    
    // Wake up processes waiting for data
    // This would integrate with process waiting system
}

// TTY kref (reference counting) support
void tty_kref_put(tty_t* tty) {
    if (!tty) {
        return;
    }
    
    if (tty->ref_count > 0) {
        tty->ref_count--;
        if (tty->ref_count == 0) {
            // Last reference released - can free TTY
            console_print("TTY device freed: ");
            console_print(tty->name);
            console_print("\n");
            // Actual freeing would happen here
        }
    }
}

// Utility functions for line discipline processing
void tty_canonical_process(tty_t* tty) {
    // Process input in canonical mode (line-oriented)
    // Handle erase, kill, word erase, etc.
    // This would be implemented in full canonical processing
}

void tty_raw_process(tty_t* tty) {
    // Process input in raw mode (character-oriented)
    // No special processing, just pass through
}

// Default termios setup
const struct termios tty_def_termios = {
    .c_iflag = TTY_DEF_IFLAG,
    .c_oflag = TTY_DEF_OFLAG,
    .c_cflag = TTY_DEF_CFLAG,
    .c_lflag = TTY_DEF_LFLAG,
    .c_cc = {
        [VINTR] = 0x7F,   // DEL
        [VQUIT] = '\\',   // Ctrl+\
        [VERASE] = 0x08,  // BS
        [VKILL] = 'U' & 0x1F, // Ctrl+U
        [VEOF] = 'D' & 0x1F,  // Ctrl+D
        [VTIME] = 0,
        [VMIN] = 1,
        [VSUSP] = 'Z' & 0x1F, // Ctrl+Z
        [VSTART] = 'S' & 0x1F, // Ctrl+S
        [VSTOP] = 'Q' & 0x1F  // Ctrl+Q
    },
    .c_ispeed = B9600,
    .c_ospeed = B9600
};

// Process integration functions
// These would integrate with the process management system
i32 tty_open_by_driver(u32 major, u32 minor) {
    // Open TTY by driver major/minor numbers
    tty_driver_t* driver = tty_drivers;
    
    while (driver) {
        if (driver->major == major) {
            if (minor < driver->num && driver->ttys && driver->ttys[minor]) {
                return 0;  // Success
            }
            break;
        }
        driver = driver->next;
    }
    
    return ERR_NOT_FOUND;
}

void tty_handle_output_interrupt(tty_t* tty) {
    if (!tty) return;
    
    // Continue transmission from output buffer
    if (tty->output_head != tty->output_tail && tty->put_char) {
        u32 idx = tty->output_head % TTY_OUTPUT_BUFFER;
        tty->put_char(tty, tty->output_buffer[idx]);
        tty->output_head++;
    }
}

// Terminal multiplexing
// Create multiple TTY devices for different terminals
static tty_t* console_ttys[32];  // Support up to 32 virtual consoles
static u32 num_console_ttys = 0;

i32 tty_create_console(u32 console_id) {
    if (console_id >= 32 || num_console_ttys >= 32) {
        return ERR_INVALID;
    }
    
    // Create a new console TTY
    tty_t* tty = tty_allocate_driver(1, 0);
    if (!tty) {
        return ERR_NO_MEMORY;
    }
    
    // Set up console-specific configuration
    snprintf(tty->name, MAX_STRING_LEN, "tty%d", console_id);
    tty->minor = console_id;
    tty->termios = tty_def_termios;
    tty->termios.c_cflag |= B38400;  // Higher baud for console
    
    // Register with VFS
    vfs_create_device_node(tty->name, S_IFCHR | 0660, tty->major, console_id);
    
    console_ttys[num_console_ttys++] = tty;
    
    console_print("Created console TTY: ");
    console_print(tty->name);
    console_print(" (console ");
    console_print_dec(console_id);
    console_print(")\n");
    
    return 0;
}

i32 tty_create_console_alias(const char* alias, u32 console_id) {
    if (console_id >= num_console_ttys || !console_ttys[console_id]) {
        return ERR_INVALID;
    }
    
    // Create alias device node
    vfs_create_device_node(alias, S_IFCHR | 0660, console_ttys[console_id]->major, console_id);
    
    console_print("Created console alias: ");
    console_print(alias);
    console_print(" -> tty");
    console_print_dec(console_id);
    console_print("\n");
    
    return 0;
}

// Initialize terminal multiplexing
void tty_multiplex_init(void) {
    console_print("Initializing TTY multiplexing...\n");
    
    // Create default consoles
    tty_create_console(0);  // /dev/tty0 (main console)
    tty_create_console(1);  // /dev/tty1
    
    // Create console aliases
    tty_create_console_alias("/dev/console", 0);
    tty_create_console_alias("/dev/tty", 0);
    
    console_print("TTY multiplexing initialized: ");
    console_print_dec(num_console_ttys);
    console_print(" consoles created\n");
}

// Session and process group management integration
// These would integrate with the process management system
void tty_session_init(void) {
    console_print("Initializing TTY session management...\n");
    
    // Set up default session for init process
    // This would be called when init starts
}

i32 tty_assign_controlling_terminal(u32 session_leader_pid, u32 tty_id) {
    if (tty_id >= num_console_ttys || !console_ttys[tty_id]) {
        return ERR_INVALID;
    }
    
    tty_t* tty = console_ttys[tty_id];
    tty_session_set_leader(tty, session_leader_pid);
    
    console_print("Assigned controlling terminal tty");
    console_print_dec(tty_id);
    console_print(" to session leader PID ");
    console_print_dec(session_leader_pid);
    console_print("\n");
    
    return 0;
}

i32 tty_set_foreground_process_group(u32 pgrp, u32 tty_id) {
    if (tty_id >= num_console_ttys || !console_ttys[tty_id]) {
        return ERR_INVALID;
    }
    
    tty_t* tty = console_ttys[tty_id];
    tty_foreground_set_group(tty, pgrp);
    
    console_print("Set foreground process group ");
    console_print_dec(pgrp);
    console_print(" for tty");
    console_print_dec(tty_id);
    console_print("\n");
    
    return 0;
}

// Signal delivery integration
// These functions would deliver signals to process groups
void tty_signal_delivery(tty_t* tty, i32 signal, u32 process_group) {
    console_print("Delivering signal ");
    console_print_dec(signal);
    console_print(" to process group ");
    console_print_dec(process_group);
    console_print(" on TTY ");
    console_print(tty->name);
    console_print("\n");
    
    // This would integrate with the process signal delivery system
    // send_signal_to_process_group(process_group, signal);
}

void tty_signal_stop(tty_t* tty) {
    if (tty->foreground_group > 0) {
        tty_signal_delivery(tty, SIGTSTP, tty->foreground_group);
    }
}

// Register default line disciplines
static void tty_register_default_ldiscs(void) {
    // Register N_TTY (standard line discipline)
    static tty_ldisc_t n_tty_ldisc = {
        .magic = 0x5402,
        .name = "n_tty",
        .num = N_TTY,
        .receive_buf = n_tty_receive_buf,
        .receive_room = n_tty_receive_room,
        .write_wakeup = n_tty_write_wakeup,
        .close = n_tty_close,
        .ioctl = n_tty_ioctl,
        .ops_data = NULL
    };
    
    tty_register_ldisc(&n_tty_ldisc);
}

// TTY Driver operations
i32 tty_register_driver(tty_driver_t* driver) {
    if (!driver || !driver->name || driver->major == 0) {
        return ERR_INVALID;
    }
    
    driver->ttys = (tty_t**)malloc(sizeof(tty_t*) * driver->num);
    if (!driver->ttys) {
        return ERR_NO_MEMORY;
    }
    
    // Initialize all TTY devices for this driver
    for (u32 i = 0; i < driver->num; i++) {
        driver->ttys[i] = NULL;
    }
    
    // Add driver to global list
    driver->next = tty_drivers;
    tty_drivers = driver;
    
    console_print("Registered TTY driver: ");
    console_print(driver->name);
    console_print(" (major=");
    console_print_dec(driver->major);
    console_print(", lines=");
    console_print_dec(driver->num);
    console_print(")\n");
    
    return 0;
}

i32 tty_unregister_driver(tty_driver_t* driver) {
    if (!driver) {
        return ERR_INVALID;
    }
    
    // Remove from global list
    if (tty_drivers == driver) {
        tty_drivers = driver->next;
    } else {
        tty_driver_t* prev = tty_drivers;
        while (prev && prev->next != driver) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = driver->next;
        }
    }
    
    // Free allocated resources
    if (driver->ttys) {
        free(driver->ttys);
    }
    
    return 0;
}

// TTY device allocation
tty_t* tty_allocate_driver(u32 lines, u32 major) {
    tty_t* tty;
    
    tty = (tty_t*)malloc(sizeof(tty_t) * lines);
    if (!tty) {
        return NULL;
    }
    
    // Initialize TTY devices
    for (u32 i = 0; i < lines; i++) {
        tty_t* t = &tty[i];
        
        snprintf(t->name, MAX_STRING_LEN, "tty%d", tty_line_count++);
        t->major = major;
        t->minor = i;
        t->driver = NULL;
        t->ldisc = NULL;
        t->open = NULL;
        t->close = NULL;
        t->write = NULL;
        t->put_char = NULL;
        t->write_room = NULL;
        t->set_termios = NULL;
        t->stop = NULL;
        t->start = NULL;
        t->hangup = NULL;
        t->driver_data = NULL;
        t->flags = 0;
        t->capabilities = 0;
        t->input_head = 0;
        t->input_tail = 0;
        t->output_head = 0;
        t->output_tail = 0;
        t->termios = tty_def_termios;
        t->winsize.ws_row = 24;
        t->winsize.ws_col = 80;
        t->session_leader = 0;
        t->foreground_group = 0;
        t->process_group = 0;
        t->have_signals = true;
        t->ref_count = 0;
        t->next = tty_list;
        
        tty_list = t;
    }
    
    return tty;
}

i32 tty_register_device(tty_t* tty, u32 minor) {
    if (!tty || minor >= tty->driver->num) {
        return ERR_INVALID;
    }
    
    // Register with device subsystem
    i32 major = device_register(tty->name, DEVICE_CHAR, NULL, tty);
    if (major < 0) {
        return major;
    }
    
    tty->major = major;
    return 0;
}

i32 tty_unregister_device(tty_t* tty, u32 minor) {
    // Device unregistration would go here
    return 0;
}

// Line discipline registration
i32 tty_register_ldisc(tty_ldisc_t* ldisc) {
    if (!ldisc || !ldisc->name || ldisc->num >= 16) {
        return ERR_INVALID;
    }
    
    // Check for duplicate
    tty_ldisc_t* existing = tty_ldiscs;
    while (existing) {
        if (existing->num == ldisc->num || strcmp(existing->name, ldisc->name) == 0) {
            return ERR_INVALID;
        }
        existing = existing->next;
    }
    
    // Add to global list
    ldisc->next = tty_ldiscs;
    tty_ldiscs = ldisc;
    
    console_print("Registered line discipline: ");
    console_print(ldisc->name);
    console_print(" (num=");
    console_print_dec(ldisc->num);
    console_print(")\n");
    
    return 0;
}

tty_ldisc_t* tty_get_ldisc(u32 ldisc_num) {
    tty_ldisc_t* ldisc = tty_ldiscs;
    
    while (ldisc && ldisc->num != ldisc_num) {
        ldisc = ldisc->next;
    }
    
    return ldisc;
}

i32 tty_set_ldisc(tty_t* tty, u32 ldisc_num) {
    if (!tty) {
        return ERR_INVALID;
    }
    
    tty_ldisc_t* ldisc = tty_get_ldisc(ldisc_num);
    if (!ldisc) {
        return ERR_INVALID;
    }
    
    // Close old line discipline
    if (tty->ldisc && tty->ldisc->close) {
        tty->ldisc->close(tty);
    }
    
    // Set new line discipline
    tty->ldisc = ldisc;
    
    // Initialize new line discipline
    if (ldisc->ioctl) {
        // Initial setup if needed
    }
    
    return 0;
}

// TTY operations
i32 tty_write(tty_t* tty, const u8* buf, u32 count) {
    if (!tty || !buf || count == 0) {
        return ERR_INVALID;
    }
    
    // Check write room
    u32 room = tty_write_room(tty);
    if (room == 0) {
        return 0;
    }
    
    count = (count < room) ? count : room;
    
    // Copy to output buffer
    for (u32 i = 0; i < count; i++) {
        u32 idx = tty->output_tail % TTY_OUTPUT_BUFFER;
        tty->output_buffer[idx] = buf[i];
        tty->output_tail++;
    }
    
    // Echo if enabled
    if (tty->termios.c_lflag & ECHO) {
        tty_echo_chars(tty, buf, count);
    }
    
    // Call driver write function if available
    if (tty->write && count > 0) {
        tty->write(tty, buf, count);
    }
    
    return count;
}

i32 tty_put_char(tty_t* tty, u8 ch) {
    return tty_write(tty, &ch, 1);
}

u32 tty_write_room(tty_t* tty) {
    if (!tty) {
        return 0;
    }
    
    return TTY_OUTPUT_BUFFER - (tty->output_tail - tty->output_head);
}

void tty_handle_input_interrupt(tty_t* tty, u8 byte) {
    if (!tty) {
        return;
    }
    
    // Add to input buffer
    if ((tty->input_tail - tty->input_head) < TTY_INPUT_BUFFER) {
        u32 idx = tty->input_tail % TTY_INPUT_BUFFER;
        tty->input_buffer[idx] = byte;
        tty->input_tail++;
    }
    
    // Echo if enabled
    if (tty->termios.c_lflag & ECHO) {
        tty_echo_char(tty, byte);
    }
    
    // Line discipline processing
    if (tty->ldisc && tty->ldisc->receive_buf) {
        tty->ldisc->receive_buf(tty, &byte, NULL, 1);
    }
    
    // Control character handling
    if (tty->termios.c_lflag & ISIG) {
        if (byte == tty->termios.c_cc[VINTR]) {
            tty_signal_intr(tty, SIGINT);
        } else if (byte == tty->termios.c_cc[VQUIT]) {
            tty_signal_quit(tty);
        } else if (byte == tty->termios.c_cc[VSUSP]) {
            tty_signal_susp(tty);
        }
    }
}

void tty_signal_intr(tty_t* tty, i32 signal) {
    console_print("TTY: Interrupt signal (");
    console_print_dec(signal);
    console_print(") received\n");
    // Signal delivery would be implemented here
}

void tty_signal_quit(tty_t* tty) {
    console_print("TTY: Quit signal received\n");
    // Signal delivery would be implemented here
}

void tty_signal_susp(tty_t* tty) {
    console_print("TTY: Suspend signal received\n");
    // Signal delivery would be implemented here
}

// Termios operations
i32 tty_get_termios(tty_t* tty, struct termios* termios) {
    if (!tty || !termios) {
        return ERR_INVALID;
    }
    
    *termios = tty->termios;
    return 0;
}

i32 tty_set_termios(tty_t* tty, struct termios* termios) {
    if (!tty || !termios) {
        return ERR_INVALID;
    }
    
    tty->termios = *termios;
    
    // Call hardware-specific setter if available
    if (tty->set_termios) {
        return tty->set_termios(tty, termios);
    }
    
    return 0;
}

// Window size operations
i32 tty_get_winsize(tty_t* tty, struct winsize* ws) {
    if (!tty || !ws) {
        return ERR_INVALID;
    }
    
    *ws = tty->winsize;
    return 0;
}

i32 tty_set_winsize(tty_t* tty, struct winsize* ws) {
    if (!tty || !ws) {
        return ERR_INVALID;
    }
    
    tty->winsize = *ws;
    return 0;
}

// Process group operations
i32 tty_set_process_group(tty_t* tty, u32 pgrp) {
    if (!tty) {
        return ERR_INVALID;
    }
    
    tty->process_group = pgrp;
    return 0;
}

i32 tty_get_process_group(tty_t* tty, u32* pgrp) {
    if (!tty || !pgrp) {
        return ERR_INVALID;
    }
    
    *pgrp = tty->process_group;
    return 0;
}

i32 tty_check_data_ready(tty_t* tty) {
    if (!tty) {
        return 0;
    }
    
    return (tty->input_tail > tty->input_head) ? 1 : 0;
}

void tty_set_flags(tty_t* tty, u32 flags) {
    if (tty) {
        tty->flags |= flags;
    }
}

void tty_clear_flags(tty_t* tty, u32 flags) {
    if (tty) {
        tty->flags &= ~flags;
    }
}

// Utility functions
i32 tty_char_is_control(u8 ch) {
    return (ch < 0x20 || ch == 0x7F) ? 1 : 0;
}

i32 tty_char_is_printable(u8 ch) {
    return (ch >= 0x20 && ch != 0x7F) ? 1 : 0;
}

void tty_echo_char(tty_t* tty, u8 ch) {
    if (!tty || !(tty->termios.c_lflag & ECHO)) {
        return;
    }
    
    if (tty_char_is_control(ch)) {
        // Echo control characters as ^X
        if (tty->termios.c_lflag & ECHOCTL) {
            u8 ctrl = (ch == 0x7F) ? '?' : (ch + '@');
            if (tty->put_char) {
                tty->put_char(tty, '^');
                tty->put_char(tty, ctrl);
            }
        }
    } else {
        if (tty->put_char) {
            tty->put_char(tty, ch);
        }
    }
}

void tty_echo_chars(tty_t* tty, const u8* buf, u32 count) {
    if (!tty || !buf || !(tty->termios.c_lflag & ECHO)) {
        return;
    }
    
    for (u32 i = 0; i < count; i++) {
        tty_echo_char(tty, buf[i]);
    }
}

// Line discipline: N_TTY implementation
i32 n_tty_receive_buf(tty_t* tty, const u8* buf, const u8* flags, u32 count) {
    // For now, just process the data
    // Full implementation would handle canonical/raw modes
    for (u32 i = 0; i < count; i++) {
        if (tty_char_is_control(buf[i]) && (tty->termios.c_lflag & ICANON)) {
            // Handle special characters in canonical mode
            if (buf[i] == tty->termios.c_cc[VINTR] ||
                buf[i] == tty->termios.c_cc[VQUIT] ||
                buf[i] == tty->termios.c_cc[VSUSP] ||
                buf[i] == tty->termios.c_cc[VEOF]) {
                // These are handled in tty_handle_input_interrupt
                continue;
            }
        }
    }
    
    return count;
}

i32 n_tty_receive_room(tty_t* tty) {
    return TTY_INPUT_BUFFER - (tty->input_tail - tty->input_head);
}

i32 n_tty_write_wakeup(tty_t* tty) {
    // Wake up processes waiting for write room
    return 0;
}

i32 n_tty_close(tty_t* tty) {
    // Clean up line discipline state
    return 0;
}

i32 n_tty_ioctl(tty_t* tty, i32 cmd, u64 arg) {
    // Line discipline specific ioctls
    return 0;
}