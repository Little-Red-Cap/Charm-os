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

#ifndef O_RDONLY
#define O_RDONLY 0
#endif

#ifndef O_WRONLY
#define O_WRONLY 0x1
#endif

#ifndef O_RDWR
#define O_RDWR 0x2
#endif

#ifndef O_CREAT
#define O_CREAT 0x40
#endif

#ifndef O_TRUNC
#define O_TRUNC 0x200
#endif

#ifndef O_APPEND
#define O_APPEND 0x400
#endif

#ifndef S_IFMT
#define S_IFMT 0170000u
#endif

#ifndef S_IFIFO
#define S_IFIFO 0010000u
#endif

#ifndef S_IFCHR
#define S_IFCHR 0020000u
#endif

#ifndef S_IFDIR
#define S_IFDIR 0040000u
#endif

#ifndef S_IFREG
#define S_IFREG 0100000u
#endif

typedef int (*elf_fstat_fn)(int fd, PosixStat* st);
typedef int (*elf_isatty_fn)(int fd);
typedef int* (*elf_errno_location_fn)(void);
typedef int (*elf_getpid_fn)(void);
typedef int (*elf_sleep_fn)(unsigned int seconds);
typedef int (*elf_kill_fn)(int pid, int sig);

#ifndef SIGINT
#define SIGINT 2
#endif

#ifndef SIGKILL
#define SIGKILL 9
#endif

#ifndef SIGTERM
#define SIGTERM 15
#endif

struct ElfHostCalls {
    elf_write_fn write;
    elf_exit_fn exit;
    elf_open_fn open;
    elf_close_fn close;
    elf_read_fn read;
    elf_fstat_fn fstat;
    elf_isatty_fn isatty;
    elf_errno_location_fn errno_location;
    elf_getpid_fn getpid;
    elf_sleep_fn sleep;
    elf_kill_fn kill;
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

static inline int getpid(void) {
    if (!elf_hostcalls()->getpid) return 0;
    return elf_hostcalls()->getpid();
}

static inline int sleep(unsigned int seconds) {
    if (!elf_hostcalls()->sleep) return -1;
    return elf_hostcalls()->sleep(seconds);
}

static inline int kill(int pid, int sig) {
    if (!elf_hostcalls()->kill) return -1;
    return elf_hostcalls()->kill(pid, sig);
}
