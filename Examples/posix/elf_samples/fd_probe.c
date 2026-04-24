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

static int write_str(int fd, const char* s) {
    return write_all(fd, s, cstr_len(s));
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

    PosixStat st = {0, 0};
    if (fstat(0, &st) != 0) {
        write_str(1, "stdin-fstat\n");
        close(fd);
        return 34;
    }

    if ((st.st_mode & S_IFMT) != S_IFCHR) {
        write_str(1, "stdin-mode\n");
        close(fd);
        return 35;
    }

    if (isatty(0) != 1) {
        write_str(1, "stdin-tty\n");
        close(fd);
        return 36;
    }

    if (fstat(1, &st) != 0) {
        write_str(1, "stdout-fstat\n");
        close(fd);
        return 37;
    }

    if ((st.st_mode & S_IFMT) != S_IFIFO) {
        write_str(1, "stdout-mode\n");
        close(fd);
        return 38;
    }

    if (isatty(1) != 0) {
        write_str(1, "stdout-tty\n");
        close(fd);
        return 39;
    }

    if (fstat(fd, &st) != 0) {
        write_str(1, "file-fstat\n");
        close(fd);
        return 40;
    }

    if ((st.st_mode & S_IFMT) != S_IFREG) {
        write_str(1, "file-mode\n");
        close(fd);
        return 41;
    }

    if (isatty(fd) != 0) {
        write_str(1, "file-tty\n");
        close(fd);
        return 42;
    }

    if (read(-1, &ch, 1) != -1) {
        write_all(1, "read-bad\n", 9);
        close(fd);
        return 43;
    }

    if (errno != 9) {
        write_all(1, "read-errno\n", 11);
        close(fd);
        return 44;
    }

    if (write(-1, &ch, 1) != -1) {
        write_all(1, "write-bad\n", 10);
        close(fd);
        return 45;
    }

    if (errno != 9) {
        write_all(1, "write-errno\n", 12);
        close(fd);
        return 46;
    }

    if (close(-1) != -1) {
        write_all(1, "close-bad\n", 10);
        close(fd);
        return 47;
    }

    if (errno != 9) {
        write_all(1, "close-errno\n", 12);
        close(fd);
        return 48;
    }

    write_str(1, "stdin=chr tty=1\nstdout=fifo tty=0\nfile=reg tty=0\nrw-ok\n");
    close(fd);
    return 0;
}
