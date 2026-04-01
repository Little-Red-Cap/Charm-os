#include "elf_hostcall.h"

volatile struct ElfHostCalls elf_hostcalls_table;

int entry(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;
    (void)envp;
    const char msg[] = "hello\n";
    elf_hostcalls()->write(1, msg, sizeof(msg) - 1);
    return 0;
}
