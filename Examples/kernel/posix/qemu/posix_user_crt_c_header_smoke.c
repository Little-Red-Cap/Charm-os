#include "charm_posix_user_crt.h"

int charm_posix_c_header_probe_entry(void) {
    if (charm_posix_argc() != 2) return 71;

    char** argv = charm_posix_argv();
    char** envp = charm_posix_envp();
    if (argv == 0 || envp == 0) return 72;
    if (argv[0] == 0 || argv[1] == 0) return 73;

    const char* foo = charm_posix_getenv("FOO");
    if (foo == 0) return 74;
    if (foo[0] != 'B' || foo[1] != 'A' || foo[2] != 'R' || foo[3] != '\0') return 75;
    if (charm_posix_environ() != envp) return 83;
    if (charm_posix_getenv("MISSING") != 0) return 84;
    if (charm_posix_getenv(0) != 0) return 85;

    char ch = 0;
    if (charm_posix_read(-1, &ch, 1) != -1) return 76;

    int* err = charm_posix_errno_location();
    if (err == 0 || *err != CHARM_POSIX_EBADF) return 77;
    *err = 41;
    if (charm_posix_getpid() <= 0) return 78;
    if (*err != 41) return 86;
    *err = 42;
    if (charm_posix_sleep(0) != 0) return 87;
    if (*err != 42) return 88;
    *err = 43;
    if (charm_posix_write(1, "c-header-ok\n", 12) != 12) return 79;
    if (*err != 43) return 89;
    return 0;
}

int charm_posix_c_header_exit_entry(void) {
    if (charm_posix_write(1, "c-header-exit\n", 14) != 14) return 81;
    charm_posix_exit(37);
    return 82;
}

int charm_posix_c_header_abort_entry(void) {
    if (charm_posix_write(1, "c-header-abort\n", 15) != 15) return 91;
    charm_posix_abort();
    return 92;
}
