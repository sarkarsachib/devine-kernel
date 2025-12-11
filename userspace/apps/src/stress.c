#include "../../libc/include/kuser.h"

int main(void) {
    const char msg[] = "stress: spinning...\n";
    write(1, msg, sizeof(msg) - 1);
    volatile unsigned long counter = 0;
    for (unsigned long i = 0; i < 1000000; ++i) {
        counter += i;
    }
    const char done[] = "stress: done\n";
    write(1, done, sizeof(done) - 1);
    return (int)(counter & 0x7);
}
