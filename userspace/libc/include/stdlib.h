#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

void *aligned_alloc(size_t alignment, size_t size);
int posix_memalign(void **memptr, size_t alignment, size_t size);

long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long long strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);

double strtod(const char *nptr, char **endptr);

int abs(int j);
long labs(long j);
long long llabs(long long j);

int atoi(const char *nptr);
long atol(const char *nptr);
long long atoll(const char *nptr);

void qsort(void *base, size_t nmemb, size_t size, 
           int (*compar)(const void *, const void *));
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
              int (*compar)(const void *, const void *));

char *getenv(const char *name);
int setenv(const char *name, const char *value, int overwrite);
int unsetenv(const char *name);
int putenv(char *string);
int clearenv(void);

int rand(void);
void srand(unsigned int seed);
void *srandom(unsigned int seed);
long random(void);
void srandomdev(void);

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

void abort(void);
void exit(int status);
void _Exit(int status);
int atexit(void (*func)(void));

#endif /* STDLIB_H */