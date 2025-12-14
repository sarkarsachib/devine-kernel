#include "../include/kuser.h"
#include "../include/string.h"
#include "../include/stdlib.h"
#include "../include/stdio.h"
#include "../include/term.h"

// Forward declarations
struct stat;
struct dirent;
struct tm;
typedef unsigned int mode_t;
typedef unsigned int nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef long pid_t;

#define SYS_EXIT   0
#define SYS_FORK   1
#define SYS_EXEC   2
#define SYS_WAIT   3
#define SYS_GETPID 4
#define SYS_MMAP   5
#define SYS_MUNMAP 6
#define SYS_BRK    7
#define SYS_CLONE  8
#define SYS_WRITE  9
#define SYS_READ   10
#define SYS_OPEN   11
#define SYS_CLOSE  12
#define SYS_PIPE   13
#define SYS_DUP2   23

static long __syscall6(long number, long a1, long a2, long a3, long a4, long a5, long a6) {
#ifdef __x86_64__
    long ret;
    register long r10 asm("r10") = a4;
    register long r8 asm("r8") = a5;
    register long r9 asm("r9") = a6;
    asm volatile("syscall"
                 : "=a"(ret)
                 : "a"(number), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9)
                 : "rcx", "r11", "memory");
    return ret;
#elif defined(__aarch64__)
    register long x8 asm("x8") = number;
    register long x0 asm("x0") = a1;
    register long x1 asm("x1") = a2;
    register long x2 asm("x2") = a3;
    register long x3 asm("x3") = a4;
    register long x4 asm("x4") = a5;
    register long x5 asm("x5") = a6;
    asm volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5) : "memory");
    return x0;
#else
#error "Unsupported architecture"
#endif
}

// Enhanced memory management
static void *heap_start = NULL;
static void *heap_current = NULL;

void *malloc(size_t size) {
    if (!heap_start) {
        heap_start = __syscall6(SYS_BRK, 0, 0, 0, 0, 0, 0);
        heap_current = heap_start;
    }
    
    // Simple first-fit allocator
    size_t total_size = size + sizeof(size_t); // Store size at beginning
    if (heap_current + total_size > heap_start + 4096) { // 4KB heap for now
        return NULL;
    }
    
    void *ptr = heap_current;
    *(size_t*)ptr = size;
    heap_current += total_size;
    return (char*)ptr + sizeof(size_t);
}

void free(void *ptr) {
    // Simple allocator - don't actually free for now
    (void)ptr;
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    if (ptr) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) {
        return malloc(size);
    }
    
    size_t old_size = *(size_t*)((char*)ptr - sizeof(size_t));
    void *new_ptr = malloc(size);
    if (new_ptr) {
        size_t copy_size = old_size < size ? old_size : size;
        memcpy(new_ptr, ptr, copy_size);
        free(ptr);
    }
    return new_ptr;
}

// Enhanced string functions
void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    if (s < d) {
        // Copy backwards
        for (size_t i = n; i > 0; i--) {
            d[i-1] = s[i-1];
        }
    } else {
        // Copy forwards
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            return a[i] - b[i];
        }
    }
    return 0;
}

char *strcpy(char *dest, const char *src) {
    char *ret = dest;
    while ((*dest++ = *src++)) {
    }
    return ret;
}

char *strcat(char *dest, const char *src) {
    char *ret = dest;
    while (*dest) dest++;
    while ((*dest++ = *src++)) {
    }
    return ret;
}

char *strncat(char *dest, const char *src, size_t n) {
    char *ret = dest;
    while (*dest) dest++;
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    dest[i] = '\0';
    return ret;
}

int strcoll(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

size_t strxfrm(char *dest, const char *src, size_t n) {
    size_t len = strlen(src);
    if (n > 0) {
        strncpy(dest, src, n - 1);
        dest[n - 1] = '\0';
    }
    return len;
}

char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *ret = malloc(len);
    if (ret) {
        memcpy(ret, s, len);
    }
    return ret;
}

char *strndup(const char *s, size_t n) {
    size_t len = 0;
    while (len < n && s[len]) {
        len++;
    }
    char *ret = malloc(len + 1);
    if (ret) {
        memcpy(ret, s, len);
        ret[len] = '\0';
    }
    return ret;
}

