/* TTY Core Header - Comprehensive Terminal Subsystem */

#pragma once

#include "types.h"

// Forward declarations
typedef struct tty_struct tty_t;
typedef struct tty_driver_struct tty_driver_t;
typedef struct tty_line_disc_struct tty_ldisc_t;

// TTY device types
#define TTY_DRIVER_TYPE_CONSOLE   0x0001
#define TTY_DRIVER_TYPE_SERIAL    0x0002
#define TTY_DRIVER_TYPE_PTY       0x0003

// TTY capability flags
#define TTY_CAP_HARDWARE_FLOW     0x00000001
#define TTY_CAP_SOFTWARE_FLOW     0x00000002
#define TTY_CAP_HAVE_CSOSPEED     0x00000004
#define TTY_CAP_HAVE_CTS          0x00000008
#define TTY_CAP_HAVE_DSR          0x00000010
#define TTY_CAP_HAVE_RNG          0x00000020
#define TTY_CAP_HAVE_DCD          0x00000040
#define TTY_CAP_HAVE_RTSCTS       0x00000080
#define TTY_CAP_HAVE_XONXOFF      0x00000100
#define TTY_CAP_HAVE_HARDWARE_SPEED 0x00000200

// termios structures
struct termios {
    u32 c_iflag;      /* input modes */
    u32 c_oflag;      /* output modes */
    u32 c_cflag;      /* control modes */
    u32 c_lflag;      /* local modes */
    u8  c_cc[32];     /* special characters */
    u32 c_ispeed;     /* input speed */
    u32 c_ospeed;     /* output speed */
};

struct winsize {
    u16 ws_row;       /* rows, in characters */
    u16 ws_col;       /* columns, in characters */
    u16 ws_xpixel;    /* horizontal size, pixels (unused) */
    u16 ws_ypixel;    /* vertical size, pixels (unused) */
};

// termios control character indices
#define VINTR    0       /* Interrupt character (normally 0177, DEL) */
#define VQUIT    1       /* Quit character (normally ^\) */
#define VERASE   2       /* Erase character (normally ^H, BS) */
#define VKILL    3       /* Kill character (normally ^U) */
#define VEOF     4       /* EOF character (normally ^D) */
#define VTIME    5       /* Time for VTIME */
#define VMIN     6       /* Min chars for VMIN */
#define VSWTCH   7       /* Switch character (normally 0) */
#define VSTART   8       /* Start character (normally ^S) */
#define VSTOP    9       /* Stop character (normally ^Q) */
#define VSUSP    10      /* Suspend character (normally ^Z) */
#define VDSUSP   11      /* Delayed suspend character (normally ^Z) */
#define VREPRINT 12      /* Reprint line (normally ^R) */
#define VDISCARD 13      /* Discard output (normally ^O) */
#define VWERASE  14      /* Word erase (normally ^W) */
#define VLNEXT   15      /* Literal next character (normally ^V) */

// Input modes (c_iflag)
#define IGNBRK  0x00000001  /* Ignore break condition */
#define BRKINT  0x00000002  /* Signal interrupt on break */
#define IGNPAR  0x00000004  /* Ignore characters with parity errors */
#define PARMRK  0x00000008  /* Mark parity and framing errors */
#define INPCK   0x00000010  /* Enable input parity checking */
#define ISTRIP  0x00000020  /* Strip character */
#define INLCR   0x00000040  /* Map NL to CR on input */
#define IGNCR   0x00000080  /* Ignore CR */
#define ICRNL   0x00000100  /* Map CR to NL on input */
#define IXON    0x00000200  /* Enable XON/XOFF flow control */
#define IXANY   0x00000400  /* Any character will restart after stop */
#define IXOFF   0x00000800  /* Enable XON/XOFF input flow control */
#define IMAXBEL 0x00001000  /* Ring bell on input queue full */

