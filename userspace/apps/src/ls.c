#include "../../libc/include/string.h"
#include "../../libc/include/stdlib.h"
#include "../../libc/include/stdio.h"
#include "../../libc/include/term.h"
#include <stdarg.h>

#define LS_MAX_LINE_LENGTH 4096
#define LS_MAX_ENTRIES 2048

// Color modes
typedef enum {
    COLOR_NONE = 0,
    COLOR_AUTO,
    COLOR_ALWAYS
} color_mode_t;

// File type indicators
static const char* file_type_indicator(int mode) {
    if (S_ISDIR(mode)) return "/";
    if (S_ISLNK(mode)) return "@";
    if (S_ISFIFO(mode)) return "|";
    if (S_ISSOCK(mode)) return "=";
    if (S_ISCHR(mode)) return "%";
    if (S_ISBLK(mode)) return "%";
    if (mode & 0111) return "*";
    return "";
}

// Get color for file based on type
static const char* get_file_color(const char *name, int mode) {
    if (S_ISDIR(mode)) return "\033[1;34m";  // Bold blue
    if (S_ISLNK(mode)) return "\033[1;36m";  // Bold cyan
    if (S_ISFIFO(mode)) return "\033[1;33m"; // Bold yellow
    if (S_ISSOCK(mode)) return "\033[1;35m"; // Bold magenta
    if (S_ISCHR(mode)) return "\033[1;33m";  // Bold yellow
    if (S_ISBLK(mode)) return "\033[1;33m";  // Bold yellow
    if (mode & 0111) return "\033[1;32m";    // Bold green (executable)
    return "";  // Default color
}

// Format file size in human readable format
static void format_size(unsigned long size, char *result, size_t max_len) {
    const char *units[] = {"B", "K", "M", "G", "T"};
    int unit = 0;
    double s = (double)size;
    
    while (s >= 1024 && unit < 4) {
        s /= 1024;
        unit++;
    }
    
    snprintf(result, max_len, "%4.1f%s", s, units[unit]);
}

// Parse ls options
typedef struct {
    int list_all;
    int long_format;
    int human_readable;
    int show_size;
    int show_time;
    int recursive;
    int sort_by_time;
    int reverse;
    color_mode_t color;
    int show_indicator;
} ls_options_t;

static void parse_ls_options(int argc, char **argv, ls_options_t *opts, int *start_index) {
    memset(opts, 0, sizeof(ls_options_t));
    opts->color = COLOR_AUTO;
    *start_index = 1;
    
    for (int i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            for (int j = 1; argv[i][j]; j++) {
                switch (argv[i][j]) {
                    case 'a': case 'A': opts->list_all = 1; break;
                    case 'l': opts->long_format = 1; break;
                    case 'h': opts->human_readable = 1; break;
                    case 's': opts->show_size = 1; break;
                    case 't': opts->sort_by_time = 1; break;
                    case 'r': opts->reverse = 1; break;
                    case 'R': opts->recursive = 1; break;
                    case 'F': opts->show_indicator = 1; break;
                    case 'G': opts->color = COLOR_AUTO; break;
                    case '--color':
                        if (j == 1 && argv[i][j+1] == '=') {
                            const char *color_val = argv[i] + j + 6;
                            if (strcmp(color_val, "always") == 0) opts->color = COLOR_ALWAYS;
                            else if (strcmp(color_val, "never") == 0) opts->color = COLOR_NONE;
                            else if (strcmp(color_val, "auto") == 0) opts->color = COLOR_AUTO;
                        }
                        break;
                    default:
                        fprintf(stderr, "ls: invalid option -- '%c'\n", argv[i][j]);
                        break;
                }
            }
        } else {
            *start_index = i;
            break;
        }
    }
}

// Print long format
static void print_long_format(const char *filename, const char *path, ls_options_t *opts) {
    (void)path;
    
    // For now, simulate long format since we don't have real stat info
    struct stat st;
    if (stat(path, &st) == 0) {
        char perms[11] = "----------";
        
        // File type
        if (S_ISDIR(st.st_mode)) perms[0] = 'd';
        else if (S_ISLNK(st.st_mode)) perms[0] = 'l';
        else if (S_ISFIFO(st.st_mode)) perms[0] = 'p';
        else if (S_ISSOCK(st.st_mode)) perms[0] = 's';
        else if (S_ISCHR(st.st_mode)) perms[0] = 'c';
        else if (S_ISBLK(st.st_mode)) perms[0] = 'b';
        
        // Permissions
        if (st.st_mode & 0400) perms[1] = 'r';
        if (st.st_mode & 0200) perms[2] = 'w';
        if (st.st_mode & 0100) perms[3] = 'x';
        if (st.st_mode & 0040) perms[4] = 'r';
        if (st.st_mode & 0020) perms[5] = 'w';
        if (st.st_mode & 0010) perms[6] = 'x';
        if (st.st_mode & 0004) perms[7] = 'r';
        if (st.st_mode & 0002) perms[8] = 'w';
        if (st.st_mode & 0001) perms[9] = 'x';
        
        // Print permissions and link count
        printf("%s %2lu", perms, st.st_nlink);
        
        // Size
        char size_str[16];
        if (opts->human_readable) {
            format_size(st.st_size, size_str, sizeof(size_str));
            printf(" %8s", size_str);
        } else {
            printf(" %8lu", st.st_size);
        }
        
        // Time
        printf(" %s", ctime(&st.st_mtime) + 4); // Skip day name
        
        // Filename
        if (opts->color != COLOR_NONE) {
            const char *color = get_file_color(filename, st.st_mode);
            if (color && color[0]) {
                printf("%s%s%s", color, filename, COLOR_RESET);
            } else {
                printf("%s", filename);
            }
        } else {
            printf("%s", filename);
        }
        
        // Indicator
        if (opts->show_indicator) {
            const char *indicator = file_type_indicator(st.st_mode);
            if (indicator && indicator[0]) {
                printf("%s", indicator);
            }
        }
        
        printf("\n");
    } else {
        // Fallback for files without stat info
        printf("%s\n", filename);
    }
}

// Print short format
static void print_short_format(const char *filename, ls_options_t *opts) {
    (void)opts; // Future: implement based on actual file info
    printf("%s\n", filename);
}

int main(int argc, char **argv) {
    ls_options_t opts;
    int start_index;
    
    parse_ls_options(argc, argv, &opts, &start_index);
    
    if (argc == start_index) {
        // No paths specified, use current directory
        const char *paths[] = {"."};
        for (int i = 0; i < 1; i++) {
            if (opts.long_format) {
                print_long_format(".", paths[i], &opts);
            } else {
                print_short_format(".", &opts);
            }
        }
    } else {
        // Process specified paths
        for (int i = start_index; i < argc; i++) {
            const char *path = argv[i];
            
            if (argc > start_index + 1) {
                printf("%s:\n", path);
            }
            
            if (opts.long_format) {
                print_long_format(path, path, &opts);
            } else {
                print_short_format(path, &opts);
            }
            
            if (i < argc - 1) {
                printf("\n");
            }
        }
    }
    
    return 0;
}