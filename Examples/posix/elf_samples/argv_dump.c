#include "elf_hostcall.h"

volatile struct ElfHostCalls elf_hostcalls_table;

static unsigned long cstr_len(const char* s) {
    unsigned long n = 0;
    if (!s) return 0;
    while (s[n] != '\0') n++;
    return n;
}

static void write_str(const char* s) {
    if (!s) return;
    elf_hostcalls()->write(1, s, cstr_len(s));
}

static void write_uint(unsigned int v) {
    char buf[12];
    int i = 0;
    if (v == 0) {
        buf[i++] = '0';
    } else {
        while (v > 0 && i < 10) {
            buf[i++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    for (int j = i - 1; j >= 0; --j) {
        elf_hostcalls()->write(1, &buf[j], 1);
    }
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    for (int i = 0; i < argc; ++i) {
        write_str("argv[");
        write_uint((unsigned int)i);
        write_str("]=");
        if (argv && argv[i]) {
            write_str(argv[i]);
        }
        write_str("\n");
    }
    return 0;
}
