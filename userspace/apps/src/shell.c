#include "../../libc/include/kuser.h"
#include <stddef.h>

static const char *stress_args[] = {"/usr/bin/stress", 0};

static int starts_with(const char *buffer, const char *word) {
    while (*word && *buffer && *word == *buffer) {
        ++word;
        ++buffer;
    }
    return *word == '\0';
}

int main(void) {
    const char banner[] = "shell: ready. type 'exit' or 'stress'\n";
    write(1, banner, sizeof(banner) - 1);

    char buffer[128];
    while (1) {
        const char prompt[] = "shell$ ";
        write(1, prompt, sizeof(prompt) - 1);

        long received = read(0, buffer, sizeof(buffer) - 1);
        if (received <= 0) {
            break;
        }
        buffer[received] = '\0';

        if (starts_with(buffer, "exit")) {
            break;
        }
        if (starts_with(buffer, "stress")) {
            write(1, "shell: spawning stress test\n", 29);
            fork();
            exec("/usr/bin/stress", stress_args);
            continue;
        }

        write(1, "shell: unknown command\n", 24);
    }

    write(1, "shell: bye\n", 11);
    return 0;
}
