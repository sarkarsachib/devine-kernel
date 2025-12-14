#include "../../libc/include/kuser.h"
#include "../../libc/include/string.h"
#include "../../libc/include/stdlib.h"
#include "../../libc/include/stdio.h"
#include "../../libc/include/term.h"
#include "../include/shell.h"
#include <stdarg.h>

// Global shell state
static shell_state_t shell_state;

// Built-in commands
static const char *builtin_commands[] = {
    "cd", "echo", "pwd", "export", "unset", "alias", "unalias", "source", 
    "history", "set", "help", "exit", "ls", "cat", "clear", "env", 
    "setenv", "unsetenv", "type", "which"
};
static int builtin_count = sizeof(builtin_commands) / sizeof(builtin_commands[0]);

// Line editor implementation
void line_editor_init(void) {
    shell_state.cursor_pos = 0;
    shell_state.line_length = 0;
    shell_state.history_count = 0;
    shell_state.history_index = 0;
    shell_state.exit_status = 0;
    shell_state.signal_pending = 0;
    shell_state.debug_mode = 0;
    shell_state.env_count = 0;
    shell_state.alias_count = 0;
    shell_state.last_job_id = 0;
    shell_state.ps1 = "\\u@\\h:\\w\\$ ";
    shell_state.ps2 = "> ";
    shell_state.ps4 = "+ ";
    shell_state.completion_matches = NULL;
    shell_state.completion_count = 0;
    
    // Load history and environment
    history_load();
    load_environment();
    
    // Enable bracketed paste
    term_enable_bracketed_paste();
    
    // Set default terminal title
    term_set_title("Shell");
}

