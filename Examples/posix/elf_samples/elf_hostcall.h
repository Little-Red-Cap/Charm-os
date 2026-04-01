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

struct ElfHostCalls {
    elf_write_fn write;
    elf_exit_fn exit;
    elf_open_fn open;
    elf_close_fn close;
    elf_read_fn read;
    elf_fstat_fn fstat;
    elf_isatty_fn isatty;
};

__attribute__((section(".hostcall")))
extern volatile struct ElfHostCalls elf_hostcalls_table;

static inline volatile struct ElfHostCalls* elf_hostcalls(void) {
    return &elf_hostcalls_table;
}
