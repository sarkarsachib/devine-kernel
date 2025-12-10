#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

// Basic type definitions
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef unsigned long  u64;

typedef signed char    i8;
typedef signed short   i16;
typedef signed int     i32;
typedef signed long    i64;

// Size-independent types
typedef u64 usize;
typedef i64 isize;

// Common constants
#define ALIGN_UP(x, align)   (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))

// Bit manipulation
#define BIT(n) (1UL << (n))

// String length
#define MAX_STRING_LEN 256

// Memory addresses
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000UL
#define PAGE_SIZE 0x1000UL

// CPU constants
#define MAX_CPUS 64
#define STACK_SIZE 8192

// Capability definitions
#define CAP_NONE         0x00000000
#define CAP_READ         0x00000001
#define CAP_WRITE        0x00000002
#define CAP_EXEC         0x00000004
#define CAP_CREATE       0x00000008
#define CAP_DELETE       0x00000010
#define CAP_ADMIN        0x00000020
#define CAP_ALL          0x0000003F

// Error codes
#define ERR_SUCCESS      0
#define ERR_INVALID     -1
#define ERR_NOT_FOUND   -2
#define ERR_PERMISSION  -3
#define ERR_NO_MEMORY   -4
#define ERR_BUSY        -5
#define ERR_AGAIN       -6