int line_editor_get_line(const char *prompt, char *buffer, size_t size) {
    if (size < 2) return -1;
    
    // Clear the buffer
    shell_state.current_line[0] = '\0';
    shell_state.cursor_pos = 0;
    shell_state.line_length = 0;
    
    // Show prompt with expansion
    char expanded_prompt[256];
    prompt_process(prompt, expanded_prompt, sizeof(expanded_prompt));
    printf("%s", expanded_prompt);
    fflush(stdout);
    
    int running = 1;
    while (running) {
        char c;
        if (read(0, &c, 1) <= 0) {
            buffer[0] = '\0';
            return -1;
        }
        
        // Handle special characters
        if (c == '\n' || c == '\r') {
            printf("\n");
            strcpy(buffer, shell_state.current_line);
            return strlen(buffer);
        }
        
        if (c == '\t') {
            // Tab completion
            char *matches[64];
            int completion_count = completion_complete(shell_state.current_line, shell_state.cursor_pos, 
                                                    matches, 64);
            
            if (completion_count == 1) {
                // Single match - complete it
                char *match = matches[0];
                int match_len = strlen(match);
                int space_pos = shell_state.cursor_pos;
                
                // Find word boundary
                while (space_pos > 0 && shell_state.current_line[space_pos - 1] != ' ') {
                    space_pos--;
                }
                
                // Replace current word
                int remaining = sizeof(shell_state.current_line) - space_pos - 1;
                strncpy(shell_state.current_line + space_pos, match, remaining);
                shell_state.line_length = strlen(shell_state.current_line);
                shell_state.cursor_pos = space_pos + match_len;
                
                // Redraw line
                printf("\r");
                prompt_process(prompt, expanded_prompt, sizeof(expanded_prompt));
                printf("%s", expanded_prompt);
                printf("%s", shell_state.current_line);
                
                // Clear rest of line
                printf("\033[K");
                
                // Position cursor
                printf("\033[%dC", shell_state.cursor_pos - strlen(expanded_prompt));
            } else if (completion_count > 1) {
                // Multiple matches - show them
                printf("\n");
                for (int i = 0; i < completion_count; i++) {
                    printf("%s  ", matches[i]);
                }
                printf("\n");
                prompt_process(prompt, expanded_prompt, sizeof(expanded_prompt));
                printf("%s", expanded_prompt);
                printf("%s", shell_state.current_line);
            }
            
            for (int i = 0; i < completion_count; i++) {
                free(matches[i]);
            }
            continue;
        }
        
        if (c == 0x7F || c == 0x08) { // Backspace
            if (shell_state.cursor_pos > 0) {
                // Move cursor back
                term_cursor_back(1);
                // Remove character
                for (int i = shell_state.cursor_pos; i < shell_state.line_length; i++) {
                    shell_state.current_line[i - 1] = shell_state.current_line[i];
                }
                shell_state.line_length--;
                shell_state.cursor_pos--;
                shell_state.current_line[shell_state.line_length] = '\0';
                
                // Redraw from cursor
                printf("\033[K%s", shell_state.current_line + shell_state.cursor_pos);
                // Position cursor
                printf("\033[%dC", shell_state.line_length - shell_state.cursor_pos);
            }
            continue;
        }
        
        if (c == 0x1B) { // Escape sequence
            char seq[3];
            if (read(0, &seq[0], 1) > 0 && read(0, &seq[1], 1) > 0) {
                if (seq[0] == '[') {
                    switch (seq[1]) {
                        case 'A': // Up arrow
                            if (shell_state.history_index > 0) {
                                shell_state.history_index--;
                                if (shell_state.history[shell_state.history_index]) {
                                    strcpy(shell_state.current_line, 
                                          shell_state.history[shell_state.history_index]);
                                    shell_state.line_length = strlen(shell_state.current_line);
                                    shell_state.cursor_pos = shell_state.line_length;
                                    
                                    // Redraw line
                                    printf("\r");
                                    prompt_process(prompt, expanded_prompt, sizeof(expanded_prompt));
                                    printf("%s", expanded_prompt);
                                    printf("%s\033[K", shell_state.current_line);
                                }
                            }
                            break;
                        case 'B': // Down arrow
                            if (shell_state.history_index < shell_state.history_count - 1) {
                                shell_state.history_index++;
                                if (shell_state.history[shell_state.history_index]) {
                                    strcpy(shell_state.current_line, 
                                          shell_state.history[shell_state.history_index]);
                                    shell_state.line_length = strlen(shell_state.current_line);
                                    shell_state.cursor_pos = shell_state.line_length;
                                }
                            } else {
                                // Clear line
                                shell_state.current_line[0] = '\0';
                                shell_state.line_length = 0;
                                shell_state.cursor_pos = 0;
                            }
                            printf("\r");
                            prompt_process(prompt, expanded_prompt, sizeof(expanded_prompt));
                            printf("%s\033[K", shell_state.current_line);
                            break;
                        case 'C': // Right arrow
                            if (shell_state.cursor_pos < shell_state.line_length) {
                                shell_state.cursor_pos++;
                                printf("\033[C");
                            }
                            break;
                        case 'D': // Left arrow
                            if (shell_state.cursor_pos > 0) {
                                shell_state.cursor_pos--;
                                printf("\033[D");
                            }
                            break;
                        case 'H': // Home
                            shell_state.cursor_pos = 0;
                            printf("\r");
                            prompt_process(prompt, expanded_prompt, sizeof(expanded_prompt));
                            printf("%s", expanded_prompt);
                            break;
                        case 'F': // End
                            shell_state.cursor_pos = shell_state.line_length;
                            printf("\033[%dC", shell_state.line_length);
                            break;
                    }
                }
            }
            continue;
        }
        
        if (c >= 32 && c < 127) { // Printable character
            if (shell_state.line_length < (int)size - 2) {
                // Insert character at cursor
                for (int i = shell_state.line_length; i >= shell_state.cursor_pos; i--) {
                    shell_state.current_line[i + 1] = shell_state.current_line[i];
                }
                shell_state.current_line[shell_state.cursor_pos] = c;
                shell_state.line_length++;
                shell_state.cursor_pos++;
                shell_state.current_line[shell_state.line_length] = '\0';
                
                // Redraw from cursor
                printf("\033[K%s", shell_state.current_line + shell_state.cursor_pos - 1);
                // Position cursor
                printf("\033[%dD", shell_state.line_length - shell_state.cursor_pos);
            }
        }
    }
    
    return -1; // Shouldn't reach here
}

