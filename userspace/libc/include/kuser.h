#ifndef KUSER_LIBC_H
#define KUSER_LIBC_H

#include <stddef.h>
#include <stdint.h>

long write(int fd, const void *buf, size_t len);
long read(int fd, void *buf, size_t len);
int fork(void);
int exec(const char *path, const char *const argv[]);

int wait(int pid);
int waitpid(int pid, int *status);

int pipe(int pipefd[2]);
int dup2(int oldfd, int newfd);
int close(int fd);

int getpid(void);
void exit(int code) __attribute__((noreturn));

#endif /* KUSER_LIBC_H */