char *strchr(const char *s, int c) {
    while (*s && *s != (char)c) {
        s++;
    }
    return (*s == (char)c) ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

char *strcasestr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && tolower(*h) == tolower(*n)) {
            h++;
            n++;
        }
        if (!*n) return (char *)haystack;
        haystack++;
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept) {
    size_t i = 0;
    while (s[i] && strchr(accept, s[i])) {
        i++;
    }
    return i;
}

size_t strcspn(const char *s, const char *reject) {
    size_t i = 0;
    while (s[i] && !strchr(reject, s[i])) {
        i++;
    }
    return i;
}

char *strtok_r(char *str, const char *delim, char **saveptr) {
    if (str) {
        *saveptr = str;
    }
    if (!*saveptr || !**saveptr) {
        return NULL;
    }
    
    // Skip leading delimiters
    char *start = *saveptr;
    while (*start && strchr(delim, *start)) {
        start++;
    }
    
    if (!*start) {
        *saveptr = start;
        return NULL;
    }
    
    // Find end of token
    char *end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }
    
    if (*end) {
        *end = '\0';
        *saveptr = end + 1;
    } else {
        *saveptr = end;
    }
    
    return start;
}

char *strtok(char *str, const char *delim) {
    static char *saveptr = NULL;
    return strtok_r(str, delim, &saveptr);
}

// Character classification functions
int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int isalnum(int c) {
    return isalpha(c) || isdigit(c);
}

int isprint(int c) {
    return c >= 32 && c < 127;
}

int tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

int toupper(int c) {
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

// Enhanced stdio functions
FILE *fopen(const char *pathname, const char *mode) {
    FILE *stream = malloc(sizeof(FILE));
    if (!stream) {
        return NULL;
    }
    
    // Map mode to VFS flags
    int flags = 0;
    if (strchr(mode, 'w')) flags |= 0x02; // O_WRITE
    if (strchr(mode, 'r')) flags |= 0x01; // O_READ
    if (strchr(mode, 'a')) flags |= 0x10; // O_APPEND
    
    stream->fd = vfs_open_stub(pathname, flags);
    if (stream->fd < 0) {
        free(stream);
        return NULL;
    }
    
    stream->buffer = NULL;
    stream->buffer_size = 0;
    stream->position = 0;
    stream->size = 0;
    
    return stream;
}

int fclose(FILE *stream) {
    if (stream->buffer) {
        free(stream->buffer);
    }
    vfs_close_stub(stream->fd);
    free(stream);
    return 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t total = size * nmemb;
    long bytes = vfs_read_stub(stream->fd, total, ptr);
    return (bytes > 0) ? bytes / size : 0;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t total = size * nmemb;
    long bytes = vfs_write_stub(stream->fd, total, ptr);
    return (bytes > 0) ? bytes / size : 0;
}

char *fgets(char *s, int size, FILE *stream) {
    int i = 0;
    while (i < size - 1) {
        char c;
        if (vfs_read_stub(stream->fd, 1, &c) <= 0) {
            break;
        }
        s[i++] = c;
        if (c == '\n') {
            break;
        }
    }
    s[i] = '\0';
    return (i > 0) ? s : NULL;
}

int fputs(const char *s, FILE *stream) {
    size_t len = strlen(s);
    long written = vfs_write_stub(stream->fd, len, s);
    return (written == (long)len) ? 0 : EOF;
}

int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vprintf(format, args);
    va_end(args);
    return result;
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vfprintf(stream, format, args);
    va_end(args);
    return result;
}

int sprintf(char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsprintf(str, format, args);
    va_end(args);
    return result;
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsnprintf(str, size, format, args);
    va_end(args);
    return result;
}

// Enhanced stdlib functions
long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long result = 0;
    int sign = 1;
    
    while (isspace(*s)) s++;
    
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    
    if (base == 0 || base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16;
            s += 2;
        } else if (base == 0) {
            base = 10;
        }
    }
    
    if (base < 2 || base > 36) {
        if (endptr) *endptr = (char *)nptr;
        return 0;
    }
    
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        result = result * base + digit;
        s++;
    }
    
    if (endptr) {
        *endptr = (char *)s;
    }
    
    return sign * (long)result;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long result = 0;
    
    while (isspace(*s)) s++;
    
    if (*s == '+') s++;
    
    if (base == 0 || base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16;
            s += 2;
        } else if (base == 0) {
            base = 10;
        }
    }
    
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        result = result * base + digit;
        s++;
    }
    
    if (endptr) {
        *endptr = (char *)s;
    }
    
    return result;
}

