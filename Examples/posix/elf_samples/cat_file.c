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

static void mark(const char* s) {
    write_str(2, s);
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    mark("A\n");
    const char* path = "/cat.txt";
    if (argc > 1 && argv && argv[1]) {
        path = argv[1];
    }

    int fd = elf_hostcalls()->open(path, O_RDONLY, 0);
    if (fd < 0) {
        mark("open fail\n");
        return 11;
    }
    mark("B\n");

    PosixStat st;
    if (elf_hostcalls()->fstat(fd, &st) != 0) {
        mark("fstat fail\n");
        elf_hostcalls()->close(fd);
        return 12;
    }
    mark("C\n");

    if (elf_hostcalls()->isatty(1) != 1) {
        mark("stdout not tty\n");
        elf_hostcalls()->close(fd);
        return 13;
    }
    mark("D\n");

    if (elf_hostcalls()->isatty(fd) != 0) {
        mark("file seen as tty\n");
        elf_hostcalls()->close(fd);
        return 14;
    }
    mark("E\n");

    char buf[64];
    for (;;) {
        int n = elf_hostcalls()->read(fd, buf, sizeof(buf));
        if (n < 0) {
            mark("read fail\n");
            elf_hostcalls()->close(fd);
            return 15;
        }
        if (n == 0) break;
        elf_hostcalls()->write(1, buf, (unsigned long)n);
    }
    mark("F\n");

    elf_hostcalls()->close(fd);
    return 0;
}
