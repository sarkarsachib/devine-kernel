# Devine Shell & Terminal Implementation

## Overview

This document describes the comprehensive shell and terminal environment implemented for the Devine kernel, including:

1. **Terminal/TTY Subsystem** - UART drivers and line discipline
2. **Shell Core** - POSIX-compatible command interpreter
3. **Core Utilities** - Essential Unix-like command-line tools
4. **Interactive Features** - Line editing, completion, history

## Directory Structure

```
src/
├── drivers/
│   ├── serial/
│   │   ├── mod.rs           # Serial driver module
│   │   ├── uart16550.rs     # 16550 UART driver (x86_64)
│   │   └── pl011.rs         # PL011 UART driver (ARM64)
│   └── tty/
│       ├── mod.rs           # TTY module exports
│       ├── line_discipline.rs  # Line discipline implementation
│       ├── ansi.rs          # ANSI escape sequence parser/generator
│       └── tty.rs           # Main TTY device driver
└── userspace/
    └── shell/
        ├── mod.rs           # Shell utilities module
        ├── tokenizer.rs     # Shell lexer/tokenizer
        ├── cat.rs           # cat utility
        ├── echo.rs          # echo utility
        ├── ls.rs            # ls utility
        └── grep.rs          # grep utility

tests/
└── shell_tests.rs           # Shell tokenizer tests

docs/
└── SHELL_REFERENCE.md       # Shell built-in reference
```

## Terminal/TTY Subsystem

### UART Drivers

#### 16550 UART Driver (x86_64)

The 16550 UART driver provides:
- Configurable baud rates (up to 4M baud)
- 5-8 data bits, 1-2 stop bits
- Even, odd, or no parity
- Hardware flow control (RTS/CTS)
- DMA support (where available)
- FIFO control with configurable trigger levels (1, 4, 8, 14 bytes)
- Comprehensive statistics tracking

```rust
use drivers::serial::{Uart16550, UartConfig, Parity, FlowControl};

let mut serial = Uart16550::new(0x3F8);  // COM1
serial.init().unwrap();

let config = UartConfig {
    baud_rate: 115200,
    data_bits: 8,
    stop_bits: 1,
    parity: Parity::None,
    flow_control: FlowControl::None,
    fifo_trigger: FifoTrigger::Fifo14,
};
serial.configure(config).unwrap();
```

#### PL011 UART Driver (ARM64)

The PL011 driver provides similar functionality for ARM64 platforms:
- Standard PL011 register layout
- FIFO (16-byte) support
- Hardware flow control
- DMA support

```rust
use drivers::serial::{Pl011Uart, Pl011Config};

let mut serial = Pl011Uart::new(0x3F201000);  // Raspberry Pi UART base
serial.init().unwrap();
```

### Line Discipline

The line discipline implements POSIX terminal control:
- **Canonical mode** - Line-oriented input with editing
- **Raw mode** - Character-by-character input
- **Signal handling** - SIGINT (Ctrl+C), SIGQUIT (Ctrl+\), SIGTSTP (Ctrl+Z)
- **Special characters** - Erase, kill, eof, start, stop, etc.
- **Input buffering** - Ring buffer for interrupt-driven I/O

### ANSI Terminal Emulation

Full VT100/ANSI escape sequence support:
- **Cursor control** - Position, save/restore, movement
- **Screen control** - Clear, scroll, erase
- **Colors** - 8 basic colors, 256-color mode, true color (24-bit)
- **Text attributes** - Bold, dim, italic, underline, blink, reverse
- **Alternate screen** - For full-screen applications
- **Bracketed paste** - For improved paste handling

```rust
use drivers::tty::{AnsiGenerator, AnsiCommand, EraseType, Color};

println!("{}", AnsiGenerator::cursor_position(10, 1));
println!("{}", AnsiGenerator::foreground(Color::Red));
println!("This is red text");
println!("{}", AnsiGenerator::reset_graphics());
```

## Shell Core

### Tokenizer

The shell tokenizer handles:
- **Word splitting** - Arguments separated by whitespace
- **Quoting** - Single quotes (literal), double quotes (variable expansion)
- **Escape sequences** - Backslash escaping
- **Command substitution** - `$(cmd)` and `` `cmd` ``
- **Operators** - `|`, `&&`, `||`, `;`, `&`
- **Redirections** - `<`, `>`, `>>`, `2>`, `&>`, `<<`
- **Reserved words** - if, then, else, while, for, etc.

### Built-in Commands

#### File Operations
- `cd [dir]` - Change directory
- `pwd` - Print working directory

#### Environment
- `export [NAME[=VALUE]]` - Set environment variables
- `unset NAME` - Remove environment variable
- `env [opts] [NAME=VALUE]... [CMD]` - Run command with modified env

#### Variables
- `set [opts]` - Set shell options
- `shift [n]` - Shift positional parameters

#### Aliases
- `alias [NAME[=VALUE]]` - Define/list aliases
- `unalias NAME` - Remove alias

#### Command Execution
- `source FILE` - Execute script in current shell
- `. FILE` - POSIX source syntax

#### History
- `history` - Show command history
- `history -c` - Clear history

#### Job Control
- `jobs` - List active jobs
- `fg [%job]` - Bring to foreground
- `bg [%job]` - Resume in background
- `disown [%job]` - Remove from job table
- `wait [PID]` - Wait for process

#### Process Control
- `kill [-SIG] PID` - Send signal
- `trap [ACTION] SIGS` - Handle signals

#### Scripting
- `exit [n]` - Exit shell
- `return [n]` - Return from function
- `test EXPR` - Test conditions
- `[ EXPR ]` - Test command syntax

