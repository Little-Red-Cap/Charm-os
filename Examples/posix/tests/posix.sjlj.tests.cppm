module;

#include <csetjmp>
#include <cstdio>
#include <cstdlib>

export module posix.sjlj.tests;

#if defined(POSIX_SJLJ_SMOKE_TEST) && POSIX_SJLJ_SMOKE_TEST

namespace {

    inline void log_line(const char* msg) noexcept {
        std::printf("%s\n", msg);
    }

    [[noreturn]] void trigger_jump(std::jmp_buf& env) noexcept {
        log_line("[posix-smoke] sjlj C");
        std::longjmp(env, 7);
    }
}

export void run_posix_sjlj_smoke_tests() noexcept {
    std::jmp_buf env{};
    log_line("[posix-smoke] sjlj A");
    const int rc = setjmp(env);
    if (rc == 0) {
        log_line("[posix-smoke] sjlj B");
        trigger_jump(env);
    }
    if (rc != 7) {
        log_line("[posix-smoke] sjlj fail");
        std::abort();
    }
    log_line("[posix-smoke] sjlj ok");
}

#else

export void run_posix_sjlj_smoke_tests() noexcept {}

#endif
