#include "elf_hostcall.h"

volatile struct ElfHostCalls elf_hostcalls_table;

int entry(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    write(1, "before-kill\n", 12);
    if (kill(getpid(), SIGTERM) != 0) {
        write(1, "kill-failed\n", 12);
        return 4;
    }
    write(1, "after-kill\n", 11);
    return 5;
}
