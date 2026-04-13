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

    char ch = 0;
    if (charm_posix_read(-1, &ch, 1) != -1) return 76;

    int* err = charm_posix_errno_location();
    if (err == 0 || *err != CHARM_POSIX_EBADF) return 77;
    if (charm_posix_getpid() <= 0) return 78;

    return charm_posix_write(1, "c-header-ok\n", 12) == 12 ? 0 : 79;
}

int charm_posix_c_header_exit_entry(void) {
    if (charm_posix_write(1, "c-header-exit\n", 14) != 14) return 81;
    charm_posix_exit(37);
}
