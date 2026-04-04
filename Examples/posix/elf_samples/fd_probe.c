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
    const char* path = "/empty.txt";
    if (argc > 1 && argv && argv[1]) {
        path = argv[1];
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        write_all(1, "open-fail\n", 10);
        return 31;
    }

    char ch = 0;
    if (read(fd, &ch, 1) != 0) {
        write_all(1, "eof-bad\n", 8);
        close(fd);
        return 32;
    }

    if (errno != 0) {
        write_all(1, "eof-errno\n", 10);
        close(fd);
        return 33;
    }

    if (read(-1, &ch, 1) != -1) {
        write_all(1, "read-bad\n", 9);
        close(fd);
        return 34;
    }

    if (errno != 95) {
        write_all(1, "read-errno\n", 11);
        close(fd);
        return 35;
    }

    write_all(1, "read-ok\n", 8);
    close(fd);
    return 0;
}