double strtod(const char *nptr, char **endptr) {
    // Simple implementation for now
    (void)nptr;
    (void)endptr;
    return 0.0;
}

int abs(int j) {
    return j < 0 ? -j : j;
}

long labs(long j) {
    return j < 0 ? -j : j;
}

long long llabs(long long j) {
    return j < 0 ? -j : j;
}

int atoi(const char *nptr) {
    return (int)strtol(nptr, NULL, 10);
}

long atol(const char *nptr) {
    return strtol(nptr, NULL, 10);
}

long long atoll(const char *nptr) {
    return strtoll(nptr, NULL, 10);
}

long long strtoll(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long long result = 0;
    int sign = 1;
    
    while (isspace(*s)) s++;
    
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }
    
    if (base == 0 || base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16;
            s += 2;
        } else if (base == 0) {
            base = 10;
        }
    }
    
    if (base < 2 || base > 36) {
        if (endptr) *endptr = (char *)nptr;
        return 0;
    }
    
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        result = result * base + digit;
        s++;
    }
    
    if (endptr) {
        *endptr = (char *)s;
    }
    
    return sign * (long long)result;
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    unsigned long long result = 0;
    
    while (isspace(*s)) s++;
    
    if (*s == '+') s++;
    
    if (base == 0 || base == 16) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            base = 16;
            s += 2;
        } else if (base == 0) {
            base = 10;
        }
    }
    
    while (*s) {
        int digit;
        if (*s >= '0' && *s <= '9') {
            digit = *s - '0';
        } else if (*s >= 'a' && *s <= 'z') {
            digit = *s - 'a' + 10;
        } else if (*s >= 'A' && *s <= 'Z') {
            digit = *s - 'A' + 10;
        } else {
            break;
        }
        
        if (digit >= base) {
            break;
        }
        
        result = result * base + digit;
        s++;
    }
    
    if (endptr) {
        *endptr = (char *)s;
    }
    
    return result;
}

// Simple random number generator
static unsigned int seed = 1;

int rand(void) {
    seed = seed * 1103515245 + 12345;
    return (seed >> 16) & 0x7FFF;
}

void srand(unsigned int new_seed) {
    seed = new_seed ? new_seed : 1;
}

// Environment variable support (simplified)
#define MAX_ENV_VARS 64
#define MAX_ENV_VAR_SIZE 256

static char env_vars[MAX_ENV_VARS][MAX_ENV_VAR_SIZE];
static int env_count = 0;

char *getenv(const char *name) {
    for (int i = 0; i < env_count; i++) {
        char *eq = strchr(env_vars[i], '=');
        if (eq && strncmp(env_vars[i], name, eq - env_vars[i]) == 0) {
            return eq + 1;
        }
    }
    return NULL;
}

int setenv(const char *name, const char *value, int overwrite) {
    if (!name || !value || strlen(name) + strlen(value) + 1 >= MAX_ENV_VAR_SIZE) {
        return -1;
    }
    
    // Check if already exists
    for (int i = 0; i < env_count; i++) {
        char *eq = strchr(env_vars[i], '=');
        if (eq && strncmp(env_vars[i], name, eq - env_vars[i]) == 0) {
            if (!overwrite) return 0;
            snprintf(env_vars[i], MAX_ENV_VAR_SIZE, "%s=%s", name, value);
            return 0;
        }
    }
    
    // Add new variable
    if (env_count < MAX_ENV_VARS) {
        snprintf(env_vars[env_count++], MAX_ENV_VAR_SIZE, "%s=%s", name, value);
        return 0;
    }
    
    return -1;
}

int unsetenv(const char *name) {
    for (int i = 0; i < env_count; i++) {
        char *eq = strchr(env_vars[i], '=');
        if (eq && strncmp(env_vars[i], name, eq - env_vars[i]) == 0) {
            // Shift remaining variables down
            for (int j = i; j < env_count - 1; j++) {
                strcpy(env_vars[j], env_vars[j + 1]);
            }
            env_count--;
            return 0;
        }
    }
    return -1;
}

// Variadic argument handling
#include <stdarg.h>

