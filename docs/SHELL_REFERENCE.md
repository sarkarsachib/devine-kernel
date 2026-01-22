# Devine Shell Reference

A POSIX-compatible shell for the Devine kernel with advanced features.

## Built-in Commands

### File Operations

#### cd [directory]
Change the current directory.
- `cd` without arguments changes to HOME directory
- `cd -` changes to previous directory
- Supports tilde expansion (~user/path)

#### pwd
Print the current working directory.

### Environment

#### export [NAME[=VALUE]...]
Set environment variables for child processes.
- `export` alone lists all environment variables
- `export NAME=value` sets and exports

#### unset NAME
Remove a variable from the environment.

#### env [OPTION]... [NAME=VALUE]...
Run a command in a modified environment.
- `-i` Start with empty environment
- `-u NAME` Remove variable from environment

### Aliases

#### alias [NAME[=VALUE]]
Define or display aliases.
- `alias` alone lists all aliases
- `alias name=value` creates an alias

#### unalias NAME
Remove an alias.

### Variables

#### set [OPTION]...
Set or unset shell options and positional parameters.
- `set -x` Enable trace mode (debug)
- `set +x` Disable trace mode
- `set -e` Exit on error

### Command Execution

#### source FILE [ARGUMENTS]
Read and execute commands from FILE in current shell.

#### . FILE [ARGUMENTS]
Same as source (POSIX syntax).

### History

#### history
Display command history.
- `history -c` Clear history

### Scripting

#### exit [n]
Exit the shell with status n (default: last command status).

#### return [n]
Return from a function with status n.

#### shift [n]
Shift positional parameters left by n.

#### test [EXPRESSION]
Evaluate expression (also available as `[ EXPRESSION ]`).

### Job Control

#### jobs
List active jobs.

#### fg [%job]
Bring job to foreground.

#### bg [%job]
Resume job in background.

#### disown [%job]
Remove job from job table.

#### wait [n]
Wait for process n and return its status.

### Process Control

#### kill [-s SIGNAL | -SIGNAL] PID
Send a signal to process.
- `-l` List available signals
- Default signal is SIGTERM (15)

### Debugging

#### trap [ACTION] SIGNALS
Execute ACTION when signal is received.

### Miscellaneous

#### echo [STRING]...
Print arguments to stdout.
- `-n` Don't print trailing newline
- `-e` Enable escape sequences

#### printf FORMAT [ARGUMENTS...]
Print formatted output.
- Supports standard printf formats

#### help [COMMAND]
Display help for builtin or command.

#### type COMMAND
Show how COMMAND would be interpreted.
- `-t` Show type (alias, function, builtin, file, keyword)
- `-p` Show pathname of file

#### which COMMAND
Show the full path of executable.

## Shell Features

### Variables

- `$VAR` or `${VAR}` - Variable expansion
- `${VAR:-default}` - Use default if unset
- `${VAR:=default}` - Set default if unset
- `${VAR:+value}` - Use value if set
- `${#VAR}` - Length of variable
- `${VAR:offset}` - Substring
- `${VAR:offset:length}` - Substring with length

### Special Variables

- `$0` - Name of the script or shell
- `$1` - $2 - ... - Positional parameters
- `$#` - Number of positional parameters
- `$@` - All positional parameters (quoted)
- `$*` - All positional parameters (unquoted)
- `$?` - Exit status of last command
- `$$` - PID of current shell
- `$!` - PID of last background command
- `$-` - Current option flags
- `$IFS` - Input field separator
- `$PATH` - Command search path

### Command Substitution

- `$(command)` - Execute command and substitute output
- `` `command` `` - Legacy backtick syntax
- Nested command substitution supported

### Arithmetic Expansion

- `$((expression))` - Evaluate arithmetic expression
- Supports: `+`, `-`, `*`, `/`, `%`, `**`, `<<`, `>>`, `&`, `|`, `^`, `&&`, `||`, `<`, `>`, `<=`, `>=`, `==`, `!=`, `!`, `~`

### Quoting

