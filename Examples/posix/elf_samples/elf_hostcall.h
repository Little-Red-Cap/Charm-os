#pragma once

#include <stdint.h>
#include <stddef.h>

typedef int (*elf_write_fn)(int fd, const void* buf, unsigned long len);
typedef void (*elf_exit_fn)(int code);

struct ElfHostCalls {
    elf_write_fn write;
    elf_exit_fn exit;
};

__attribute__((section(".hostcall")))
extern volatile struct ElfHostCalls elf_hostcalls_table;

static inline volatile struct ElfHostCalls* elf_hostcalls(void) {
    return &elf_hostcalls_table;
}
