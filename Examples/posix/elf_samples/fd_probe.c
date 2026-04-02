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

static void append_str(char* out, unsigned long cap, unsigned long* len, const char* s) {
    if (!s || !out || !len) return;
    while (*s != '\0' && *len + 1 < cap) {
        out[(*len)++] = *s++;
    }
    out[*len] = '\0';
}

static void append_dec(char* out, unsigned long cap, unsigned long* len, int value) {
    if (!out || !len) return;
    char buf[12];
    int idx = 0;
    if (value < 0) {
        buf[idx++] = '-';
        value = -value;
    }
    char tmp[10];
    int t = 0;
    do {
        tmp[t++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value > 0);
    while (t > 0) {
        buf[idx++] = tmp[--t];
    }
    buf[idx++] = '\n';
    for (int i = 0; i < idx && *len + 1 < cap; ++i) {
        out[(*len)++] = buf[i];
    }
    out[*len] = '\0';
}

static void append_kv(char* out, unsigned long cap, unsigned long* len, const char* label, int value) {
    append_str(out, cap, len, label);
    append_dec(out, cap, len, value);
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

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    const char* path = "/cat.txt";
    if (argc > 1 && argv && argv[1]) {
        path = argv[1];
    }

    char out[128];
    unsigned long len = 0;
    out[0] = '\0';

    append_kv(out, sizeof(out), &len, "t0=", isatty(0));
    append_kv(out, sizeof(out), &len, "t1=", isatty(1));
    append_kv(out, sizeof(out), &len, "t2=", isatty(2));
    append_kv(out, sizeof(out), &len, "bt=", isatty(-1));

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        const char* msg = "open fail\n";
        (void)write_all(1, msg, cstr_len(msg));
        return 11;
    }

    PosixStat st;
    int st_rc = fstat(fd, &st);
    append_kv(out, sizeof(out), &len, "fs=", st_rc);
    append_kv(out, sizeof(out), &len, "ft=", isatty(fd));
    append_kv(out, sizeof(out), &len, "bs=", fstat(-1, &st));

    (void)write_all(1, out, len);

    close(fd);
    return 0;
}
