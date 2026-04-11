#include "elf_hostcall.h"

volatile struct ElfHostCalls elf_hostcalls_table;

static unsigned int parse_uint(const char* s) {
    unsigned int v = 0u;
    if (!s) return 0u;
    while (*s >= '0' && *s <= '9') {
        v = v * 10u + (unsigned int)(*s - '0');
        s++;
    }
    return v;
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    if (argc < 2 || !argv || !argv[1]) {
        return 2;
    }
    if (sleep(parse_uint(argv[1])) != 0) {
        return 4;
    }
    write(1, "slept\n", 6);
    return 0;
}
