//
// Minimal smoke tests for posix.proc (no framework).
//

module;
#include <array>
#include <cstdlib>

export module posix.proc.tests;

#if defined(POSIX_PROC_SMOKE_TEST) && POSIX_PROC_SMOKE_TEST

import posix.proc;
import posix.fd_table;
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

    util::Result<util::usize> dummy_read(void*, posix::MutByteView) noexcept {
        return util::unexpected(util::Errc::not_supported);
    }
    util::Result<util::usize> dummy_write(void*, posix::ByteView) noexcept {
        return util::unexpected(util::Errc::not_supported);
    }
    util::Result<void> dummy_close(void*) noexcept { return {}; }
    util::Result<void> dummy_stat(void*, posix::PosixStat&) noexcept { return {}; }

    void test_spawn_wait() noexcept {
        posix::ProcService<4, 4, 8> procs{};
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
        posix::ProcService<4, 4, 8> procs{};
        procs.init();
        auto rreg = procs.register_executable("/bin/hello", &demo_main);
        assert_true(rreg);

        const char* argv[] = {"hello", nullptr};
        const char* envp[] = {"PATH=/bin:/usr/bin", nullptr};
        posix::SpawnConfig cfg{};
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.envp = std::span<const char* const>(envp, 1);
        cfg.path_mode = posix::PathMode::search_path;

        auto spawn = procs.spawn(cfg);
        assert_true(spawn);
        auto st = procs.waitpid(spawn.value().pid, 0);
        assert_true(st);
    }

    void test_stdio_and_actions() noexcept {
        posix::ProcService<4, 4, 8> procs{};
        posix::FdTable<8> table{};
        table.init();
        procs.init();
        procs.bind_fd_table(table);

        auto rreg = procs.register_executable("demo", &demo_main);
        assert_true(rreg);

        static const posix::FdOps kOps{
            &dummy_read,
            &dummy_write,
            &dummy_close,
            &dummy_stat
        };

        posix::FdEntry entry{};
        entry.kind = posix::FdKind::file;
        entry.flags = posix::FdFlags::read_write;
        entry.ops = &kOps;
        entry.ctx = nullptr;

        auto rfd3 = table.attach(entry, 3);
        assert_true(rfd3);
        auto rfd4 = table.attach(entry, 4);
        assert_true(rfd4);

        posix::FileActions<4> actions{};
        assert_true(actions.add_dup2(3, 1));
        assert_true(actions.add_close(4));

        const char* argv[] = {"demo", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "demo";
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.file_actions = &actions;
        cfg.stdio_in = 3;
        cfg.stdio_out = 4;

        auto spawn = procs.spawn(cfg);
        assert_true(spawn);
        auto st = procs.waitpid(spawn.value().pid, 0);
        assert_true(st);

        posix::FileActions<1> open_actions{};
        assert_true(open_actions.add_open(5, "/tmp/x", 0, 0));
        cfg.file_actions = &open_actions;
        auto spawn_open = procs.spawn(cfg);
        assert_true(!spawn_open);
        assert_eq(spawn_open.error(), util::Errc::not_supported);
    }
} // namespace

export void run_posix_proc_smoke_tests() noexcept {
    test_spawn_wait();
    test_search_path_argv0();
    test_stdio_and_actions();
}

#endif
