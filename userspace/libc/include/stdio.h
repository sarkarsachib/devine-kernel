#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

typedef struct {
    int fd;
    void *buffer;
    size_t buffer_size;
    size_t position;
    size_t size;
} FILE;

#define stdin ((FILE *)1)
#define stdout ((FILE *)2)
#define stderr ((FILE *)3)

#define EOF (-1)

FILE *fopen(const char *pathname, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fputs(const char *s, FILE *stream);
int fprintf(FILE *stream, const char *format, ...);
int printf(const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list args);
int vprintf(const char *format, va_list args);
int vsprintf(char *str, const char *format, va_list args);
int vsnprintf(char *str, size_t size, const char *format, va_list args);

int fflush(FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int ferror(FILE *stream);
void clearerr(FILE *stream);

int getc(FILE *stream);
int getchar(void);
int fgetc(FILE *stream);
int putc(int c, FILE *stream);
int putchar(int c);
int fputc(int c, FILE *stream);

#endif /* STDIO_H */