#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include "../include/term.h"

// Shell configuration
#define MAX_LINE_LENGTH 2048
#define MAX_ARGS 256
#define MAX_HISTORY 1000
#define MAX_ENV_VARS 128
#define MAX_ALIASES 64
#define MAX_COMMANDS 128

// Shell state
typedef struct {
    char current_line[MAX_LINE_LENGTH];
    int cursor_pos;
    int line_length;
    char *history[MAX_HISTORY];
    int history_count;
    int history_index;
    int exit_status;
    int signal_pending;
    int debug_mode;
    
    // Environment
    char env_vars[MAX_ENV_VARS][256];
    int env_count;
    
    // Aliases
    char aliases[MAX_ALIASES][512];
    int alias_count;
    
    // Prompts
    char *ps1;
    char *ps2;
    char *ps4;
    
    // Tab completion
    char last_completion[256];
    int completion_count;
    char **completion_matches;
    
    // Job control
    int last_job_id;
} shell_state_t;

// Line editor functions
void line_editor_init(void);
int line_editor_get_line(const char *prompt, char *buffer, size_t size);
void line_editor_add_history(const char *line);
void line_editor_clear_history(void);
int line_editor_set_terminal_title(const char *title);

// Tab completion
void completion_init(void);
void completion_register_commands(const char **commands, int count);
void completion_register_files(const char *path);
int completion_complete(const char *line, int cursor_pos, char **matches, int max_matches);
void completion_free_matches(char **matches, int count);

// Built-in commands
int builtin_cd(int argc, char **argv);
int builtin_echo(int argc, char **argv);
int builtin_pwd(int argc, char **argv);
int builtin_export(int argc, char **argv);
int builtin_unset(int argc, char **argv);
int builtin_alias(int argc, char **argv);
int builtin_unalias(int argc, char **argv);
int builtin_source(int argc, char **argv);
int builtin_history(int argc, char **argv);
int builtin_set(int argc, char **argv);
int builtin_help(int argc, char **argv);
int builtin_exit(int argc, char **argv);

// Prompt processing
int prompt_process(const char *prompt, char *result, size_t size);
void prompt_update_title(void);
void prompt_set_default(void);

// Variable expansion
int expand_variables(char *line, char *result, size_t result_size, int for_completion);
int expand_tilde(char *path, char *result, size_t size);
char *get_variable_value(const char *name);

// Command parsing
int parse_command_line(char *line, char ***argv, int *argc);
void free_parsed_command(char **argv, int argc);
int handle_redirections(char **argv, int *argc);

// History management
int history_load(void);
int history_save(void);
void history_add(const char *line);
const char *history_get(int index);
void history_clear(void);

// Main shell loop
int shell_run(void);
void shell_init(void);
void shell_cleanup(void);

#endif /* SHELL_H */