void line_editor_add_history(const char *line) {
    if (line && line[0]) {
        // Check if this line is different from the last one in history
        int duplicate = 0;
        if (shell_state.history_count > 0) {
            const char *last_entry = shell_state.history[shell_state.history_count - 1];
            if (last_entry && strcmp(line, last_entry) == 0) {
                duplicate = 1;
            }
        }
        
        if (!duplicate) {
            if (shell_state.history_count >= MAX_HISTORY) {
                // Remove oldest entry
                free(shell_state.history[0]);
                for (int i = 0; i < MAX_HISTORY - 1; i++) {
                    shell_state.history[i] = shell_state.history[i + 1];
                }
                shell_state.history_count--;
            }
            
            shell_state.history[shell_state.history_count] = strdup(line);
            shell_state.history_count++;
            shell_state.history_index = shell_state.history_count;
        }
    }
}

// Built-in command implementations
int builtin_cd(int argc, char **argv) {
    char *path;
    
    if (argc == 1) {
        path = getenv("HOME");
        if (!path) path = "/";
    } else {
        path = argv[1];
    }
    
    // Expand tilde
    char expanded_path[256];
    expand_tilde(path, expanded_path, sizeof(expanded_path));
    
    // TODO: Actually change directory using VFS
    // For now, just update PWD environment variable
    setenv("PWD", expanded_path, 1);
    
    printf("cd: %s\n", expanded_path);
    return 0;
}

int builtin_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (i > 1) printf(" ");
        printf("%s", argv[i]);
    }
    printf("\n");
    return 0;
}

int builtin_pwd(int argc, char **argv) {
    char *pwd = getenv("PWD");
    if (!pwd) pwd = "/";
    printf("%s\n", pwd);
    return 0;
}

int builtin_export(int argc, char **argv) {
    if (argc == 1) {
        // Show all environment variables
        for (int i = 0; i < shell_state.env_count; i++) {
            printf("%s\n", shell_state.env_vars[i]);
        }
        return 0;
    }
    
    char *eq = strchr(argv[1], '=');
    if (eq) {
        *eq = '\0';
        setenv(argv[1], eq + 1, 1);
        *eq = '=';
    } else {
        setenv(argv[1], "", 1);
    }
    return 0;
}

int builtin_unset(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: unset VARIABLE\n");
        return 1;
    }
    unsetenv(argv[1]);
    return 0;
}

int builtin_alias(int argc, char **argv) {
    if (argc == 1) {
        // Show all aliases
        for (int i = 0; i < shell_state.alias_count; i++) {
            printf("%s\n", shell_state.aliases[i]);
        }
        return 0;
    }
    
    // Add new alias
    if (shell_state.alias_count < MAX_ALIASES) {
        strcpy(shell_state.aliases[shell_state.alias_count++], argv[1]);
    }
    return 0;
}

int builtin_unalias(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: unalias ALIAS\n");
        return 1;
    }
    
    // Remove alias
    for (int i = 0; i < shell_state.alias_count; i++) {
        if (strncmp(shell_state.aliases[i], argv[1], strlen(argv[1])) == 0) {
            for (int j = i; j < shell_state.alias_count - 1; j++) {
                strcpy(shell_state.aliases[j], shell_state.aliases[j + 1]);
            }
            shell_state.alias_count--;
            break;
        }
    }
    return 0;
}

int builtin_source(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: source FILE\n");
        return 1;
    }
    
    // TODO: Execute script file
    printf("source: %s (not implemented)\n", argv[1]);
    return 0;
}

int builtin_history(int argc, char **argv) {
    if (argc == 1) {
        // Show history
        for (int i = 0; i < shell_state.history_count; i++) {
            printf("%4d  %s\n", i + 1, shell_state.history[i]);
        }
    } else if (strcmp(argv[1], "-c") == 0) {
        // Clear history
        history_clear();
    }
    return 0;
}

int builtin_set(int argc, char **argv) {
    if (argc == 1) {
        // Show all variables
        for (int i = 0; i < shell_state.env_count; i++) {
            printf("%s\n", shell_state.env_vars[i]);
        }
        return 0;
    }
    
    if (strcmp(argv[1], "-x") == 0) {
        shell_state.debug_mode = 1;
        printf("Debug mode enabled\n");
    } else if (strcmp(argv[1], "+x") == 0) {
        shell_state.debug_mode = 0;
        printf("Debug mode disabled\n");
    }
    return 0;
}