// File mode constants
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)

#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_IFLNK  0120000
#define S_IFIFO  0010000
#define S_IFSOCK 0140000
#define S_IFCHR  0020000
#define S_IFBLK  0060000

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

int vprintf(const char *format, va_list args) {
    char buffer[256];
    int written = vsnprintf(buffer, sizeof(buffer), format, args);
    write(1, buffer, written);
    return written;
}

int vfprintf(FILE *stream, const char *format, va_list args) {
    char buffer[256];
    int written = vsnprintf(buffer, sizeof(buffer), format, args);
    vfs_write_stub(stream->fd, written, buffer);
    return written;
}

int vsprintf(char *str, const char *format, va_list args) {
    // Simple vsprintf implementation
    const char *p = format;
    char *dest = str;
    int written = 0;
    
    while (*p) {
        if (*p == '%' && *(p + 1)) {
            p++; // Skip %
            if (*p == 's') {
                char *arg = va_arg(args, char*);
                while (*arg) {
                    *dest++ = *arg++;
                    written++;
                }
            } else if (*p == 'd') {
                int arg = va_arg(args, int);
                char num_buf[20];
                int len = snprintf(num_buf, sizeof(num_buf), "%d", arg);
                memcpy(dest, num_buf, len);
                dest += len;
                written += len;
            } else {
                *dest++ = *p;
                written++;
            }
        } else {
            *dest++ = *p;
            written++;
        }
        p++;
    }
    *dest = '\0';
    return written;
}

int vsnprintf(char *str, size_t size, const char *format, va_list args) {
    int result = vsprintf(str, format, args);
    if (size > 0 && result >= (int)size) {
        str[size - 1] = '\0';
    }
    return result;
}

// Other useful functions
int puts(const char *s) {
    size_t n = strlen(s);
    write(1, s, n);
    write(1, "\n", 1);
    return 0;
}

int putchar(int c) {
    char ch = (char)c;
    write(1, &ch, 1);
    return c;
}

int getchar(void) {
    char ch;
    read(0, &ch, 1);
    return (int)(unsigned char)ch;
}

// Term support
termios_t current_term;

void term_init(void) {
    current_term.has_colors = 1;
    current_term.max_colors = 256;
    current_term.lines = 24;
    current_term.cols = 80;
    current_term.bold_supported = 1;
    current_term.underline_supported = 1;
    current_term.alt_screen = 1;
    current_term.bracketed_paste = 1;
}

void term_get_size(int *lines, int *cols) {
    if (lines) *lines = current_term.lines;
    if (cols) *cols = current_term.cols;
}

int term_isatty(void) {
    return 1; // Assume we're interactive
}

void term_set_title(const char *title) {
    char seq[128];
    snprintf(seq, sizeof(seq), TERM_TITLE_SET, title);
    write(1, seq, strlen(seq));
}

void term_cursor_up(int n) {
    char seq[16];
    snprintf(seq, sizeof(seq), TERM_CURSOR_UP(n));
    write(1, seq, strlen(seq));
}

void term_cursor_down(int n) {
    char seq[16];
    snprintf(seq, sizeof(seq), TERM_CURSOR_DOWN(n));
    write(1, seq, strlen(seq));
}

void term_cursor_forward(int n) {
    char seq[16];
    snprintf(seq, sizeof(seq), TERM_CURSOR_FORWARD(n));
    write(1, seq, strlen(seq));
}

void term_cursor_back(int n) {
    char seq[16];
    snprintf(seq, sizeof(seq), TERM_CURSOR_BACK(n));
    write(1, seq, strlen(seq));
}

void term_cursor_home(void) {
    write(1, TERM_CURSOR_HOME, strlen(TERM_CURSOR_HOME));
}

void term_clear_screen(void) {
    write(1, TERM_CLEAR_SCREEN, strlen(TERM_CLEAR_SCREEN));
    write(1, TERM_CURSOR_HOME, strlen(TERM_CURSOR_HOME));
}

void term_clear_eol(void) {
    write(1, TERM_CLEAR_EOL, strlen(TERM_CLEAR_EOL));
}

void term_clear_bos(void) {
    write(1, TERM_CLEAR_BOS, strlen(TERM_CLEAR_BOS));
}

void term_clear_eos(void) {
    write(1, TERM_CLEAR_EOS, strlen(TERM_CLEAR_EOS));
}

