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

static void write_uint(unsigned long v) {
    char buf[16];
    int i = 0;
    if (v == 0) {
        buf[i++] = '0';
    } else {
        while (v > 0 && i < (int)sizeof(buf)) {
            buf[i++] = (char)('0' + (v % 10));
            v /= 10;
        }
    }
    while (i-- > 0) {
        elf_hostcalls()->write(1, &buf[i], 1);
    }
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    const char* path = "/cat.txt";
    if (argc > 1 && argv && argv[1]) {
        path = argv[1];
    }

    int fd = elf_hostcalls()->open(path, O_RDONLY, 0);
    if (fd < 0) {
        write_str(2, "open fail\n");
        return 11;
    }

    PosixStat st;
    if (elf_hostcalls()->fstat(fd, &st) != 0) {
        write_str(2, "fstat fail\n");
        elf_hostcalls()->close(fd);
        return 12;
    }

    if (elf_hostcalls()->isatty(1) != 1) {
        write_str(2, "stdout not tty\n");
        elf_hostcalls()->close(fd);
        return 13;
    }

    if (elf_hostcalls()->isatty(fd) != 0) {
        write_str(2, "file seen as tty\n");
        elf_hostcalls()->close(fd);
        return 14;
    }

    char buf[64];
    for (;;) {
        int n = elf_hostcalls()->read(fd, buf, sizeof(buf));
        if (n < 0) {
            write_str(2, "read fail\n");
            elf_hostcalls()->close(fd);
            return 15;
        }
        if (n == 0) break;
        elf_hostcalls()->write(1, buf, (unsigned long)n);
    }

    elf_hostcalls()->close(fd);
    write_str(1, "size=");
    write_uint((unsigned long)st.st_size);
    write_str(1, "\n");
    return 0;
}
