#include "elf_hostcall.h"

volatile struct ElfHostCalls elf_hostcalls_table;

int entry(int argc, char** argv, char** envp) {
    (void)argc;
    (void)argv;
    (void)envp;

    char buf[16];
    int pid = getpid();
    unsigned int value = pid < 0 ? 0u : (unsigned int)pid;
    int len = 0;

    if (value == 0u) {
        buf[len++] = '0';
    } else {
        char tmp[16];
        int tmp_len = 0;
        while (value > 0u && tmp_len < (int)sizeof(tmp)) {
            tmp[tmp_len++] = (char)('0' + (value % 10u));
            value /= 10u;
        }
        while (tmp_len > 0) {
            buf[len++] = tmp[--tmp_len];
        }
    }

    buf[len++] = '\n';
    write(1, buf, (unsigned long)len);
    return 0;
}