void term_alternate_screen(int on) {
    if (on) {
        write(1, TERM_ALT_SCREEN_ON, strlen(TERM_ALT_SCREEN_ON));
    } else {
        write(1, TERM_ALT_SCREEN_OFF, strlen(TERM_ALT_SCREEN_OFF));
    }
}

void term_enable_bracketed_paste(void) {
    write(1, TERM_BRACKETED_PASTE_ON, strlen(TERM_BRACKETED_PASTE_ON));
}

void term_disable_bracketed_paste(void) {
    write(1, TERM_BRACKETED_PASTE_OFF, strlen(TERM_BRACKETED_PASTE_OFF));
}

const char* term_color_name_to_code(const char *name) {
    if (!name || !name[0]) return COLOR_RESET;
    
    if (strcmp(name, "red") == 0) return COLOR_FG_RED;
    if (strcmp(name, "green") == 0) return COLOR_FG_GREEN;
    if (strcmp(name, "blue") == 0) return COLOR_FG_BLUE;
    if (strcmp(name, "yellow") == 0) return COLOR_FG_YELLOW;
    if (strcmp(name, "magenta") == 0) return COLOR_FG_MAGENTA;
    if (strcmp(name, "cyan") == 0) return COLOR_FG_CYAN;
    if (strcmp(name, "white") == 0) return COLOR_FG_WHITE;
    if (strcmp(name, "bold") == 0) return COLOR_BOLD;
    if (strcmp(name, "dim") == 0) return COLOR_DIM;
    if (strcmp(name, "underline") == 0) return COLOR_UNDERLINE;
    
    return COLOR_RESET;
}

// Missing stdio/stdlib functions
int fflush(FILE *stream) {
    (void)stream; // No buffering implemented yet
    return 0;
}

int fseek(FILE *stream, long offset, int whence) {
    (void)stream;
    (void)offset;
    (void)whence;
    return 0; // Success for now
}

long ftell(FILE *stream) {
    (void)stream;
    return 0; // No file position tracking yet
}

int ferror(FILE *stream) {
    (void)stream;
    return 0; // No error tracking yet
}

void clearerr(FILE *stream) {
    (void)stream;
}

int getc(FILE *stream) {
    char c;
    if (vfs_read(stream->fd, 1, &c) > 0) {
        return (int)(unsigned char)c;
    }
    return EOF;
}

int fgetc(FILE *stream) {
    return getc(stream);
}

int putc(int c, FILE *stream) {
    char ch = (char)c;
    if (vfs_write(stream->fd, 1, &ch) > 0) {
        return c;
    }
    return EOF;
}

int fputc(int c, FILE *stream) {
    return putc(c, stream);
}

// Additional missing functions
void *aligned_alloc(size_t alignment, size_t size) {
    (void)alignment; // Simple implementation
    return malloc(size);
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
    *memptr = aligned_alloc(alignment, size);
    return (*memptr != NULL) ? 0 : -1;
}

void abort(void) {
    exit(1);
}

void _Exit(int status) {
    exit(status);
}

int atexit(void (*func)(void)) {
    (void)func;
    return 0; // Simple implementation
}

long random(void) {
    return rand();
}

void srandomdev(void) {
    srand(123456789);
}

void *srandom(unsigned int seed) {
    (void)seed;
    return NULL;
}

int putenv(char *string) {
    char *eq = strchr(string, '=');
    if (!eq) return -1;
    
    *eq = '\0';
    int result = setenv(string, eq + 1, 1);
    *eq = '=';
    return result;
}

int clearenv(void) {
    env_count = 0;
    return 0;
}

