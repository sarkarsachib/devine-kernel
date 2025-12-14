#ifndef TERM_H
#define TERM_H

#include <stddef.h>

// Terminal capabilities
typedef struct {
    int has_colors;
    int max_colors;
    int lines;
    int cols;
    int bold_supported;
    int underline_supported;
    int alt_screen;
    int bracketed_paste;
} termios_t;

// ANSI escape sequences for colors
#define COLOR_RESET       "\033[0m"
#define COLOR_BOLD        "\033[1m"
#define COLOR_DIM         "\033[2m"
#define COLOR_UNDERLINE   "\033[4m"
#define COLOR_BLINK       "\033[5m"
#define COLOR_REVERSE     "\033[7m"
#define COLOR_HIDDEN      "\033[8m"

#define COLOR_FG_BLACK    "\033[30m"
#define COLOR_FG_RED      "\033[31m"
#define COLOR_FG_GREEN    "\033[32m"
#define COLOR_FG_YELLOW   "\033[33m"
#define COLOR_FG_BLUE     "\033[34m"
#define COLOR_FG_MAGENTA  "\033[35m"
#define COLOR_FG_CYAN     "\033[36m"
#define COLOR_FG_WHITE    "\033[37m"
#define COLOR_FG_BRIGHT   "\033[90m"
#define COLOR_FG_BRIGHT_RED   "\033[91m"
#define COLOR_FG_BRIGHT_GREEN "\033[92m"
#define COLOR_FG_BRIGHT_YELLOW "\033[93m"
#define COLOR_FG_BRIGHT_BLUE  "\033[94m"
#define COLOR_FG_BRIGHT_MAGENTA "\033[95m"
#define COLOR_FG_BRIGHT_CYAN  "\033[96m"
#define COLOR_FG_BRIGHT_WHITE "\033[97m"

#define COLOR_BG_BLACK    "\033[40m"
#define COLOR_BG_RED      "\033[41m"
#define COLOR_BG_GREEN    "\033[42m"
#define COLOR_BG_YELLOW   "\033[43m"
#define COLOR_BG_BLUE     "\033[44m"
#define COLOR_BG_MAGENTA  "\033[45m"
#define COLOR_BG_CYAN     "\033[46m"
#define COLOR_BG_WHITE    "\033[47m"

// Terminal control sequences
#define TERM_CURSOR_UP(n)      "\033[" #n "A"
#define TERM_CURSOR_DOWN(n)    "\033[" #n "B"
#define TERM_CURSOR_FORWARD(n) "\033[" #n "C"
#define TERM_CURSOR_BACK(n)    "\033[" #n "D"
#define TERM_CURSOR_HOME       "\033[H"
#define TERM_CURSOR_SAVE       "\033[s"
#define TERM_CURSOR_RESTORE    "\033[u"
#define TERM_CLEAR_SCREEN      "\033[2J"
#define TERM_CLEAR_EOL         "\033[K"
#define TERM_CLEAR_BOS         "\033[1J"
#define TERM_CLEAR_EOS         "\033[J"
#define TERM_ALT_SCREEN_ON     "\033[?1049h"
#define TERM_ALT_SCREEN_OFF    "\033[?1049l"
#define TERM_BRACKETED_PASTE_ON    "\033[?2004h"
#define TERM_BRACKETED_PASTE_OFF   "\033[?2004l"
#define TERM_TITLE_SET        "\033]2;%s\007"

// Function declarations
void term_init(void);
void term_get_size(int *lines, int *cols);
int term_isatty(void);
void term_enable_bracketed_paste(void);
void term_disable_bracketed_paste(void);
void term_set_title(const char *title);
void term_save_cursor(void);
void term_restore_cursor(void);
void term_cursor_up(int n);
void term_cursor_down(int n);
void term_cursor_forward(int n);
void term_cursor_back(int n);
void term_cursor_home(void);
void term_clear_screen(void);
void term_clear_eol(void);
void term_clear_bos(void);
void term_clear_eos(void);
void term_alternate_screen(int on);
void term_enable_bracketed_paste(void);
void term_disable_bracketed_paste(void);

// Color helpers
const char* term_color_name_to_code(const char *name);

#endif /* TERM_H */