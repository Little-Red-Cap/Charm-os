#include "elf_hostcall.h"

volatile struct ElfHostCalls elf_hostcalls_table;

static int parse_int(const char* s) {
    int sign = 1;
    int v = 0;
    if (!s) return 0;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return v * sign;
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    if (argc < 2 || !argv || !argv[1]) {
        _exit(0);
    }
    _exit(parse_int(argv[1]));
    write(1, "after-exit\n", 11);
    return 99;
}
