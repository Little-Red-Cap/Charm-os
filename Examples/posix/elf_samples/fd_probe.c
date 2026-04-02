#include "elf_hostcall.h"

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

volatile struct ElfHostCalls elf_hostcalls_table;

static int write_all(int fd, const char* buf, unsigned long len) {
    unsigned long off = 0;
    while (off < len) {
        int w = write(fd, buf + off, len - off);
        if (w <= 0) return -1;
        off += (unsigned long)w;
    }
    return 0;
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    const char* path = "/missing.txt";
    if (argc > 1 && argv && argv[1]) {
        path = argv[1];
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd >= 0) {
        write_all(1, "open-ok\n", 8);
        close(fd);
        return 21;
    }

    if (errno != 2) {
        write_all(1, "errno-bad\n", 10);
        return 22;
    }

    if (isatty(-1) != 0) {
        write_all(1, "isatty-bad\n", 11);
        return 23;
    }

    if (errno != 0) {
        write_all(1, "isatty-errno\n", 13);
        return 24;
    }

    PosixStat st;
    if (fstat(-1, &st) != -1) {
        write_all(1, "fstat-ok\n", 9);
        return 25;
    }

    if (errno != 5) {
        write_all(1, "fstat-errno\n", 12);
        return 26;
    }

    write_all(1, "errno-ok\n", 9);
    return 0;
}
