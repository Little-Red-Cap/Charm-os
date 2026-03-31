//
// Unified smoke test entry for POSIX modules (no framework).
//

module;
#include <cstdio>

export module posix.smoke;

export import posix.api.tests;
export import posix.errno.tests;
export import posix.fd_table.tests;
export import posix.pipe.tests;
export import posix.proc.tests;

export void run_posix_smoke_tests() noexcept {
    std::printf("[posix-smoke] begin\n");
    run_posix_errno_smoke_tests();
    run_posix_fd_table_smoke_tests();
    run_posix_pipe_smoke_tests();
    run_posix_proc_smoke_tests();
    run_posix_api_smoke_tests();
    std::printf("[posix-smoke] end ok\n");
}
