#include "../include/kuser.h"

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

extern int main(int argc, char **argv);

void kuser_entry(unsigned long *stack) {
    int argc = 0;
    char **argv = 0;
    if (stack) {
        argc = (int)(*stack);
        argv = (char **)(stack + 1);
    }
    int ret = main(argc, argv);
    exit(ret);
}

int errno;

int *__errno_location(void) {
    return &errno;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    return dest;
}

void *memset(void *dest, int c, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    for (size_t i = 0; i < n; ++i) {
        d[i] = (unsigned char)c;
    }
    return dest;
}

size_t strlen(const char *s) {
    size_t n = 0;
    if (!s) {
        return 0;
    }
    while (s[n]) {
        ++n;
    }
    return n;
}

int strcmp(const char *a, const char *b) {
    size_t i = 0;
    while (a[i] && b[i] && a[i] == b[i]) {
        ++i;
    }
    return (unsigned char)a[i] - (unsigned char)b[i];
}

int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i] || !a[i] || !b[i]) {
            return (unsigned char)a[i] - (unsigned char)b[i];
        }
    }
    return 0;
}

int puts(const char *s) {
    size_t n = strlen(s);
    write(1, s, n);
    write(1, "\n", 1);
    return 0;
}
