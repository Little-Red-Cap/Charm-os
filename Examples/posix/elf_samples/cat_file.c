#include "elf_hostcall.h"

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

volatile struct ElfHostCalls elf_hostcalls_table;

static unsigned long cstr_len(const char* s) {
    unsigned long n = 0;
    if (!s) return 0;
    while (s[n] != '\0') n++;
    return n;
}

static void write_str(int fd, const char* s) {
    if (!s) return;
    elf_hostcalls()->write(fd, s, cstr_len(s));
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    write_str(2, "A\n");

    const char* path = "/cat.txt";
    if (argc > 1 && argv && argv[1]) {
        path = argv[1];
    }

    int fd = elf_hostcalls()->open(path, O_RDONLY, 0);
    if (fd < 0) {
        write_str(2, "open fail\n");
        return 11;
    }
    write_str(2, "B\n");

    PosixStat st;
    if (elf_hostcalls()->fstat(fd, &st) != 0) {
        write_str(2, "fstat fail\n");
        elf_hostcalls()->close(fd);
        return 12;
    }
    write_str(2, "C\n");

    if (elf_hostcalls()->isatty(1) != 1) {
        write_str(2, "stdout not tty\n");
        elf_hostcalls()->close(fd);
        return 13;
    }
    write_str(2, "D\n");

    if (elf_hostcalls()->isatty(fd) != 0) {
        write_str(2, "file seen as tty\n");
        elf_hostcalls()->close(fd);
        return 14;
    }
    write_str(2, "E\n");

    char buf[64];
    int n = elf_hostcalls()->read(fd, buf, sizeof(buf));
    if (n < 0) {
        write_str(2, "read fail\n");
        elf_hostcalls()->close(fd);
        return 15;
    }
    if (n == 0) {
        write_str(2, "empty\n");
        elf_hostcalls()->close(fd);
        return 16;
    }
    elf_hostcalls()->write(1, buf, (unsigned long)n);
    write_str(2, "F\n");

    elf_hostcalls()->close(fd);
    return 0;
}
