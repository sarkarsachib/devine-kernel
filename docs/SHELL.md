# Shell Reference

The integrated shell provides a basic command-line interface for interacting with the Devine OS kernel.

## Built-in Commands

- `exit`: Terminates the shell session.
- `stress`: Launches the stress test utility (`/usr/bin/stress`).

## Features

- **Standard I/O**: Reads from standard input (serial port) and writes to standard output.
- **Process Management**: Uses `fork` and `exec` to launch commands.

## Startup Behavior

On boot, `init` launches `/bin/sh`. The shell prints a banner:
```
shell: ready. type 'exit' or 'stress'
```

## Troubleshooting

- **No Output**: Ensure your serial port is configured correctly (115200 baud, 8N1).
- **No Input**: If typing doesn't work, check if the terminal emulator is sending characters correctly. The shell uses polling on the serial port.

## Utilities

Bundled utilities in `/bin` and `/usr/bin`:
- `init`: Process ID 1, system initializer.
- `sh`: The shell.
- `cat`: Concatenate and print files (usage: `cat`).
- `ls`: List directory contents (usage: `ls`).
- `stress`: Stress test tool.
