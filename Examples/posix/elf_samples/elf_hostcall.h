#pragma once

#include <stdint.h>
#include <stddef.h>

typedef int (*elf_write_fn)(int fd, const void* buf, unsigned long len);
typedef void (*elf_exit_fn)(int code);
typedef int (*elf_open_fn)(const char* path, int flags, int mode);
typedef int (*elf_close_fn)(int fd);
typedef int (*elf_read_fn)(int fd, void* buf, unsigned long len);

typedef struct PosixStat {
    unsigned long long st_size;
    unsigned int st_mode;
} PosixStat;

typedef int (*elf_fstat_fn)(int fd, PosixStat* st);
typedef int (*elf_isatty_fn)(int fd);
typedef int* (*elf_errno_location_fn)(void);

struct ElfHostCalls {
    elf_write_fn write;
    elf_exit_fn exit;
    elf_open_fn open;
    elf_close_fn close;
    elf_read_fn read;
    elf_fstat_fn fstat;
    elf_isatty_fn isatty;
    elf_errno_location_fn errno_location;
};

__attribute__((section(".hostcall")))
extern volatile struct ElfHostCalls elf_hostcalls_table;

static inline volatile struct ElfHostCalls* elf_hostcalls(void) {
    return &elf_hostcalls_table;
}

static inline int* elf_errno_location(void) {
    if (!elf_hostcalls()->errno_location) return (int*)0;
    return elf_hostcalls()->errno_location();
}

#define errno (*elf_errno_location())

static inline void _exit(int code) {
    elf_hostcalls()->exit(code);
    for (;;) {}
}

static inline int write(int fd, const void* buf, unsigned long len) {
    return elf_hostcalls()->write(fd, buf, len);
}

static inline int read(int fd, void* buf, unsigned long len) {
    return elf_hostcalls()->read(fd, buf, len);
}

static inline int open(const char* path, int flags, int mode) {
    return elf_hostcalls()->open(path, flags, mode);
}

static inline int close(int fd) {
    return elf_hostcalls()->close(fd);
}

static inline int fstat(int fd, PosixStat* st) {
    return elf_hostcalls()->fstat(fd, st);
}

static inline int isatty(int fd) {
    return elf_hostcalls()->isatty(fd);
}
