//
// Minimal smoke tests for posix.proc (no framework).
//

module;
#include <array>
#include <cstdlib>

export module posix.proc.tests;

#if defined(POSIX_PROC_SMOKE_TEST) && POSIX_PROC_SMOKE_TEST

import posix.proc;
import util.core;
import util.error;

namespace {
    [[noreturn]] inline void fail() noexcept { std::abort(); }
    inline void assert_true(bool v) noexcept { if (!v) fail(); }
    template <class A, class B>
    inline void assert_eq(const A& a, const B& b) noexcept { if (!(a == b)) fail(); }

    int demo_main(int argc, char** argv) {
        if (argc < 1 || argv == nullptr) return 7;
        return 42;
    }

    void test_spawn_wait() noexcept {
        posix::ProcService<4, 4> procs{};
        procs.init();
        auto rreg = procs.register_executable("demo", &demo_main);
        assert_true(rreg);

        const char* argv[] = {"demo", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "demo";
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.path_mode = posix::PathMode::exact;

        auto spawn = procs.spawn(cfg);
        assert_true(spawn);
        const auto pid = spawn.value().pid;

        auto st = procs.waitpid(pid, 0);
        assert_true(st);
        assert_eq(st.value().pid.value, pid.value);
        assert_eq(st.value().code, 42);
        assert_eq(st.value().kind, posix::WaitKind::exited);
    }

    void test_search_path_argv0() noexcept {
        posix::ProcService<4, 4> procs{};
        procs.init();
        auto rreg = procs.register_executable("hello", &demo_main);
        assert_true(rreg);

        const char* argv[] = {"hello", nullptr};
        posix::SpawnConfig cfg{};
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.path_mode = posix::PathMode::search_path;

        auto spawn = procs.spawn(cfg);
        assert_true(spawn);
        auto st = procs.waitpid(spawn.value().pid, 0);
        assert_true(st);
    }
} // namespace

export void run_posix_proc_smoke_tests() noexcept {
    test_spawn_wait();
    test_search_path_argv0();
}

#endif