- Single quotes `'...'` - Literal (no expansion)
- Double quotes `"..."` - Allows variable and command substitution
- Backslash `\` - Escapes next character

### Pattern Matching

- `*` - Matches any string (including empty)
- `?` - Matches any single character
- `[...]` - Matches any character in set
- `[!...]` - Matches any character not in set
- `{a,b,c}` - Brace expansion

### Redirections

- `cmd > file` - Redirect stdout to file (truncate)
- `cmd >> file` - Append stdout to file
- `cmd < file` - Redirect stdin from file
- `cmd 2> file` - Redirect stderr to file
- `cmd &> file` - Redirect both stdout and stderr
- `cmd > file 2>&1` - Redirect stderr to stdout
- `cmd 2>&1 | cmd2` - Pipe with stderr
- `cmd << EOF` - Here-document
- `cmd <<< "string"` - Here-string
- `cmd <&fd` - Duplicate file descriptor (input)
- `cmd >&fd` - Duplicate file descriptor (output)

### Pipes

- `cmd1 | cmd2` - Pipe stdout of cmd1 to stdin of cmd2
- `cmd1 |& cmd2` - Pipe both stdout and stderr

### Logical Operators

- `cmd1 && cmd2` - Run cmd2 if cmd1 succeeds
- `cmd1 || cmd2` - Run cmd2 if cmd1 fails
- `cmd1 ; cmd2` - Run cmd1 then cmd2
- `cmd1 &` - Run cmd1 in background

### Control Structures

#### if/then/elif/else/fi
```bash
if condition; then
    commands
elif condition; then
    commands
else
    commands
fi
```

#### case/esac
```bash
case word in
    pattern1)
        commands;;
    pattern2)
        commands;;
    *)
        default commands;;
esac
```

#### for
```bash
for var in list; do
    commands
done

# C-style for loop
for ((i=0; i<10; i++)); do
    commands
done
```

#### while/until
```bash
while condition; do
    commands
done

until condition; do
    commands
done
```

#### select
```bash
select var in list; do
    commands
done
```

#### Functions
```bash
name () {
    commands
}

function name {
    commands
}
```

### Test Operations

- String tests: `-z` (empty), `-n` (not empty), `=`, `!=`
- Numeric tests: `-eq`, `-ne`, `-lt`, `-le`, `-gt`, `-ge`
- File tests: `-e` (exists), `-f` (regular file), `-d` (directory), `-r`, `-w`, `-x`, `-s` (non-empty)
- Logical: `!` (not), `-a` (and), `-o` (or)

## Line Editing

- **Arrow keys** - Navigate history
- **Ctrl+A** - Move to beginning of line
- **Ctrl+E** - Move to end of line
- **Ctrl+B** - Move back one character
- **Ctrl+F** - Move forward one character
- **Ctrl+U** - Kill from cursor to beginning
- **Ctrl+K** - Kill from cursor to end
- **Ctrl+Y** - Yank (paste) last killed text
- **Ctrl+W** - Kill previous word
- **Ctrl+D** - Delete character or EOF
- **Ctrl+L** - Clear screen
- **Tab** - Command/file completion

## Tab Completion

Supports completion for:
- Commands (from PATH)
- Files and directories
- Environment variables
- User names (~user)
- Host names (@host)

## History

- Up/Down arrows - Navigate history
- `history` - List command history
- `history -c` - Clear history
- History expansion with `!`, `!!`, `!$`, `!*`

## Prompt Customization

Prompt escape sequences:
- `\u` - Username
- `\h` - Hostname (short)
- `\H` - Hostname (full)
- `\w` - Current directory (relative)
- `\W` - Current directory (basename)
- `\d` - Date
- `\t` - Time (24-hour HH:MM:SS)
- `\T` - Time (12-hour HH:MM:SS)
- `\A` - Time (HH:MM)
- `\$` - $ or # (effective UID)
- `\n` - Newline
- `\\` - Literal backslash

## Startup Files

- `/etc/profile` - System-wide initialization
- `~/.bash_profile` or `~/.bash_login` - User login shell
- `~/.profile` - Login shell (POSIX)
- `~/.bashrc` - Interactive non-login shell
- `~/.bash_logout` - Login shell logout

## Signal Handling

- `SIGINT` (Ctrl+C) - Interrupt current command
- `SIGQUIT` (Ctrl+\) - Quit with core dump
- `SIGTSTP` (Ctrl+Z) - Suspend process
- `SIGCONT` - Resume suspended process
- `SIGTERM` - Graceful termination

## Exit Status

- `0` - Success
- `1-125` - Command-specific errors
- `126` - Command found but not executable
- `127` - Command not found
- `128+N` - Signal N terminated the process
