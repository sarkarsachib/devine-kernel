#include "../../libc/include/kuser.h"

int main(void) {
    const char msg[] = "cat: device layer not yet available\n";
    write(1, msg, sizeof(msg) - 1);
    return 0;
}