int builtin_help(int argc, char **argv) {
    printf("Built-in commands:\n");
    for (int i = 0; i < builtin_count; i++) {
        printf("  %s\n", builtin_commands[i]);
    }
    printf("\nUse 'help COMMAND' for more information.\n");
    return 0;
}

int builtin_exit(int argc, char **argv) {
    if (argc > 1) {
        shell_state.exit_status = atoi(argv[1]);
    }
    exit(shell_state.exit_status);
    return 0; // Never reached
}

// Tab completion
void completion_init(void) {
    // Register built-in commands
    completion_register_commands(builtin_commands, builtin_count);
}

void completion_register_commands(const char **commands, int count) {
    (void)commands;
    (void)count;
    // TODO: Store commands for completion
}

void completion_register_files(const char *path) {
    (void)path;
    // TODO: Register file patterns for completion
}

int completion_complete(const char *line, int cursor_pos, char **matches, int max_matches) {
    (void)line;
    (void)cursor_pos;
    (void)matches;
    (void)max_matches;
    // TODO: Implement tab completion
    return 0;
}

void completion_free_matches(char **matches, int count) {
    (void)matches;
    (void)count;
    // TODO: Free completion matches
}

// Prompt processing
int prompt_process(const char *prompt, char *result, size_t size) {
    char *dest = result;
    size_t remaining = size - 1;
    
    while (*prompt && remaining > 0) {
        if (*prompt == '\\') {
            prompt++;
            if (*prompt) {
                switch (*prompt) {
                    case 'u': {
                        char *user = getenv("USER");
                        if (user && remaining > strlen(user)) {
                            strcpy(dest, user);
                            dest += strlen(user);
                            remaining -= strlen(user);
                        }
                        break;
                    }
                    case 'h': {
                        char *host = getenv("HOSTNAME");
                        if (host && remaining > strlen(host)) {
                            strcpy(dest, host);
                            dest += strlen(host);
                            remaining -= strlen(host);
                        } else {
                            const char *default_host = "localhost";
                            if (remaining > strlen(default_host)) {
                                strcpy(dest, default_host);
                                dest += strlen(default_host);
                                remaining -= strlen(default_host);
                            }
                        }
                        break;
                    }
                    case 'w': {
                        char *pwd = getenv("PWD");
                        if (!pwd) pwd = "/";
                        if (remaining > strlen(pwd)) {
                            strcpy(dest, pwd);
                            dest += strlen(pwd);
                            remaining -= strlen(pwd);
                        }
                        break;
                    }
                    case '$': {
                        *dest++ = '$';
                        remaining--;
                        break;
                    }
                    case '\\': {
                        *dest++ = '\\';
                        remaining--;
                        break;
                    }
                    default:
                        *dest++ = '\\';
                        remaining--;
                        break;
                }
                prompt++;
            }
        } else {
            *dest++ = *prompt++;
            remaining--;
        }
    }
    
    *dest = '\0';
    return dest - result;
}

void prompt_update_title(void) {
    char title[128];
    prompt_process("\\u@\\h:\\w\\$ ", title, sizeof(title));
    term_set_title(title);
}

void prompt_set_default(void) {
    shell_state.ps1 = "\\u@\\h:\\w\\$ ";
    shell_state.ps2 = "> ";
    shell_state.ps4 = "+ ";
}

// Variable expansion
int expand_variables(char *line, char *result, size_t result_size, int for_completion) {
    (void)line;
    (void)result;
    (void)result_size;
    (void)for_completion;
    // TODO: Implement variable expansion
    strcpy(result, line);
    return strlen(result);
}

int expand_tilde(char *path, char *result, size_t size) {
    if (path[0] == '~') {
        char *home = getenv("HOME");
        if (home) {
            snprintf(result, size, "%s%s", home, path + 1);
        } else {
            snprintf(result, size, "/home%s", path + 1);
        }
    } else {
        strncpy(result, path, size - 1);
        result[size - 1] = '\0';
    }
    return strlen(result);
}