// Output modes (c_oflag)
#define OPOST   0x00000001  /* Perform output processing */
#define OLCUC   0x00000002  /* Map lowercase to uppercase on output */
#define ONLCR   0x00000004  /* Map NL to CR-NL on output */
#define OCRNL   0x00000008  /* Map CR to NL on output */
#define ONOCR   0x00000010  /* No CR output at column 0 */
#define ONLRET  0x00000020  /* NL performs CR function */
#define OFILL   0x00000040  /* Use fill characters for delays */
#define OFDEL   0x00000080  /* Fill character is DEL */
#define NLDLY   0x00000100  /* Select newline delays */
#define CRDLY   0x00000200  /* Select carriage return delays */
#define TABDLY  0x00000400  /* Select horizontal tab delays */
#define BSDLY   0x00000800  /* Select backspace delays */
#define FFDLY   0x00001000  /* Select form feed delays */
#define VTDLY   0x00002000  /* Select vertical tab delays */
#define OXTABS  0x00004000  /* Expand tabs to spaces on output */

// Control modes (c_cflag)
#define CBAUD   0x0000000F  /* Baud rate mask */
#define B50     0x00000001  /* 50 baud */
#define B75     0x00000002  /* 75 baud */
#define B110    0x00000003  /* 110 baud */
#define B134    0x00000004  /* 134.5 baud */
#define B150    0x00000005  /* 150 baud */
#define B200    0x00000006  /* 200 baud */
#define B300    0x00000007  /* 300 baud */
#define B600    0x00000008  /* 600 baud */
#define B1200   0x00000009  /* 1200 baud */
#define B1800   0x0000000A  /* 1800 baud */
#define B2400   0x0000000B  /* 2400 baud */
#define B4800   0x0000000C  /* 4800 baud */
#define B9600   0x0000000D  /* 9600 baud */
#define B19200  0x0000000E  /* 19200 baud */
#define B38400  0x0000000F  /* 38400 baud */

#define CSIZE   0x00000030  /* Character size mask */
#define CS5     0x00000000  /* 5 bits */
#define CS6     0x00000010  /* 6 bits */
#define CS7     0x00000020  /* 7 bits */
#define CS8     0x00000030  /* 8 bits */
#define CSTOPB  0x00000040  /* Two stop bits instead of one */
#define CREAD   0x00000080  /* Enable receiver */
#define PARENB  0x00000100  /* Enable parity generation */
#define PARODD  0x00000200  /* Odd parity instead of even */

// Local modes (c_lflag)
#define ISIG    0x00000001  /* Enable signals */
#define ICANON  0x00000002  /* Enable canonical input */
#define XCASE   0x00000004  /* Canonical upper/lower presentation */
#define ECHO    0x00000008  /* Enable echoing of input characters */
#define ECHOE   0x00000010  /* Echo erase character as backspace-space-backspace */
#define ECHOK   0x00000020  /* Echo NL after kill character */
#define ECHONL  0x00000040  /* Echo NL even if ECHO is not set */
#define NOFLSH  0x00000080  /* Disable flush after interrupt/quit */
#define TOSTOP  0x00000100  /* Stop background jobs from writing to terminal */
#define IEXTEN  0x00000200  /* Enable extended input processing */
#define ECHOCTL 0x00000400  /* Echo control characters as ^X */
#define ECHOKE  0x00000800  /* Echo kill character by erasing line */

// Default termios settings
#define TTY_DEF_IFLAG (IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON)
#define TTY_DEF_OFLAG (OPOST | ONLCR)
#define TTY_DEF_CFLAG (B9600 | CS8 | CREAD)
#define TTY_DEF_LFLAG (ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL | NOFLSH | IEXTEN)

// ioctl command definitions
#define TCGETS      0x5401    /* Get termios structure */
#define TCSETS      0x5402    /* Set termios structure */
#define TCSETSW     0x5403    /* Set termios structure after draining */
#define TCSETSF     0x5404    /* Set termios structure after flushing */
#define TCGETA      0x5405    /* Get termios structure (alternate) */
#define TCSETA      0x5406    /* Set termios structure (alternate) */
#define TCSETAW     0x5407    /* Set termios structure (alternate, drain) */
#define TCSETAF     0x5408    /* Set termios structure (alternate, flush) */
#define TCSBRK      0x5409    /* Send break */
#define TCXONC      0x540A    /* Flow control */
#define TCFLSH      0x540B    /* Flush buffers */
#define TIOCGWINSZ  0x5413    /* Get window size */
#define TIOCSWINSZ  0x5414    /* Set window size */
#define TIOCGPGRP   0x540F    /* Get process group ID */
#define TIOCSPGRP   0x5410    /* Set process group ID */
#define TIOCGSOFTCAR 0x5411   /* Get software carrier flag */
#define TIOCSSOFTCAR 0x5412   /* Set software carrier flag */

