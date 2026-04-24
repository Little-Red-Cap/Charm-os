//
// Aggregated POSIX program smoke entry.
//

module;

export module posix.programs.tests;

#if defined(POSIX_PROGRAMS_SMOKE_TEST) && POSIX_PROGRAMS_SMOKE_TEST

import posix.test_harness;
import posix.programs.exec.tests;
import posix.programs.fs.tests;
import posix.programs.fdpath.tests;
import posix.programs.shell.tests;

namespace {
    using namespace posix::testsupport;
}

export void run_posix_programs_smoke_tests() noexcept {
    log_line("[posix-smoke] programs begin");
    run_posix_program_exec_smoke_tests();
    run_posix_program_fdpath_smoke_tests();
    run_posix_program_fs_smoke_tests();
    run_posix_program_shell_smoke_tests();
#if defined(CHARM_POSIX_NEWLIB_STDIO_SMOKE) && CHARM_POSIX_NEWLIB_STDIO_SMOKE
    run_posix_program_stdio_smoke_tests();
#endif
    log_line("[posix-smoke] programs end ok");
}

#endif
