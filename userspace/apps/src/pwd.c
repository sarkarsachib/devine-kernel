#include "../../libc/include/string.h"
#include "../../libc/include/stdlib.h"
#include "../../libc/include/stdio.h"
#include <stdarg.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: pwd\n");
        return 1;
    }
    
    char *pwd = getenv("PWD");
    if (!pwd) {
        printf("Cannot get current directory\n");
        return 1;
    }
    
    printf("%s\n", pwd);
    return 0;
}