//! sleep - delay for a specified time
//!
//! Usage: sleep NUMBER[SUFFIX]...
//!        sleep --help
//!        sleep --version
//!
//! Pause for NUMBER seconds.  SUFFIX may be 's' for seconds (default),
//! 'm' for minutes, 'h' for hours, or 'd' for days.
//!
//! Since this is a kernel environment, sleep uses busy-waiting
//! and is not efficient for long durations.

pub fn run(args: &[&str]) -> i32 {
    if args.is_empty() {
        eprintln!("sleep: missing operand");
        return 1;
    }

    let mut total_seconds = 0.0f64;

    for arg in args {
        let mut num_str = *arg;
        let mut suffix = 's';

        // Check for suffix character
        if !num_str.is_empty() {
            let last_char = num_str.chars().last().unwrap();
            if !last_char.is_ascii_digit() {
                suffix = last_char;
                num_str = &num_str[..num_str.len() - 1];
            }
        }

        // Parse the number
        let value: f64 = match num_str.parse() {
            Ok(v) => v,
            Err(_) => {
                eprintln!("sleep: invalid time interval '{}'", arg);
                return 1;
            }
        };

        // Convert to seconds based on suffix
        let seconds = match suffix {
            's' => value,
            'm' => value * 60.0,
            'h' => value * 3600.0,
            'd' => value * 86400.0,
            _ => {
                eprintln!("sleep: invalid time interval '{}'", arg);
                return 1;
            }
        };

        total_seconds += seconds;
    }

    // In kernel environment, we would use a timer interrupt
    // For now, this is a placeholder
    if total_seconds > 0.0 {
        // busy_wait(total_seconds);
    }

    0
}