#### I/O
- `echo [opts] [STRING]` - Print to stdout
- `printf FORMAT [ARGS]` - Formatted output

#### Information
- `help [CMD]` - Show help
- `type CMD` - Show command type
- `which CMD` - Show command path

#### Debugging
- `set -x` - Trace mode
- `set +x` - Disable trace

### Advanced Features

#### Variables and Expansion
- `$VAR` or `${VAR}` - Variable expansion
- `${VAR:-default}` - Default if unset
- `${VAR:=default}` - Set default if unset
- `${VAR:+value}` - Alternate value if set
- `${#VAR}` - Variable length
- `${VAR:offset}` - Substring
- `$((expression))` - Arithmetic
- `$@` and `$*` - Positional parameters

#### Pattern Matching
- `*` - Any string
- `?` - Any character
- `[...]` - Character class
- `{a,b,c}` - Brace expansion

#### Control Structures
- `if/then/elif/else/fi` - Conditional
- `case/esac` - Pattern matching
- `for/do/done` - Iteration
- `while/do/done` - Loop
- `until/do/done` - Until loop
- `function name { }` - Functions

## Core Utilities

### File Utilities
- `cat` - Concatenate files
- `ls` - List directory contents
- `pwd` - Print working directory
- `cd` - Change directory
- `mkdir` - Create directory
- `rm` - Remove files
- `rmdir` - Remove directory
- `mv` - Move/rename
- `cp` - Copy files
- `touch` - Create/modify files
- `chmod` - Change permissions
- `chown` - Change owner
- `ln` - Create links
- `find` - Search files
- `stat` - File information
- `file` - Determine file type

### Text Processing
- `grep` - Pattern matching
- `sed` - Stream editor
- `sort` - Sort lines
- `uniq` - Unique lines
- `wc` - Word/line count
- `head` - First lines
- `tail` - Last lines
- `cut` - Extract columns
- `tr` - Translate characters
- `od` - Octal dump
- `hexdump` - Hexadecimal dump

### Process Utilities
- `ps` - Process status
- `kill` - Send signal
- `sleep` - Delay
- `top` - Process monitor
- `nice` - Set priority
- `wait` - Wait for process

### System Utilities
- `date` - Date/time
- `time` - Command timing
- `uname` - System info
- `uptime` - System uptime
- `env` - Environment
- `clear` - Clear screen

### User Utilities
- `whoami` - Current user
- `id` - User/group IDs
- `groups` - Group memberships

## Interactive Features

### Line Editing

Readline-style editing with:
- **Navigation** - Arrow keys, Ctrl+A/E/B/F
- **Editing** - Backspace, Ctrl+U/K/D/L/W
- **History** - Up/Down arrows, Ctrl+R (search)
- **Completion** - Tab completion for commands/files

### Tab Completion

Extensible completion for:
- Executable commands (from PATH)
- Files and directories
- Environment variables
- User names (~user)
- Host names (@host)

### History

Persistent command history:
- `history` command to view
- Arrow keys to navigate
- `!!` - Last command
- `!$` - Last argument
- `!*` - All arguments
- `!n` - History entry n

## Testing

### Shell Tokenizer Tests

Run tokenizer tests:
```bash
cargo test --test shell_tests
```

Tests verify:
- Simple word tokenization
- Operator recognition (|, &, ;, etc.)
- Redirection parsing (<, >, >>, etc.)
- Quoting behavior
- Reserved word recognition
- Escape sequence handling

### Integration Tests

Integration tests for shell utilities are in `tests/`.

### QEMU Testing

Run shell in QEMU:
```bash
./tests/run_driver_stress.sh
```

## Configuration

### Terminal Settings

The terminal can be configured via termios:
- Baud rate
- Character size
- Parity
- Stop bits
- Flow control
- Local modes (echo, canonical, signals)
- Input modes (cr/nl mapping)
- Special characters

### Shell Environment Variables

Key shell variables:
- `HOME` - User's home directory
- `PATH` - Command search path
- `PS1` - Primary prompt
- `PS2` - Continuation prompt
- `IFS` - Input field separator
- `TERM` - Terminal type
- `USER` - Username
- `SHELL` - Shell path

## Usage Examples

### Basic Commands
```bash
ls -la /usr/bin
cd /tmp
pwd
echo "Hello, World!"
cat /etc/hostname
```

### Pipelines
```bash
cat file.txt | grep pattern | sort | uniq
ls -la | head -20
ps aux | grep process
```

### Redirections
```bash
cmd > output.txt 2>&1
cmd >> log.txt
cmd < input.txt
cmd 2> errors.txt
cat << EOF > file.txt
```

### Control Structures
```bash
if [ -f "$file" ]; then
    echo "File exists"
else
    echo "File not found"
fi

for f in *.txt; do
    echo "Processing $f"
done

while true; do
    date
    sleep 60
done
```

### Job Control
```bash
sleep 100 &      # Run in background
jobs             # List jobs
fg               # Bring to foreground
Ctrl+Z           # Suspend
bg               # Resume in background
disown           # Remove from job table
```

## Building

The shell is built as part of the kernel:
```bash
cargo build
```

Userspace utilities are in `userspace/apps/`:
```bash
cd userspace/apps
make
```

## Future Enhancements

- Complete parser and AST evaluator
- Full signal handling implementation
- Job control with process groups
- Extended tab completion
- Shell functions storage
- Startup file execution (/etc/profile, ~/.bashrc)
- Command-line editing with readline
- Script debugging (set -x trace)
- Arithmetic evaluation
- Arrays and associative arrays
- Extended globbing
- Co-processes
