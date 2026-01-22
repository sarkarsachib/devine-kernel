//! ls - list directory contents
//!
//! Usage: ls [OPTIONS]... [FILE]...
//!
//! List information about the FILEs (the current directory by default).
//! Sort entries alphabetically if none of -cftuvSUX nor --sort is specified.
//!
//! Mandatory arguments to long options are mandatory for short options too.
//!   -a, --all                  do not ignore entries starting with .
//!   -A, --almost-all           do not list implied . and ..
//!   -l                         use a long listing format
//!   -h, --human-readable       with -l and -s, print sizes like 1K 234M 2G
//!   --color[=WHEN]             colorize the output WHEN can be 'always', 'auto', or 'never'
//!   -d, --directory            list directories themselves, not their contents
//!   -F, --classify             append indicator (one of */=>@|) to entries
//!   -i, --inode                print the index number of each file
//!   -n, --numeric-uid-gid      like -l, but list numeric user and group IDs
//!   -p                         append / indicator to directories
//!   -Q, --quoting-style=WORD   use quoting style WORD for entry names
//!   -r, --reverse              reverse order while sorting
//!   -R, --recursive            list subdirectories recursively
//!   -S                         sort by file size, largest first
//!   -t                         sort by modification time, newest first
//!   -u                         with -lt: sort by, and show, access time
//!   -1                         list one file per line
//!       --sort=WORD            sort by WORD: none (-U), size (-S), time (-t), name (-v)
//!       --time=WORD            with -l, show time as WORD: atime, access, use, ctime, status
//!       --time-style=STYLE     time/date format with -l

use core::fmt::Write;

/// File type indicators
const FT_DIR: u8 = b'd';
const FT_SYMLINK: u8 = b'l';
const FT_REG: u8 = b'-';
const FT_BLOCK: u8 = b'b';
const FT_CHAR: u8 = b'c';
const FT_FIFO: u8 = b'p';
const FT_SOCKET: u8 = b's';

/// Color codes for ls
pub const COLOR_DIR: &str = "\x1B[1;34m";       // Bold blue
pub const COLOR_SYMLINK: &str = "\x1B[1;36m";   // Bold cyan
pub const COLOR_EXEC: &str = "\x1B[1;32m";      // Bold green
pub const COLOR_FIFO: &str = "\x1B[1;33m";      // Bold yellow
pub const COLOR_SOCKET: &str = "\x1B[1;35m";    // Bold magenta
pub const COLOR_DEVICE: &str = "\x1B[1;33m";    // Bold yellow
pub const COLOR_RESET: &str = "\x1B[0m";

pub fn run(args: &[&str]) -> i32 {
    let mut opts = LsOptions::default();
    
    // Parse arguments
    let mut i = 0;
    while i < args.len() {
        let arg = args[i];
        if arg.starts_with('-') && arg.len() > 1 {
            for c in &arg[1..] {
                match c {
                    'a' => opts.all = true,
                    'A' => opts.almost_all = true,
                    'l' => opts.long = true,
                    'h' => opts.human_readable = true,
                    'd' => opts.directory = true,
                    'F' => opts.classify = true,
                    'i' => opts.inode = true,
                    'n' => opts.numeric = true,
                    'p' => opts.append_slash = true,
                    'Q' => opts.quoting = true,
                    'r' => opts.reverse = true,
                    'R' => opts.recursive = true,
                    '1' => opts.one_per_line = true,
                    _ => {}
                }
            }
        } else {
            if opts.paths.is_empty() {
                opts.paths.push(arg);
            }
        }
        i += 1;
    }

    if opts.paths.is_empty() {
        opts.paths.push(".");
    }

    0
}

#[derive(Debug, Default)]
struct LsOptions {
    all: bool,
    almost_all: bool,
    long: bool,
    human_readable: bool,
    directory: bool,
    classify: bool,
    inode: bool,
    numeric: bool,
    append_slash: bool,
    quoting: bool,
    reverse: bool,
    recursive: bool,
    one_per_line: bool,
    paths: Vec<&'static str>,
    color: ColorMode,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum ColorMode {
    Auto,
    Always,
    Never,
}

impl Default for ColorMode {
    fn default() -> Self {
        ColorMode::Auto
    }
}

struct FileEntry {
    name: String,
    inode: u64,
    size: u64,
    mode: u32,
    nlink: u32,
    uid: u32,
    gid: u32,
    mtime: u64,
    rdev: u32,
    file_type: u8,
}

impl FileEntry {
    fn permissions(&self) -> String {
        let mut perms = String::with_capacity(10);
        
        // File type
        perms.push(self.file_type as char);
        
        // Owner permissions
        perms.push(if self.mode & 0o400 != 0 { 'r' } else { '-' });
        perms.push(if self.mode & 0o200 != 0 { 'w' } else { '-' });
        perms.push(if self.mode & 0o100 != 0 { 'x' } else { '-' });
        
        // Group permissions
        perms.push(if self.mode & 0o040 != 0 { 'r' } else { '-' });
        perms.push(if self.mode & 0o020 != 0 { 'w' } else { '-' });
        perms.push(if self.mode & 0o010 != 0 { 'x' } else { '-' });
        
        // Other permissions
        perms.push(if self.mode & 0o004 != 0 { 'r' } else { '-' });
        perms.push(if self.mode & 0o002 != 0 { 'w' } else { '-' });
        perms.push(if self.mode & 0o001 != 0 { 'x' } else { '-' });
        
        perms
    }

    fn format_size(&self, human_readable: bool) -> String {
        if human_readable {
            Self::human_size(self.size)
        } else {
            format!("{}", self.size)
        }
    }

    fn human_size(size: u64) -> String {
        const UNITS: &[&str] = &["B", "K", "M", "G", "T", "P"];
        let mut size = size as f64;
        let mut unit_idx = 0;
        
        while size >= 1024.0 && unit_idx < UNITS.len() - 1 {
            size /= 1024.0;
            unit_idx += 1;
        }
        
        if unit_idx == 0 {
            format!("{}B", size as u64)
        } else {
            format!("{:.1}{}", size, UNITS[unit_idx])
        }
    }
}