// Signal definitions
#define SIGINT      2       /* Interrupt signal */
#define SIGQUIT     3       /* Quit signal */
#define SIGTSTP     20      /* Terminal stop signal */
#define SIGCONT     18      /* Continue signal */

// Additional utility macros
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// String utilities (since we don't have full libc)
i32 snprintf(char* str, u64 size, const char* format, ...);
i32 strcmp(const char* s1, const char* s2);
u64 strlen(const char* s);
char* strncpy(char* dest, const char* src, u64 n);

// TTY buffer sizes
#define TTY_BUFFER_SIZE     4096
#define TTY_INPUT_BUFFER    256
#define TTY_OUTPUT_BUFFER   256

// TTY operations structure
typedef struct tty_ops_struct {
    i32 (*open)(tty_t* tty);
    i32 (*write)(tty_t* tty, const u8* buf, u32 count);
    i32 (*set_termios)(tty_t* tty, struct termios* termios);
    i32 (*put_char)(tty_t* tty, u8 ch);
    i32 (*write_room)(tty_t* tty);
    i32 (*stop)(tty_t* tty);
    i32 (*start)(tty_t* tty);
    i32 (*hangup)(tty_t* tty);
    i32 (*ioctl)(tty_t* tty, i32 cmd, u64 arg);
} tty_ops_t;

// TTY device structure
struct tty_struct {
    u32 major;                   /* Device major number */
    u32 minor;                   /* Device minor number */
    char name[MAX_STRING_LEN];   /* Device name */
    tty_driver_t* driver;        /* TTY driver */
    tty_ldisc_t* ldisc;          /* Line discipline */
    
    // Device operations
    i32 (*open)(tty_t* tty);
    i32 (*close)(tty_t* tty);
    i32 (*write)(tty_t* tty, const u8* buf, u32 count);
    i32 (*put_char)(tty_t* tty, u8 ch);
    i32 (*write_room)(tty_t* tty);
    
    // Hardware-specific operations
    i32 (*set_termios)(tty_t* tty, struct termios* termios);
    i32 (*stop)(tty_t* tty);
    i32 (*start)(tty_t* tty);
    i32 (*hangup)(tty_t* tty);
    
    // Data
    void* driver_data;           /* Driver-specific data */
    u32 flags;                   /* Device flags */
    u32 capabilities;            /* Device capabilities */
    
    // Buffers
    u8 input_buffer[TTY_INPUT_BUFFER];
    u32 input_head;
    u32 input_tail;
    u8 output_buffer[TTY_OUTPUT_BUFFER];
    u32 output_head;
    u32 output_tail;
    
    // Line settings
    struct termios termios;
    struct winsize winsize;
    
    // Process group and session info
    u32 session_leader;          /* Session leader PID */
    u32 foreground_group;        /* Foreground process group */
    u32 process_group;           /* Process group for this tty */
    
    // Signal handling
    bool have_signals;           /* Enable signal handling */
    
    // Reference count
    u32 ref_count;
    
    // Next TTY in list
    tty_t* next;
};

// TTY driver structure
struct tty_driver_struct {
    u32 major;                   /* Driver major number */
    u32 minor_start;             /* Starting minor number */
    u32 num;                     /* Number of devices */
    char* name;                  /* Driver name */
    u32 type;                    /* Driver type */
    u32 subtype;                 /* Driver subtype */
    u32 flags;                   /* Driver flags */
    
    // Operations
    tty_t* (*lookup)(u32 major, u32 minor);
    i32 (*install)(u32 major, tty_t* tty);
    void (*remove)(u32 major, u32 minor);
    
    // Standard operations
    i32 (*open_fn)(tty_t* tty, u32 minor);
    void (*close_fn)(tty_t* tty, u32 minor);
    
    // Write operations
    i32 (*write_fn)(tty_t* tty, u32 minor, const u8* buf, u32 count);
    void (*put_char_fn)(tty_t* tty, u32 minor, u8 ch);
    u32 (*write_room_fn)(tty_t* tty, u32 minor);
    