void qsort(void *base, size_t nmemb, size_t size, 
           int (*compar)(const void *, const void *)) {
    (void)base;
    (void)nmemb;
    (void)size;
    (void)compar;
    // Simple bubble sort implementation could go here
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *)) {
    const char *base_ptr = (const char *)base;
    size_t low = 0, high = nmemb;
    
    while (low < high) {
        size_t mid = (low + high) / 2;
        const char *elem = base_ptr + mid * size;
        int result = compar(key, elem);
        
        if (result == 0) {
            return (void *)elem;
        } else if (result < 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }
    return NULL;
}

// Missing sys/wait.h functions and definitions
#define WIFEXITED(status) (((status) & 0x7f) == 0)
#define WIFSIGNALED(status) (((status) & 0x7f) != 0 && ((status) & 0x7f) < 0x7f)
#define WIFSTOPPED(status) (((status) & 0xff) == 0x7f)
#define WTERMSIG(status) ((status) & 0x7f)
#define WSTOPSIG(status) (((status) >> 8) & 0xff)

int waitpid(int pid, int *status, int options) {
    (void)pid;
    (void)status;
    (void)options;
    // Simple implementation - just call wait()
    return wait(0);
}

// WEXITSTATUS is a macro, not a function
int wait_status_exit_code(int status) {
    return ((status) >> 8) & 0xff;
}

// Missing unistd.h functions
int access(const char *pathname, int mode) {
    (void)pathname;
    (void)mode;
    // Simple implementation - assume file exists for testing
    return 0;
}

char *getcwd(char *buf, size_t size) {
    char *pwd = getenv("PWD");
    if (!pwd) pwd = "/";
    if (buf && size > 0) {
        strncpy(buf, pwd, size - 1);
        buf[size - 1] = '\0';
        return buf;
    }
    return strdup(pwd);
}

int chdir(const char *path) {
    (void)path;
    // TODO: Implement actual directory change using VFS
    return 0;
}

int dup(int oldfd) {
    (void)oldfd;
    // Simple implementation - return new fd
    return 3; // Next available fd
}

int dup2(int oldfd, int newfd) {
    (void)oldfd;
    (void)newfd;
    // Simple implementation
    return newfd;
}

int close(int fd) {
    return vfs_close(fd);
}

int pipe(int pipefd[2]) {
    (void)pipefd;
    // Simple implementation
    return 0;
}

// Missing dirent.h functions
struct dirent {
    long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    char d_name[256];
};

typedef struct {
    void *handle;
    struct dirent entry;
} DIR;

DIR *opendir(const char *name) {
    (void)name;
    DIR *dir = malloc(sizeof(DIR));
    return dir;
}

struct dirent *readdir(DIR *dirp) {
    (void)dirp;
    // Simple implementation - return dummy entry
    static struct dirent dummy;
    dummy.d_ino = 1;
    strcpy(dummy.d_name, ".");
    return &dummy;
}

int closedir(DIR *dirp) {
    free(dirp);
    return 0;
}

// Missing sys/stat.h functions
int stat(const char *pathname, struct stat *statbuf) {
    (void)pathname;
    (void)statbuf;
    // Simple implementation
    return 0;
}

int fstat(int fd, struct stat *statbuf) {
    (void)fd;
    (void)statbuf;
    // Simple implementation
    return 0;
}

int mkdir(const char *pathname, mode_t mode) {
    (void)pathname;
    (void)mode;
    // TODO: Implement using VFS
    return 0;
}

int rmdir(const char *pathname) {
    (void)pathname;
    // TODO: Implement using VFS
    return 0;
}

int unlink(const char *pathname) {
    (void)pathname;
    // TODO: Implement using VFS
    return 0;
}

mode_t umask(mode_t mask) {
    (void)mask;
    return 0;
}

// Missing signal.h functions
typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler) {
    (void)signum;
    (void)handler;
    return NULL;
}

int kill(pid_t pid, int sig) {
    (void)pid;
    (void)sig;
    return 0;
}

unsigned int alarm(unsigned int seconds) {
    (void)seconds;
    return 0;
}

void pause(void) {
    // Simple implementation
    for (;;) {
        // Just spin for now
        volatile int dummy = 0;
        (void)dummy;
    }
}

int sleep(unsigned int seconds) {
    (void)seconds;
    return 0;
}

// Missing time.h functions
typedef long time_t;
typedef long suseconds_t;

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (tv) {
        tv->tv_sec = 0;
        tv->tv_usec = 0;
    }
    return 0;
}

time_t time(time_t *tloc) {
    time_t now = 0;
    if (tloc) {
        *tloc = now;
    }
    return now;
}

struct tm *localtime(const time_t *timep) {
    (void)timep;
    static struct tm dummy_tm;
    return &dummy_tm;
}

// Add ctime function for time conversion
char *ctime(const time_t *timep) {
    (void)timep;
    static char dummy_time[] = "Mon Jan  1 00:00:00 1970\n";
    return dummy_time;
}

