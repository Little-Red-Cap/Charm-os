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

static int write_all(int fd, const char* buf, unsigned long len) {
    unsigned long off = 0;
    while (off < len) {
        int w = write(fd, buf + off, len - off);
        if (w <= 0) return -1;
        off += (unsigned long)w;
    }
    return 0;
}

static void emit(const char* s) {
    (void)write_all(1, s, cstr_len(s));
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    const char* path = "/stat.txt";
    if (argc > 1 && argv && argv[1]) {
        path = argv[1];
    }

    emit("A\n");
    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        emit("open fail\n");
        return 11;
    }

    emit("B\n");
    PosixStat st;
    int st_rc = fstat(fd, &st);
    if (st_rc != 0) {
        emit("fstat fail\n");
        close(fd);
        return 12;
    }

    emit("C\n");
    int bad_rc = fstat(-1, &st);
    if (bad_rc != -1) {
        emit("bad-fstat rc\n");
        close(fd);
        return 13;
    }

    emit("D\n");
    if (close(fd) != 0) {
        emit("close fail\n");
        return 14;
    }

    emit("E\n");
    return 0;
}
