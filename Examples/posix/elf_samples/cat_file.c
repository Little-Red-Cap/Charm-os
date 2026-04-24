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
    write(fd, s, cstr_len(s));
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    write_str(2, "A\n");

    int use_stdin = 1;
    if (argc > 1 && argv && argv[1]) {
        if (!(argv[1][0] == '-' && argv[1][1] == '\0')) {
            use_stdin = 0;
        }
    }

    int fd = 0;
    if (use_stdin) {
        write_str(2, "S\n");
    } else {
        const char* path = argv[1];
        fd = open(path, O_RDONLY, 0);
        if (fd < 0) {
            write_str(2, "open fail\n");
            return 11;
        }
        write_str(2, "B\n");
    }

    PosixStat st;
    if (fstat(fd, &st) != 0) {
        write_str(2, "fstat fail\n");
        close(fd);
        return 12;
    }
    write_str(2, "C\n");

    if (isatty(1) != 1) {
        write_str(2, "stdout not tty\n");
        close(fd);
        return 13;
    }
    write_str(2, "D\n");

    if (isatty(fd) != 0) {
        if (use_stdin) {
            write_str(2, "stdin seen as tty\n");
        } else {
            write_str(2, "file seen as tty\n");
        }
        close(fd);
        return 14;
    }
    write_str(2, "E\n");

    char buf[64];
    int chunks = 0;
    for (;;) {
        int n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            write_str(2, "read fail\n");
            close(fd);
            return 15;
        }
        if (n == 0) {
            if (chunks == 0) {
                write_str(2, "empty\n");
                close(fd);
                return 16;
            }
            write_str(2, "G\n");
            break;
        }
        write(1, buf, (unsigned long)n);
        write_str(2, "F\n");
        ++chunks;
    }

    close(fd);
    return 0;
}