// Add mktime function
time_t mktime(struct tm *tm) {
    (void)tm;
    return 0;
}

// Add asctime function
char *asctime(const struct tm *tm) {
    (void)tm;
    static char dummy_time[] = "Mon Jan  1 00:00:00 1970\n";
    return dummy_time;
}

// Add missing type definitions
typedef unsigned int mode_t;
typedef unsigned int nlink_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef long pid_t;

// Missing sys/stat.h structures
struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    unsigned long st_size;
    unsigned long st_blksize;
    unsigned long st_blocks;
    unsigned long st_atime;
    unsigned long st_mtime;
    unsigned long st_ctime;
};

// Missing file mode constants
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)

#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFREG  0100000
#define S_IFLNK  0120000
#define S_IFIFO  0010000
#define S_IFSOCK 0140000
#define S_IFCHR  0020000
#define S_IFBLK  0060000

#define S_IRWXU 00700
#define S_IRUSR 00400
#define S_IWUSR 00200
#define S_IXUSR 00100
#define S_IRWXG 00070
#define S_IRGRP 00040
#define S_IWGRP 00020
#define S_IXGRP 00010
#define S_IRWXO 00007
#define S_IROTH 00004
#define S_IWOTH 00002
#define S_IXOTH 00001

// VFS function declarations (forward declarations)
extern int vfs_open(const char *pathname, unsigned int flags);
extern int vfs_close(unsigned int fd);
extern long vfs_read(unsigned int fd, unsigned long size, void *buf);
extern long vfs_write(unsigned int fd, unsigned long size, const void *buf);

// VFS stub implementations for compilation
static int vfs_open_stub(const char *pathname, unsigned int flags) {
    (void)pathname;
    (void)flags;
    return 3; // Return dummy fd
}

static int vfs_close_stub(unsigned int fd) {
    (void)fd;
    return 0;
}

static long vfs_read_stub(unsigned int fd, unsigned long size, void *buf) {
    (void)fd;
    (void)size;
    (void)buf;
    return 0;
}

static long vfs_write_stub(unsigned int fd, unsigned long size, const void *buf) {
    (void)fd;
    (void)size;
    (void)buf;
    return 0;
}

// Forward declaration for ssize_t
typedef long ssize_t;

// Other syscalls
long write(int fd, const void *buf, size_t len) {
    return __syscall6(SYS_WRITE, fd, (long)buf, len, 0, 0, 0);
}

long read(int fd, void *buf, size_t len) {
    return __syscall6(SYS_READ, fd, (long)buf, len, 0, 0, 0);
}

int fork(void) {
    return (int)__syscall6(SYS_FORK, 0, 0, 0, 0, 0, 0);
}

int exec(const char *path, const char *const argv[]) {
    return (int)__syscall6(SYS_EXEC, (long)path, (long)argv, 0, 0, 0, 0);
}

int wait(int pid) {
    return (int)__syscall6(SYS_WAIT, pid, 0, 0, 0, 0, 0);
}

int waitpid(int pid, int *status) {
    return (int)__syscall6(SYS_WAIT, pid, (long)status, 0, 0, 0, 0);
}

int pipe(int pipefd[2]) {
    return (int)__syscall6(SYS_PIPE, (long)pipefd, 0, 0, 0, 0, 0);
}

int dup2(int oldfd, int newfd) {
    return (int)__syscall6(SYS_DUP2, oldfd, newfd, 0, 0, 0, 0);
}

int close(int fd) {
    return (int)__syscall6(SYS_CLOSE, fd, 0, 0, 0, 0, 0);
}

int getpid(void) {
    return (int)__syscall6(SYS_GETPID, 0, 0, 0, 0, 0, 0);
}

void exit(int code) {
    __syscall6(SYS_EXIT, code, 0, 0, 0, 0, 0);
    for (;;)
        ;
}

int errno;

int *__errno_location(void) {
    return &errno;
}

// Entry point
extern int main(int argc, char **argv);

void kuser_entry(unsigned long *stack) {
    int argc = 0;
    char **argv = 0;
    if (stack) {
        argc = (int)(*stack);
        argv = (char **)(stack + 1);
    }
    
    // Initialize terminal
    term_init();
    
    int ret = main(argc, argv);
    exit(ret);
}