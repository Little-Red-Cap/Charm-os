#include "elf_hostcall.h"

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

static int bytes_equal(const char* a, const char* b, unsigned long len) {
    for (unsigned long i = 0; i < len; ++i) {
        if (a[i] != b[i]) return 0;
    }
    return 1;
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;

    const char* path = "/append-out.txt";
    const char* payload = "tail\n";
    const char* expected = "base\ntail\n";
    if (argc > 1 && argv && argv[1]) {
        path = argv[1];
    }
    if (argc > 2 && argv && argv[2]) {
        payload = argv[2];
    }
    if (argc > 3 && argv && argv[3]) {
        expected = argv[3];
    }

    const unsigned long payload_len = cstr_len(payload);
    const unsigned long expected_len = cstr_len(expected);

    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0);
    if (fd < 0) {
        write_str(2, "open fail\n");
        return 11;
    }

    if (isatty(fd) != 0) {
        write_str(2, "file seen as tty\n");
        close(fd);
        return 12;
    }

    PosixStat st = {0, 0};
    if (fstat(fd, &st) != 0) {
        write_str(2, "fstat fail\n");
        close(fd);
        return 13;
    }

    if ((st.st_mode & S_IFMT) != S_IFREG) {
        write_str(2, "bad mode\n");
        close(fd);
        return 14;
    }

    if (st.st_size == 0) {
        write_str(2, "seed missing\n");
        close(fd);
        return 15;
    }

    if (write(fd, payload, payload_len) != (int)payload_len) {
        write_str(2, "append fail\n");
        close(fd);
        return 16;
    }

    if (close(fd) != 0) {
        write_str(2, "close fail\n");
        return 17;
    }

    fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        write_str(2, "reopen fail\n");
        return 18;
    }

    if (fstat(fd, &st) != 0) {
        write_str(2, "refstat fail\n");
        close(fd);
        return 19;
    }

    if (st.st_size != expected_len) {
        write_str(2, "size fail\n");
        close(fd);
        return 20;
    }

    char buf[64];
    if (expected_len > sizeof(buf)) {
        write_str(2, "expected too big\n");
        close(fd);
        return 21;
    }

    int n = read(fd, buf, sizeof(buf));
    if (n != (int)expected_len) {
        write_str(2, "readback fail\n");
        close(fd);
        return 22;
    }

    if (!bytes_equal(buf, expected, expected_len)) {
        write_str(2, "content fail\n");
        close(fd);
        return 23;
    }

    if (read(fd, buf, sizeof(buf)) != 0) {
        write_str(2, "eof fail\n");
        close(fd);
        return 24;
    }

    if (close(fd) != 0) {
        write_str(2, "reclose fail\n");
        return 25;
    }

    write_str(1, "append-ok\n");
    return 0;
}
