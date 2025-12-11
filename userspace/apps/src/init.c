#include "../../libc/include/kuser.h"

static const char *shell_args[] = {"/bin/sh", 0};

int main(void) {
    const char msg[] = "init: launching shell\n";
    write(1, msg, sizeof(msg) - 1);

    int pid = fork();
    if (pid == 0) {
        exec("/bin/sh", shell_args);
        exit(1);
    }

    wait(pid);
    const char done[] = "init: shell exited\n";
    write(1, done, sizeof(done) - 1);
    return 0;
}
