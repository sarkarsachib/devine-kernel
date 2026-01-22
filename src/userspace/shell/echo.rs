//! echo - display a line of text
//!
//! Usage: echo [OPTIONS]... [STRING]...
//!
//! Echo the STRING(s) to standard output.
//!
//! Options:
//!   -n             Do not output the trailing newline
//!   -e             Enable interpretation of backslash escapes
//!   -E             Disable interpretation of backslash escapes (default)
//!
//! Backslash escapes:
//!   \\     backslash
//!   \a     alert (BEL)
//!   \b     backspace
//!   \c     produce no further output
//!   \e     escape
//!   \f     form feed
//!   \n     new line
//!   \r     carriage return
//!   \t     horizontal tab
//!   \v     vertical tab
//!   \0NNN  byte with octal value NNN (1 to 3 digits)
//!   \xHH   byte with hexadecimal value HH (1 to 2 digits)

use core::fmt::Write;

pub fn run(args: &[&str]) -> i32 {
    let mut newline = true;
    let mut interpret_escapes = false;

    let mut i = 0;
    while i < args.len() {
        if args[i] == "-n" {
            newline = false;
        } else if args[i] == "-e" {
            interpret_escapes = true;
        } else if args[i] == "-E" {
            interpret_escapes = false;
        } else if args[i].starts_with('-') && args[i].len() > 1 {
            // Ignore other options (for compatibility)
        } else {
            break;
        }
        i += 1;
    }

    let mut output = String::new();
    while i < args.len() {
        if i > 0 {
            output.push(' ');
        }
        output.push_str(args[i]);
        i += 1;
    }

    if interpret_escapes {
        output = escape_string(&output);
    }

    if newline {
        println!("{}", output);
    } else {
        print!("{}", output);
    }

    0
}

fn escape_string(s: &str) -> String {
    let mut result = String::new();
    let mut chars = s.chars().peekable();
    
    while let Some(c) = chars.next() {
        if c == '\\' {
            if let Some(&next) = chars.peek() {
                let consumed = match next {
                    '\\' => { result.push('\\'); true },
                    'a' => { result.push('\x07'); true },
                    'b' => { result.push('\x08'); true },
                    'c' => { return result; },  // Stop here
                    'e' => { result.push('\x1b'); true },
                    'f' => { result.push('\x0c'); true },
                    'n' => { result.push('\n'); true },
                    'r' => { result.push('\r'); true },
                    't' => { result.push('\t'); true },
                    'v' => { result.push('\x0b'); true },
                    '0' => {
                        // Octal escape
                        let mut octal = String::new();
                        octal.push(chars.next().unwrap());
                        if let Some(&d) = chars.peek().filter(|&&d| d.is_ascii_digit()) {
                            octal.push(chars.next().unwrap());
                        }
                        if let Some(&d) = chars.peek().filter(|&&d| d.is_ascii_digit()) {
                            octal.push(chars.next().unwrap());
                        }
                        if let Ok(val) = u8::from_str_radix(&octal, 8) {
                            result.push(val as char);
                        }
                        true
                    },
                    'x' => {
                        // Hex escape
                        chars.next();  // Skip x
                        let mut hex = String::new();
                        for _ in 0..2 {
                            if let Some(&d) = chars.peek().filter(|&&d| d.is_ascii_hexdigit()) {
                                hex.push(chars.next().unwrap());
                            }
                        }
                        if let Ok(val) = u8::from_str_radix(&hex, 16) {
                            result.push(val as char);
                        }
                        true
                    },
                    _ => { result.push('\\'); false },
                };
                if consumed {
                    chars.next();
                }
            }
        } else {
            result.push(c);
        }
    }

    result
}