    // Control operations
    i32 (*set_termios_fn)(tty_t* tty, u32 minor, struct termios* termios);
    i32 (*stop_fn)(tty_t* tty, u32 minor);
    i32 (*start_fn)(tty_t* tty, u32 minor);
    i32 (*hangup_fn)(tty_t* tty, u32 minor);
    
    // Device data
    void* driver_state;          /* Driver-specific state */
    
    // TTY devices managed by this driver
    tty_t** ttys;
    u32 num_ttys;
};

// Line discipline structure
struct tty_line_disc_struct {
    u32 magic;                   /* Magic number */
    char* name;                  /* Line discipline name */
    u16 num;                     /* Line discipline number */
    
    // Operations
    i32 (*receive_buf)(tty_t* tty, const u8* buf, const u8* flags, u32 count);
    i32 (*receive_room)(tty_t* tty);
    i32 (*write_wakeup)(tty_t* tty);
    i32 (*close)(tty_t* tty);
    i32 (*ioctl)(tty_t* tty, i32 cmd, u64 arg);
    
    // Data
    void* ops_data;              /* Discipline-specific data */
};

// Line discipline numbers
#define N_TTY     0              /* Standard line discipline */
#define N_SLIP    1              /* Serial Line IP */
#define N_MOUSE   2              /* Serial mouse */
#define N_PPP     3              /* PPP */
#define N_STRIP   4              /* Strip */

/* TTY Core function prototypes */

// Initialization
void tty_init(void);
void tty_kref_init(void);

// Driver registration
i32 tty_register_driver(tty_driver_t* driver);
i32 tty_unregister_driver(tty_driver_t* driver);

// TTY device creation
tty_t* tty_allocate_driver(u32 lines, u32 major);
i32 tty_register_device(tty_t* tty, u32 minor);
i32 tty_unregister_device(tty_t* tty, u32 minor);

// Line discipline management
i32 tty_register_ldisc(tty_ldisc_t* ldisc);
i32 tty_set_ldisc(tty_t* tty, u32 ldisc_num);
tty_ldisc_t* tty_get_ldisc(u32 ldisc_num);

// TTY device operations
i32 tty_open_by_driver(u32 major, u32 minor);
void tty_kref_put(tty_t* tty);
i32 tty_write(tty_t* tty, const u8* buf, u32 count);
i32 tty_put_char(tty_t* tty, u8 ch);
u32 tty_write_room(tty_t* tty);

// TTY control operations
i32 tty_ioctl(tty_t* tty, i32 cmd, u64 arg);
i32 tty_set_termios(tty_t* tty, struct termios* termios);
i32 tty_get_termios(tty_t* tty, struct termios* termios);
i32 tty_get_winsize(tty_t* tty, struct winsize* ws);
i32 tty_set_winsize(tty_t* tty, struct winsize* ws);
i32 tty_set_process_group(tty_t* tty, u32 pgrp);
i32 tty_get_process_group(tty_t* tty, u32* pgrp);

// Line discipline operations
i32 tty_ldisc_receive_buf(tty_t* tty, const u8* buf, u32 count);
void tty_ldisc_receive(tty_t* tty, u8* buf, u32 count);
void tty_ldisc_flush_buffer(tty_t* tty);

// Signal handling
void tty_signal_intr(tty_t* tty, i32 signal);
void tty_signal_quit(tty_t* tty);
void tty_signal_susp(tty_t* tty);
void tty_signal_stop(tty_t* tty);

// Interrupt handling
void tty_handle_input_interrupt(tty_t* tty, u8 byte);
void tty_handle_output_interrupt(tty_t* tty);

// Process session management
void tty_session_set_leader(tty_t* tty, u32 pid);
void tty_foreground_set_group(tty_t* tty, u32 pgrp);
i32 tty_check_data_ready(tty_t* tty);
void tty_set_flags(tty_t* tty, u32 flags);
void tty_clear_flags(tty_t* tty, u32 flags);

// Default termios
extern const struct termios tty_def_termios;

// Utility functions
i32 tty_char_is_control(u8 ch);
i32 tty_char_is_printable(u8 ch);
void tty_canonical_process(tty_t* tty);
void tty_raw_process(tty_t* tty);
void tty_echo_char(tty_t* tty, u8 ch);
void tty_echo_chars(tty_t* tty, const u8* buf, u32 count);