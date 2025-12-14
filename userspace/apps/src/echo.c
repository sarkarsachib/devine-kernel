#include "../../libc/include/string.h"
#include "../../libc/include/stdlib.h"
#include "../../libc/include/stdio.h"
#include <stdarg.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("usage: echo [string...]\n");
        return 1;
    }
    
    for (int i = 1; i < argc; i++) {
        if (i > 1) printf(" ");
        printf("%s", argv[i]);
    }
    printf("\n");
    return 0;
}