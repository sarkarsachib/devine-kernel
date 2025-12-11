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
//
// Note: CAP_READ/CAP_WRITE/etc are legacy permission-style bits used by some
// of the C subsystems. Syscall authorization uses a separate, stable namespace
// of capabilities in the upper bits.
#define CAP_NONE         0x00000000
#define CAP_READ         0x00000001
#define CAP_WRITE        0x00000002
#define CAP_EXEC         0x00000004
#define CAP_CREATE       0x00000008
#define CAP_DELETE       0x00000010
#define CAP_ADMIN        0x00000020
#define CAP_ALL          0x0000003F

// Canonical syscall capability bits
#define CAP_PROC_MANAGE  (1ULL << 16)
#define CAP_VM_MANAGE    (1ULL << 17)
#define CAP_CONSOLE_IO   (1ULL << 18)
#define CAP_FS_RW        (1ULL << 19)
#define CAP_IPC          (1ULL << 20)
#define CAP_SYS_ADMIN    (1ULL << 63)

// Error codes
#define ERR_SUCCESS      0
#define ERR_INVALID     -1
#define ERR_NOT_FOUND   -2
#define ERR_PERMISSION  -3
#define ERR_NO_MEMORY   -4
#define ERR_BUSY        -5
#define ERR_AGAIN       -6

// File types for VFS
#define S_IFREG    0x1000
#define S_IFDIR    0x2000
#define S_IFLNK    0x3000
#define S_IFCHR    0x4000
#define S_IFBLK    0x5000
#define S_IFIFO    0x6000
#define S_IFSOCK   0x7000