char *get_variable_value(const char *name) {
    return getenv(name);
}

// Command parsing
int parse_command_line(char *line, char ***argv, int *argc) {
    char *tokens[MAX_ARGS];
    int token_count = 0;
    
    // Simple tokenization
    char *token = strtok(line, " \t\n\r");
    while (token && token_count < MAX_ARGS - 1) {
        tokens[token_count++] = strdup(token);
        token = strtok(NULL, " \t\n\r");
    }
    
    if (argc) *argc = token_count;
    if (argv) *argv = tokens;
    
    return token_count;
}

void free_parsed_command(char **argv, int argc) {
    (void)argc;
    // Free allocated tokens
    for (int i = 0; argv[i]; i++) {
        free(argv[i]);
    }
}

int handle_redirections(char **argv, int *argc) {
    (void)argv;
    (void)argc;
    // TODO: Implement I/O redirection
    return 0;
}

// History management
int history_load(void) {
    char *home = getenv("HOME");
    if (!home) return -1;
    
    char history_file[256];
    snprintf(history_file, sizeof(history_file), "%s/.bash_history", home);
    
    FILE *f = fopen(history_file, "r");
    if (!f) return -1;
    
    char line[512];
    while (fgets(line, sizeof(line), f) && shell_state.history_count < MAX_HISTORY) {
        line[strcspn(line, "\n\r")] = '\0';
        shell_state.history[shell_state.history_count] = strdup(line);
        shell_state.history_count++;
    }
    
    fclose(f);
    return 0;
}

int history_save(void) {
    char *home = getenv("HOME");
    if (!home) return -1;
    
    char history_file[256];
    snprintf(history_file, sizeof(history_file), "%s/.bash_history", home);
    
    FILE *f = fopen(history_file, "w");
    if (!f) return -1;
    
    for (int i = 0; i < shell_state.history_count; i++) {
        fprintf(f, "%s\n", shell_state.history[i]);
    }
    
    fclose(f);
    return 0;
}

void history_add(const char *line) {
    line_editor_add_history(line);
}

const char *history_get(int index) {
    if (index >= 0 && index < shell_state.history_count) {
        return shell_state.history[index];
    }
    return NULL;
}

void history_clear(void) {
    for (int i = 0; i < shell_state.history_count; i++) {
        free(shell_state.history[i]);
    }
    shell_state.history_count = 0;
    shell_state.history_index = 0;
}

void load_environment(void) {
    // Set default environment variables
    setenv("HOME", "/home", 1);
    setenv("PWD", "/", 1);
    setenv("USER", "root", 1);
    setenv("HOSTNAME", "devine", 1);
    setenv("SHELL", "/bin/sh", 1);
    setenv("TERM", "xterm-256color", 1);
}

// External command execution
int execute_external_command(int argc, char **argv) {
    if (argc == 0) return 0;
    
    // Check for built-in commands first
    for (int i = 0; i < builtin_count; i++) {
        if (strcmp(argv[0], builtin_commands[i]) == 0) {
            return execute_builtin(i, argc, argv);
        }
    }
    
    // Try to execute external program
    char path[512];
    snprintf(path, sizeof(path), "/bin/%s", argv[0]);
    
    // Try current directory first
    if (access(argv[0], 0) == 0) {
        strcpy(path, argv[0]);
    }
    
    // Fork and execute
    int pid = fork();
    if (pid == 0) {
        // Child process
        exec(path, (const char *const *)argv);
        // If we get here, exec failed
        printf("%s: command not found\n", argv[0]);
        exit(127);
    } else if (pid > 0) {
        // Parent process - wait for child
        int status;
        wait(pid);
        return WEXITSTATUS(status);
    } else {
        printf("fork failed\n");
        return 1;
    }
}

