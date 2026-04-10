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
    write(1, s, cstr_len(s));
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
        write(1, &buf[j], 1);
    }
}

int entry(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;
    if (!envp) return 0;
    for (int i = 0; envp[i]; ++i) {
        write_str("env[");
        write_uint((unsigned int)i);
        write_str("]=");
        write_str(envp[i]);
        write_str("\n");
    }
    return 0;
}
