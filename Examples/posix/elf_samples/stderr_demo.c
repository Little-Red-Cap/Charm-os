#include "elf_hostcall.h"

volatile struct ElfHostCalls elf_hostcalls_table;

int entry(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    const char out[] = "out\n";
    const char err[] = "err\n";
    write(1, out, sizeof(out) - 1);
    write(2, err, sizeof(err) - 1);
    return 0;
}