int execute_builtin(int builtin_index, int argc, char **argv) {
    switch (builtin_index) {
        case 0: return builtin_cd(argc, argv);     // cd
        case 1: return builtin_echo(argc, argv);   // echo
        case 2: return builtin_pwd(argc, argv);    // pwd
        case 3: return builtin_export(argc, argv); // export
        case 4: return builtin_unset(argc, argv);  // unset
        case 5: return builtin_alias(argc, argv);  // alias
        case 6: return builtin_unalias(argc, argv);// unalias
        case 7: return builtin_source(argc, argv); // source
        case 8: return builtin_history(argc, argv);// history
        case 9: return builtin_set(argc, argv);    // set
        case 10: return builtin_help(argc, argv);  // help
        case 11: return builtin_exit(argc, argv);  // exit
        case 12: return execute_ls(argc, argv);    // ls
        case 13: return execute_cat(argc, argv);   // cat
        case 14: return builtin_clear(argc, argv); // clear
        case 15: return builtin_env(argc, argv);   // env
        case 16: return builtin_setenv(argc, argv);// setenv
        case 17: return builtin_unsetenv(argc, argv); // unsetenv
        default: 
            printf("%s: command not found\n", argv[0]);
            return 1;
    }
}

// Additional built-in commands
int builtin_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    term_clear_screen();
    return 0;
}

int builtin_env(int argc, char **argv) {
    (void)argc;
    (void)argv;
    for (int i = 0; i < shell_state.env_count; i++) {
        printf("%s\n", shell_state.env_vars[i]);
    }
    return 0;
}

int builtin_setenv(int argc, char **argv) {
    if (argc < 2) {
        return builtin_env(argc, argv);
    }
    return setenv(argv[1], argv[2] ? argv[2] : "", 1);
}

int builtin_unsetenv(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: unsetenv VARIABLE\n");
        return 1;
    }
    return unsetenv(argv[1]);
}

// Simple file utilities for testing
int execute_ls(int argc, char **argv) {
    (void)argc;
    (void)argv;
    printf("ls: listing directory (simulated)\n");
    printf("bin  dev  etc  home  proc  tmp  usr\n");
    return 0;
}

int execute_cat(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: cat FILE\n");
        return 1;
    }
    
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        printf("cat: %s: No such file\n", argv[1]);
        return 1;
    }
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), f)) {
        printf("%s", buffer);
    }
    
    fclose(f);
    return 0;
}

// Main shell loop
void shell_init(void) {
    // Initialize terminal
    term_init();
    
    // Load startup files
    load_startup_files();
    
    // Initialize shell components
    line_editor_init();
    completion_init();
    prompt_set_default();
}

void shell_cleanup(void) {
    // Save history
    history_save();
    
    // Disable bracketed paste
    term_disable_bracketed_paste();
}

void load_startup_files(void) {
    // TODO: Load /etc/profile, ~/.bash_profile, ~/.bashrc
    // For now, just set some defaults
    load_environment();
}

int shell_run(void) {
    char line[MAX_LINE_LENGTH];
    
    while (1) {
        // Update terminal title
        prompt_update_title();
        
        // Get input line
        int len = line_editor_get_line(shell_state.ps1, line, sizeof(line));
        if (len < 0) break;
        
        // Add to history
        if (len > 0) {
            line_editor_add_history(line);
        }
        
        // Skip empty lines
        if (len == 0) continue;
        
        // Execute the command
        if (shell_state.debug_mode) {
            printf("%s%s\n", shell_state.ps4, line);
        }
        
        // Parse and execute
        char *line_copy = strdup(line);
        char **argv;
        int argc;
        
        parse_command_line(line_copy, &argv, &argc);
        
        if (argc > 0) {
            // Handle built-in commands
            int builtin_idx = -1;
            for (int i = 0; i < builtin_count; i++) {
                if (strcmp(argv[0], builtin_commands[i]) == 0) {
                    builtin_idx = i;
                    break;
                }
            }
            
            if (builtin_idx >= 0) {
                shell_state.exit_status = execute_builtin(builtin_idx, argc, argv);
            } else {
                shell_state.exit_status = execute_external_command(argc, argv);
            }
        }
        
        free_parsed_command(argv, argc);
        free(line_copy);
    }
    
    return shell_state.exit_status;
}

// Main entry point
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    shell_init();
    int result = shell_run();
    shell_cleanup();
    
    return result;
}