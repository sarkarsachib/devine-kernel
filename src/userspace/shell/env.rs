//! env - run a program in a modified environment
//!
//! Usage: env [OPTION]... [NAME=VALUE]... [COMMAND [ARG]...]
//!        env -i [NAME=VALUE]... [COMMAND [ARG]...]
//!
//! Set each NAME to VALUE in the environment and run COMMAND.
//!
//! Options:
//!   -i, --ignore-environment  Start with an empty environment
//!   -0, --null           End each output line with NUL, not newline
//!   -u, --unset=NAME     Remove variable from environment
//!
//! If no COMMAND is specified, print the resulting environment.

pub fn run(args: &[&str]) -> i32 {
    let mut ignore_env = false;
    let mut unset_vars: Vec<&str> = Vec::new();
    let mut env_vars: Vec<(&str, &str)> = Vec::new();
    let mut command: Option<(&str, &[&str])> = None;

    let mut i = 0;
    while i < args.len() {
        let arg = args[i];
        
        if arg == "-i" || arg == "--ignore-environment" {
            ignore_env = true;
        } else if arg == "-0" || arg == "--null" {
            // NUL terminated output
        } else if arg == "-u" || arg == "--unset" {
            i += 1;
            if i < args.len() {
                unset_vars.push(args[i]);
            }
        } else if arg.starts_with("-u=") {
            unset_vars.push(&arg[3..]);
        } else if arg.contains('=') {
            // Environment variable assignment
            if let Some(eq_pos) = arg.find('=') {
                let name = &arg[..eq_pos];
                let value = &arg[eq_pos + 1..];
                env_vars.push((name, value));
            }
        } else {
            // This and following args are the command
            command = Some((arg, &args[i + 1..]));
            break;
        }
        i += 1;
    }

    // If no command, just print environment
    if command.is_none() {
        if ignore_env {
            // Start with empty, add new vars
            for &(name, value) in &env_vars {
                println!("{}={}", name, value);
            }
        } else {
            // Would print current environment minus unset vars
            for &(name, value) in &env_vars {
                println!("{}={}", name, value);
            }
        }
        return 0;
    }

    0
}
