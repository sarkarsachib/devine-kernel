#include "../../libc/include/kuser.h"

int main(void) {
    const char msg[] = "ls: VFS stubs - no directory listing\n";
    write(1, msg, sizeof(msg) - 1);
    return 0;
}
