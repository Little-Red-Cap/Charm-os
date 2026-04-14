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

static void append_str(char* out, unsigned long cap, unsigned long* len, const char* s) {
    if (!s || !out || !len) return;
    while (*s != '\0' && *len + 1 < cap) {
        out[(*len)++] = *s++;
    }
    out[*len] = '\0';
}

static void append_dec_u32(char* out, unsigned long cap, unsigned long* len, unsigned long value) {
    if (!out || !len) return;
    char tmp[20];
    int t = 0;
    do {
        tmp[t++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value > 0);
    while (t > 0 && *len + 1 < cap) {
        out[(*len)++] = tmp[--t];
    }
    out[(*len)++] = '\n';
    out[*len] = '\0';
}

static void append_dec_i32(char* out, unsigned long cap, unsigned long* len, int value) {
    if (!out || !len) return;
    if (value < 0 && *len + 1 < cap) {
        out[(*len)++] = '-';
        value = -value;
    }
    append_dec_u32(out, cap, len, (unsigned long)value);
}

static void append_kv_i32(char* out, unsigned long cap, unsigned long* len, const char* label, int value) {
    append_str(out, cap, len, label);
    append_dec_i32(out, cap, len, value);
}

static void append_kv_u32(char* out, unsigned long cap, unsigned long* len, const char* label, unsigned long value) {
    append_str(out, cap, len, label);
    append_dec_u32(out, cap, len, value);
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;
    const char* path = "/stat.txt";
    if (argc > 1 && argv && argv[1]) {
        path = argv[1];
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        const char* msg = "open fail\n";
        (void)write_all(1, msg, cstr_len(msg));
        return 11;
    }

    PosixStat file_st;
    int st_rc = fstat(fd, &file_st);
    if (st_rc != 0) {
        const char* msg = "fstat fail\n";
        (void)write_all(1, msg, cstr_len(msg));
        close(fd);
        return 12;
    }

    if ((file_st.st_mode & S_IFMT) != S_IFREG) {
        const char* msg = "bad-fstat mode\n";
        (void)write_all(1, msg, cstr_len(msg));
        close(fd);
        return 16;
    }

    PosixStat stdout_st;
    if (fstat(1, &stdout_st) != 0) {
        const char* msg = "stdout-fstat fail\n";
        (void)write_all(1, msg, cstr_len(msg));
        close(fd);
        return 17;
    }

    if ((stdout_st.st_mode & S_IFMT) != S_IFIFO) {
        const char* msg = "bad-stdout mode\n";
        (void)write_all(1, msg, cstr_len(msg));
        close(fd);
        return 18;
    }

    PosixStat stderr_st;
    if (fstat(2, &stderr_st) != 0) {
        const char* msg = "stderr-fstat fail\n";
        (void)write_all(1, msg, cstr_len(msg));
        close(fd);
        return 19;
    }

    if ((stderr_st.st_mode & S_IFMT) != S_IFCHR) {
        const char* msg = "bad-stderr mode\n";
        (void)write_all(1, msg, cstr_len(msg));
        close(fd);
        return 20;
    }

    PosixStat bad_st;
    int bad_rc = fstat(-1, &bad_st);
    if (bad_rc != -1) {
        const char* msg = "bad-fstat rc\n";
        (void)write_all(1, msg, cstr_len(msg));
        close(fd);
        return 13;
    }

    if (errno != 9) {
        const char* msg = "bad-fstat errno\n";
        (void)write_all(1, msg, cstr_len(msg));
        close(fd);
        return 15;
    }

    if (close(fd) != 0) {
        const char* msg = "close fail\n";
        (void)write_all(1, msg, cstr_len(msg));
        return 14;
    }

    char out[96];
    unsigned long len = 0;
    out[0] = '\0';
    append_kv_i32(out, sizeof(out), &len, "rc=", st_rc);
    append_kv_u32(out, sizeof(out), &len, "file=", (unsigned long)(file_st.st_mode & S_IFMT));
    append_kv_u32(out, sizeof(out), &len, "fsz=", (unsigned long)file_st.st_size);
    append_kv_u32(out, sizeof(out), &len, "out=", (unsigned long)(stdout_st.st_mode & S_IFMT));
    append_kv_u32(out, sizeof(out), &len, "osz=", (unsigned long)stdout_st.st_size);
    append_kv_u32(out, sizeof(out), &len, "err=", (unsigned long)(stderr_st.st_mode & S_IFMT));
    append_kv_u32(out, sizeof(out), &len, "esz=", (unsigned long)stderr_st.st_size);
    append_kv_i32(out, sizeof(out), &len, "bad=", bad_rc);
    (void)write_all(1, out, len);
    return 0;
}
