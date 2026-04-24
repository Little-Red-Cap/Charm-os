#include "elf_hostcall.h"

volatile struct ElfHostCalls elf_hostcalls_table;

static int str_eq(const char* lhs, const char* rhs) {
    if (!lhs || !rhs) {
        return 0;
    }
    while (*lhs && *rhs) {
        if (*lhs != *rhs) {
            return 0;
        }
        ++lhs;
        ++rhs;
    }
    return *lhs == 0 && *rhs == 0;
}

static int parse_signal(const char* text) {
    if (!text || *text == 0) {
        return SIGTERM;
    }
    if (str_eq(text, "2") || str_eq(text, "INT") || str_eq(text, "SIGINT")) {
        return SIGINT;
    }
    if (str_eq(text, "9") || str_eq(text, "KILL") || str_eq(text, "SIGKILL")) {
        return SIGKILL;
    }
    if (str_eq(text, "15") || str_eq(text, "TERM") || str_eq(text, "SIGTERM")) {
        return SIGTERM;
    }
    return -1;
}

int entry(int argc, char** argv, char** envp) {
    (void)envp;

    int sig = SIGTERM;
    if (argc > 1) {
        sig = parse_signal(argv[1]);
        if (sig < 0) {
            write(1, "bad-signal\n", 11);
            return 3;
        }
    }

    write(1, "before-kill\n", 12);
    if (kill(getpid(), sig) != 0) {
        write(1, "kill-failed\n", 12);
        return 4;
    }
    write(1, "after-kill\n", 11);
    return 5;
}
