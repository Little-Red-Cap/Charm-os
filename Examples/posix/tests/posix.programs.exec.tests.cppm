//
// Program-driven smoke tests for minimal userland targets.
//

module;
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <span>
#include <string_view>
#include <type_traits>
#include <cstring>
#include <new>

#include "../elf_samples/hello.elf.inc"
#include "../elf_samples/argv_dump.elf.inc"
#include "../elf_samples/env_dump.elf.inc"
#include "../elf_samples/stderr_demo.elf.inc"
#include "../elf_samples/exit_code.elf.inc"
#include "../elf_samples/getpid.elf.inc"
#include "../elf_samples/sleep.elf.inc"
#include "../elf_samples/kill_self.elf.inc"
#include "../elf_samples/cat_file.elf.inc"

export module posix.programs.exec.tests;

#if defined(POSIX_PROGRAMS_SMOKE_TEST) && POSIX_PROGRAMS_SMOKE_TEST

import posix.test_harness;

namespace {
    using namespace posix::testsupport;

    struct TermStubCtx {
        util::u32 refs{1};
        bool non_block{false};
        char* capture{nullptr};
        util::usize capture_cap{0};
        util::usize capture_size{0};
    };

    util::Result<void> term_stat_stub(void*, posix::PosixStat& out) noexcept {
        out.mode = posix::make_stat_mode(posix::S_IFCHR, posix::kModePermChar);
        out.size = 0;
        return {};
    }

    util::Result<util::usize> term_read_stub(void*, posix::MutByteView) noexcept {
        return util::usize{0};
    }

    util::Result<util::usize> term_write_stub(void* ctx, posix::ByteView buf) noexcept {
        auto* state = static_cast<TermStubCtx*>(ctx);
        if (state && state->capture != nullptr && state->capture_size < state->capture_cap) {
            const auto remaining = state->capture_cap - state->capture_size;
            const auto to_copy = buf.size() < remaining ? buf.size() : remaining;
            if (to_copy > 0) {
                std::memcpy(state->capture + state->capture_size, buf.data(), to_copy);
                state->capture_size += to_copy;
            }
        }
        return buf.size();
    }

    util::Result<void> term_close_stub(void* ctx) noexcept {
        auto* state = static_cast<TermStubCtx*>(ctx);
        if (state && state->refs > 0) {
            --state->refs;
        }
        return {};
    }

    util::Result<void> term_dup_stub(void* ctx) noexcept {
        auto* state = static_cast<TermStubCtx*>(ctx);
        if (state) {
            ++state->refs;
        }
        return {};
    }

    util::Result<int> term_get_status_flags_stub(void* ctx) noexcept {
        auto* state = static_cast<TermStubCtx*>(ctx);
        if (!state) {
            return 0;
        }
        return state->non_block ? posix::O_NONBLOCK : 0;
    }

    util::Result<void> term_set_status_flags_stub(void* ctx, int flags) noexcept {
        auto* state = static_cast<TermStubCtx*>(ctx);
        if (state) {
            state->non_block = (flags & posix::O_NONBLOCK) != 0;
        }
        return {};
    }

    inline const posix::FdOps kTermOps{
        &term_read_stub,
        &term_write_stub,
        &term_close_stub,
        &term_stat_stub,
        &term_dup_stub,
        nullptr,
        &term_get_status_flags_stub,
        &term_set_status_flags_stub
    };

    void test_hello() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("hello", &hello_main);
        check_true("hello-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("hello-pipe", h.api.pipe(pipefd), 0);
        check_true("hello-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"hello", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "hello";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("hello-spawn", sp);

        std::array<char, 32> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("hello-out", out, std::string_view{"hello\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("hello-wait", st);
        check_eq("hello-exit", st.value().code, 0);
    }

    void test_argv_dump() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("argv_dump", &argv_dump_main);
        check_true("argv-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("argv-pipe", h.api.pipe(pipefd), 0);
        check_true("argv-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"argv_dump", "a", "b", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "argv_dump";
        cfg.argv = std::span<const char* const>(argv, 3);

        auto sp = h.procs.spawn(cfg);
        check_true("argv-spawn", sp);

        std::array<char, 96> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        const char expected[] = "argv[0]=argv_dump\nargv[1]=a\nargv[2]=b\n";
        check_eq("argv-out", out, std::string_view{expected});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("argv-wait", st);
    }

    void test_crt_probe() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("crt_probe", &crt_probe_main);
        check_true("crt-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("crt-pipe", h.api.pipe(pipefd), 0);
        check_true("crt-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"crt_probe", "alpha", nullptr};
        const char* envp[] = {"FOO=BAR", "BAR=BAZ", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "crt_probe";
        cfg.argv = std::span<const char* const>(argv, 2);
        cfg.envp = std::span<const char* const>(envp, 2);

        auto sp = h.procs.spawn(cfg);
        check_true("crt-spawn", sp);

        std::array<char, 32> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("crt-out", out, std::string_view{"crt-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("crt-wait", st);
        check_eq("crt-exit", st.value().code, 0);
    }

    void test_crt_exit() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("crt_exit", &crt_exit_main);
        check_true("crt-exit-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("crt-exit-pipe", h.api.pipe(pipefd), 0);
        check_true("crt-exit-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"crt_exit", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "crt_exit";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("crt-exit-spawn", sp);

        std::array<char, 32> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("crt-exit-out", out, std::string_view{"before-exit\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("crt-exit-wait", st);
        check_eq("crt-exit-code", st.value().code, 23);
    }

    void test_crt_errno() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("crt_errno", &crt_errno_main);
        check_true("crt-errno-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("crt-errno-pipe", h.api.pipe(pipefd), 0);
        check_true("crt-errno-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"crt_errno", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "crt_errno";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("crt-errno-spawn", sp);

        std::array<char, 32> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("crt-errno-out", out, std::string_view{"errno-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("crt-errno-wait", st);
        check_eq("crt-errno-code", st.value().code, 0);
    }

    void test_crt_c_probe() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("crt_c_probe", &crt_c_probe_main);
        check_true("crt-c-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("crt-c-pipe", h.api.pipe(pipefd), 0);
        check_true("crt-c-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"crt_c_probe", "beta", nullptr};
        const char* envp[] = {"FOO=BAR", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "crt_c_probe";
        cfg.argv = std::span<const char* const>(argv, 2);
        cfg.envp = std::span<const char* const>(envp, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("crt-c-spawn", sp);

        std::array<char, 32> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("crt-c-out", out, std::string_view{"crt-c-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("crt-c-wait", st);
        check_eq("crt-c-code", st.value().code, 0);
    }

    void test_crt_c_exit() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("crt_c_exit", &crt_c_exit_main);
        check_true("crt-c-exit-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("crt-c-exit-pipe", h.api.pipe(pipefd), 0);
        check_true("crt-c-exit-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"crt_c_exit", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "crt_c_exit";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("crt-c-exit-spawn", sp);

        std::array<char, 32> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("crt-c-exit-out", out, std::string_view{"crt-c-exit\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("crt-c-exit-wait", st);
        check_eq("crt-c-exit-code", st.value().code, 29);
    }

    void test_crt_c_header_probe() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("crt_c_header_probe", &crt_c_header_probe_main);
        check_true("crt-c-header-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("crt-c-header-pipe", h.api.pipe(pipefd), 0);
        check_true("crt-c-header-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"crt_c_header_probe", "beta", nullptr};
        const char* envp[] = {"FOO=BAR", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "crt_c_header_probe";
        cfg.argv = std::span<const char* const>(argv, 2);
        cfg.envp = std::span<const char* const>(envp, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("crt-c-header-spawn", sp);

        std::array<char, 32> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("crt-c-header-out", out, std::string_view{"c-header-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("crt-c-header-wait", st);
        check_eq("crt-c-header-code", st.value().code, 0);
    }

    void test_crt_c_header_exit() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("crt_c_header_exit", &crt_c_header_exit_main);
        check_true("crt-c-header-exit-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("crt-c-header-exit-pipe", h.api.pipe(pipefd), 0);
        check_true("crt-c-header-exit-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"crt_c_header_exit", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "crt_c_header_exit";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("crt-c-header-exit-spawn", sp);

        std::array<char, 32> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("crt-c-header-exit-out", out, std::string_view{"c-header-exit\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("crt-c-header-exit-wait", st);
        check_eq("crt-c-header-exit-code", st.value().code, 37);
    }

    void test_crt_c_fs_header() noexcept {
        fs::clear_mounts();
        static RamFsMount<64, 16, 128> ramfs{};
        new (&ramfs) RamFsMount<64, 16, 128>();
        auto mount_st = fs::add_mount("", ramfs.mount_point());
        check_true("crt-c-fs-header-mount", mount_st);

        Harness h{};
        auto rreg = h.procs.register_executable("crt_c_fs_header", &crt_c_fs_header_main);
        check_true("crt-c-fs-header-register", rreg);

        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        check_true("crt-c-fs-header-stdin", h.fds.attach(term_entry, 0));
        check_true("crt-c-fs-header-stdout-reserve", h.fds.attach(term_entry, 1));
        check_true("crt-c-fs-header-stderr", h.fds.attach(term_entry, 2));

        int pipefd[2]{-1, -1};
        check_eq("crt-c-fs-header-pipe", h.api.pipe(pipefd), 0);
        check_true("crt-c-fs-header-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"crt_c_fs_header", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "crt_c_fs_header";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("crt-c-fs-header-spawn", sp);

        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("crt-c-fs-header-wait", st);
        check_eq("crt-c-fs-header-code", st.value().code, 0);

        std::array<char, 32> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("crt-c-fs-header-out", out, std::string_view{"c-fs-header-ok\n"});
    }

    void test_newlib_syscall_probe() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("newlib_syscall_probe", &newlib_syscall_probe_main);
        check_true("newlib-syscall-register", rreg);

        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        check_true("newlib-syscall-stdin", h.fds.attach(term_entry, 0));
        check_true("newlib-syscall-stdout-reserve", h.fds.attach(term_entry, 1));
        check_true("newlib-syscall-stderr", h.fds.attach(term_entry, 2));

        int pipefd[2]{-1, -1};
        check_eq("newlib-syscall-pipe", h.api.pipe(pipefd), 0);
        check_true("newlib-syscall-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"newlib_syscall_probe", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "newlib_syscall_probe";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("newlib-syscall-spawn", sp);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("newlib-syscall-out", out, std::string_view{"newlib-syscall-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("newlib-syscall-wait", st);
        check_eq("newlib-syscall-code", st.value().code, 0);
    }

    void test_newlib_dup() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("newlib_dup", &newlib_dup_main);
        check_true("newlib-dup-register", rreg);

        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        check_true("newlib-dup-stdin", h.fds.attach(term_entry, 0));
        check_true("newlib-dup-stdout-reserve", h.fds.attach(term_entry, 1));
        check_true("newlib-dup-stderr", h.fds.attach(term_entry, 2));

        int pipefd[2]{-1, -1};
        check_eq("newlib-dup-pipe", h.api.pipe(pipefd), 0);
        check_true("newlib-dup-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"newlib_dup", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "newlib_dup";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("newlib-dup-spawn", sp);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("newlib-dup-out", out, std::string_view{"dup-newlib-dup-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("newlib-dup-wait", st);
        check_eq("newlib-dup-code", st.value().code, 0);
    }

    void test_newlib_dup2() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("newlib_dup2", &newlib_dup2_main);
        check_true("newlib-dup2-register", rreg);

        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        check_true("newlib-dup2-stdin", h.fds.attach(term_entry, 0));
        check_true("newlib-dup2-stdout-reserve", h.fds.attach(term_entry, 1));
        check_true("newlib-dup2-stderr", h.fds.attach(term_entry, 2));

        int pipefd[2]{-1, -1};
        check_eq("newlib-dup2-pipe", h.api.pipe(pipefd), 0);
        check_true("newlib-dup2-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"newlib_dup2", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "newlib_dup2";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("newlib-dup2-spawn", sp);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("newlib-dup2-out", out, std::string_view{"dup2-newlib-dup2-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("newlib-dup2-wait", st);
        check_eq("newlib-dup2-code", st.value().code, 0);
    }

    void test_newlib_fcntl() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("newlib_fcntl", &newlib_fcntl_main);
        check_true("newlib-fcntl-register", rreg);

        TermStubCtx stdin_ctx{};
        TermStubCtx stdout_ctx{};
        TermStubCtx stderr_ctx{};

        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        term_entry.flags = posix::FdFlags::read_only;
        term_entry.ctx = &stdin_ctx;
        check_true("newlib-fcntl-stdin", h.fds.attach(term_entry, 0));
        term_entry.flags = posix::FdFlags::write_only;
        term_entry.ctx = &stdout_ctx;
        check_true("newlib-fcntl-stdout-reserve", h.fds.attach(term_entry, 1));
        term_entry.ctx = &stderr_ctx;
        check_true("newlib-fcntl-stderr", h.fds.attach(term_entry, 2));

        int pipefd[2]{-1, -1};
        check_eq("newlib-fcntl-pipe", h.api.pipe(pipefd), 0);
        check_true("newlib-fcntl-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"newlib_fcntl", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "newlib_fcntl";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("newlib-fcntl-spawn", sp);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("newlib-fcntl-out", out, std::string_view{"fcntl-newlib-fcntl-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("newlib-fcntl-wait", st);
        check_eq("newlib-fcntl-code", st.value().code, 0);
    }

    void test_newlib_pipe() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("newlib_pipe", &newlib_pipe_main);
        check_true("newlib-pipe-register", rreg);

        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        check_true("newlib-pipe-stdin", h.fds.attach(term_entry, 0));
        check_true("newlib-pipe-stdout-reserve", h.fds.attach(term_entry, 1));
        check_true("newlib-pipe-stderr", h.fds.attach(term_entry, 2));

        int pipefd[2]{-1, -1};
        check_eq("newlib-pipe-pipe", h.api.pipe(pipefd), 0);
        check_true("newlib-pipe-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"newlib_pipe", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "newlib_pipe";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("newlib-pipe-spawn", sp);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("newlib-pipe-out", out, std::string_view{"newlib-pipe-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("newlib-pipe-wait", st);
        check_eq("newlib-pipe-code", st.value().code, 0);
    }

    void test_spawn_cloexec() noexcept {
        fs::clear_mounts();
        static RamFsMount<64, 16, 64> ramfs{};
        new (&ramfs) RamFsMount<64, 16, 64>();
        auto mount_st = fs::add_mount("", ramfs.mount_point());
        check_true("cloexec-mount", mount_st);

        Harness h{};
        auto rreg = h.procs.register_executable("cloexec", &cloexec_main);
        check_true("cloexec-register", rreg);

        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        check_true("cloexec-stdin", h.fds.attach(term_entry, 0));
        check_true("cloexec-stdout-reserve", h.fds.attach(term_entry, 1));
        check_true("cloexec-stderr", h.fds.attach(term_entry, 2));

        int pipefd[2]{-1, -1};
        check_eq("cloexec-pipe", h.api.pipe(pipefd), 0);
        check_true("cloexec-dup2", h.fds.dup2(pipefd[1], 1));

        int cloexec_fd = h.api.open("/cloexec.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("cloexec-open", cloexec_fd >= 0);
        check_eq("cloexec-set", h.api.fcntl(cloexec_fd, posix::F_SETFD, posix::FD_CLOEXEC), 0);
        check_eq("cloexec-get", h.api.fcntl(cloexec_fd, posix::F_GETFD), posix::FD_CLOEXEC);

        char fd_arg[16]{};
        std::snprintf(fd_arg, sizeof(fd_arg), "%d", cloexec_fd);
        const char* argv[] = {"cloexec", fd_arg, nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "cloexec";
        cfg.argv = std::span<const char* const>(argv, 2);

        auto sp = h.procs.spawn(cfg);
        check_true("cloexec-spawn", sp);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("cloexec-out", out, std::string_view{"cloexec-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("cloexec-wait", st);
        check_eq("cloexec-code", st.value().code, 0);

        check_eq("cloexec-parent-write", h.api.write(cloexec_fd, "p", 1), static_cast<posix::ssize_t>(1));
        check_eq("cloexec-parent-close", h.api.close(cloexec_fd), 0);

        int verify_fd = h.api.open("/cloexec.txt", posix::O_RDONLY, 0);
        check_true("cloexec-verify-open", verify_fd >= 0);
        std::array<char, 8> verify_buf{};
        auto r = h.api.read(verify_fd, verify_buf.data(), verify_buf.size());
        check_eq("cloexec-verify-read", r, static_cast<posix::ssize_t>(1));
        check_eq("cloexec-verify-text", std::string_view{verify_buf.data(), 1}, std::string_view{"p"});
        check_eq("cloexec-verify-close", h.api.close(verify_fd), 0);
    }

    void test_newlib_kill_self() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("newlib_kill_self", &newlib_kill_self_main);
        check_true("newlib-kill-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("newlib-kill-pipe", h.api.pipe(pipefd), 0);
        check_true("newlib-kill-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"newlib_kill_self", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "newlib_kill_self";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("newlib-kill-spawn", sp);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("newlib-kill-out", out, std::string_view{"newlib-kill\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("newlib-kill-wait", st);
        check_eq("newlib-kill-kind", st.value().kind, posix::WaitKind::signaled);
        check_eq("newlib-kill-sig", st.value().code, posix::SIGTERM);
    }

    void test_newlib_lseek() noexcept {
        fs::clear_mounts();
        static RamFsMount<64, 16, 64> ramfs{};
        new (&ramfs) RamFsMount<64, 16, 64>();
        auto mount_st = fs::add_mount("", ramfs.mount_point());
        check_true("newlib-lseek-mount", mount_st);

        Harness h{};
        auto rreg = h.procs.register_executable("newlib_lseek", &newlib_lseek_main);
        check_true("newlib-lseek-register", rreg);

        int in_fd = h.api.open("/newlib-seek.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("newlib-lseek-open-w", in_fd >= 0);
        check_eq("newlib-lseek-write", h.api.write(in_fd, "alpha", 5), static_cast<posix::ssize_t>(5));
        check_eq("newlib-lseek-close-w", h.api.close(in_fd), 0);

        in_fd = h.api.open("/newlib-seek.txt", posix::O_RDONLY, 0);
        check_true("newlib-lseek-open-r", in_fd >= 0);

        int pipefd[2]{-1, -1};
        check_eq("newlib-lseek-pipe", h.api.pipe(pipefd), 0);

        const char* argv[] = {"newlib_lseek", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "newlib_lseek";
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.stdio_in = in_fd;
        cfg.stdio_out = pipefd[1];
        cfg.stdio_err = 2;

        auto sp = h.procs.spawn(cfg);
        check_true("newlib-lseek-spawn", sp);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("newlib-lseek-out", out, std::string_view{"newlib-lseek-ok\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("newlib-lseek-wait", st);
        check_eq("newlib-lseek-code", st.value().code, 0);
    }

    void test_newlib_path() noexcept {
        fs::clear_mounts();
        static RamFsMount<64, 16, 128> ramfs{};
        new (&ramfs) RamFsMount<64, 16, 128>();
        auto mount_st = fs::add_mount("", ramfs.mount_point());
        check_true("newlib-path-mount", mount_st);

        Harness h{};
        auto rreg = h.procs.register_executable("newlib_path", &newlib_path_main);
        check_true("newlib-path-register", rreg);

        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        check_true("newlib-path-stdin", h.fds.attach(term_entry, 0));
        check_true("newlib-path-stdout-reserve", h.fds.attach(term_entry, 1));
        check_true("newlib-path-stderr", h.fds.attach(term_entry, 2));

        int pipefd[2]{-1, -1};
        check_eq("newlib-path-pipe", h.api.pipe(pipefd), 0);
        check_true("newlib-path-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"newlib_path", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "newlib_path";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("newlib-path-spawn", sp);

        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("newlib-path-wait", st);
        check_eq("newlib-path-code", st.value().code, 0);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("newlib-path-out", out, std::string_view{"newlib-path-ok\n"});
    }

    void test_newlib_cwd() noexcept {
        fs::clear_mounts();
        static RamFsMount<64, 16, 128> ramfs{};
        new (&ramfs) RamFsMount<64, 16, 128>();
        auto mount_st = fs::add_mount("", ramfs.mount_point());
        check_true("newlib-cwd-mount", mount_st);

        Harness h{};
        auto rreg = h.procs.register_executable("newlib_cwd", &newlib_cwd_main);
        check_true("newlib-cwd-register", rreg);

        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        check_true("newlib-cwd-stdin", h.fds.attach(term_entry, 0));
        check_true("newlib-cwd-stdout-reserve", h.fds.attach(term_entry, 1));
        check_true("newlib-cwd-stderr", h.fds.attach(term_entry, 2));

        int pipefd[2]{-1, -1};
        check_eq("newlib-cwd-pipe", h.api.pipe(pipefd), 0);
        check_true("newlib-cwd-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"newlib_cwd", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "newlib_cwd";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("newlib-cwd-spawn", sp);

        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("newlib-cwd-wait", st);
        check_eq("newlib-cwd-code", st.value().code, 0);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("newlib-cwd-out", out, std::string_view{"newlib-cwd-ok\n"});
    }

#if defined(CHARM_POSIX_NEWLIB_STDIO_SMOKE) && CHARM_POSIX_NEWLIB_STDIO_SMOKE
    void test_newlib_stdio() noexcept {
        fs::clear_mounts();
        static RamFsMount<64, 16, 128> ramfs{};
        new (&ramfs) RamFsMount<64, 16, 128>();
        auto mount_st = fs::add_mount("", ramfs.mount_point());
        check_true("newlib-stdio-mount", mount_st);

        Harness h{};
        auto rreg = h.procs.register_executable("newlib_stdio", &newlib_stdio_main);
        check_true("newlib-stdio-register", rreg);

        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        check_true("newlib-stdio-stdin", h.fds.attach(term_entry, 0));
        check_true("newlib-stdio-stdout-reserve", h.fds.attach(term_entry, 1));
        check_true("newlib-stdio-stderr", h.fds.attach(term_entry, 2));

        int pipefd[2]{-1, -1};
        check_eq("newlib-stdio-pipe", h.api.pipe(pipefd), 0);
        check_true("newlib-stdio-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"newlib_stdio", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "newlib_stdio";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("newlib-stdio-spawn", sp);
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("newlib-stdio-wait", st);
        check_eq("newlib-stdio-code", st.value().code, 0);

        std::array<char, 48> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("newlib-stdio-out", out, std::string_view{"newlib-stdio-ok\n"});
    }
#endif

    void test_exit_code_explicit_exit() noexcept {
        Harness h{};
        h.procs.enable_elf_exec(true);
        h.procs.enable_elf_hostcalls(true);
        check_true("exit-abi-reg", h.procs.register_elf_mem("exit_code", exit_code_elf, exit_code_elf_len));

        int pipefd[2]{-1, -1};
        check_eq("exit-abi-pipe", h.api.pipe(pipefd), 0);

        const char* argv[] = {"elfmem:exit_code", "23", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "elfmem:exit_code";
        cfg.argv = std::span<const char* const>(argv, 2);
        cfg.stdio_out = pipefd[1];

        auto sp = h.procs.spawn(cfg);
        check_true("exit-abi-spawn", sp);
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("exit-abi-wait", st);
        check_eq("exit-abi-code", st.value().code, 23);
        (void)h.api.close(pipefd[1]);

        std::array<char, 32> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("exit-abi-out", out, std::string_view{});

        h.procs.enable_elf_hostcalls(false);
        h.procs.enable_elf_exec(false);
    }

    void test_stderr_demo() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("stderr_demo", &stderr_demo_main);
        check_true("stderr-register", rreg);

        int out_pipe[2]{-1, -1};
        int err_pipe[2]{-1, -1};
        check_eq("stderr-out-pipe", h.api.pipe(out_pipe), 0);
        check_eq("stderr-err-pipe", h.api.pipe(err_pipe), 0);
        int err_read = err_pipe[0];
        if (err_read == 2) {
            check_true("stderr-move-read", h.fds.dup2(err_read, 5));
            err_read = 5;
        }
        check_true("stderr-dup2-out", h.fds.dup2(out_pipe[1], 1));
        check_true("stderr-dup2-err", h.fds.dup2(err_pipe[1], 2));

        const char* argv[] = {"stderr_demo", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "stderr_demo";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("stderr-spawn", sp);

        std::array<char, 16> out_buf{};
        std::array<char, 16> err_buf{};
        util::usize out_size = 0;
        util::usize err_size = 0;
        auto out = read_from_fd(h.api, out_pipe[0], out_buf, out_size);
        auto err = read_from_fd(h.api, err_read, err_buf, err_size);
        check_eq("stderr-out", out, std::string_view{"out\n"});
        check_eq("stderr-err", err, std::string_view{"err\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("stderr-wait", st);
    }

    [[maybe_unused]] void test_exit_code() noexcept {
        Harness h{};
        auto rreg = h.procs.register_executable("exit_code", &exit_code_main);
        check_true("exit-register", rreg);

        const char* argv[] = {"exit_code", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "exit_code";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        if (!sp) {
            fail();
        }
        auto st = h.procs.waitpid(sp.value().pid, 0);
        if (!st) {
            fail();
        }
        if (st.value().code != 0) {
            fail();
        }
    }

    void test_modulex_register() noexcept {
        Harness h{};

        struct ModuleXStub {
            modulex::ImageHeader hdr{};
            std::array<std::byte, 4> text{};
            modulex::Symbol syms[1]{};
            char strtab[6]{};
        };

        ModuleXStub img{};
        img.hdr.magic = modulex::k_magic;
        img.hdr.version = modulex::k_version;
        img.hdr.text_offset = static_cast<util::u32>(offsetof(ModuleXStub, text));
        img.hdr.text_size = static_cast<util::u32>(img.text.size());
        img.hdr.entry_offset = 0;
        img.hdr.image_size = static_cast<util::u32>(sizeof(ModuleXStub));
        img.hdr.sym_offset = static_cast<util::u32>(offsetof(ModuleXStub, syms));
        img.hdr.sym_size = static_cast<util::u32>(sizeof(img.syms));
        img.hdr.str_offset = static_cast<util::u32>(offsetof(ModuleXStub, strtab));
        img.hdr.str_size = static_cast<util::u32>(sizeof(img.strtab));
        img.syms[0].name_offset = 0;
        img.syms[0].value = 0;
        img.syms[0].size = 0;
        img.syms[0].kind = modulex::SymbolKind::external;
        img.syms[0].flags = 0;
        std::memcpy(img.strtab, "entry", 5);

        posix::ModuleXLoadConfig cfg{};
        cfg.resolve_external = &resolve_modulex_entry;
        cfg.use_entry_symbol = true;
        cfg.entry_symbol = "entry";
        auto rreg = h.procs.register_modulex_image("modulex_stub", &img.hdr, cfg);
        check_true("modulex-register", rreg);

        int pipefd[2]{-1, -1};
        check_eq("modulex-pipe", h.api.pipe(pipefd), 0);
        check_true("modulex-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"modulex:modulex_stub", nullptr};
        posix::SpawnConfig scfg{};
        scfg.path = "modulex:modulex_stub";
        scfg.argv = std::span<const char* const>(argv, 1);
        auto sp = h.procs.spawn(scfg);
        check_true("modulex-spawn", sp);

        std::array<char, 8> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("modulex-out", out, std::string_view{"mx\n"});
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("modulex-wait", st);
    }

    void test_elf_prefix_stub() noexcept {
        Harness h{};

        const char* argv[] = {"elf:/hello", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "elf:/hello";
        cfg.argv = std::span<const char* const>(argv, 1);
        auto image = h.procs.load_image(cfg);
        check_true("elf-not-supported", !image && image.error() == util::Errc::not_supported);
    }

    void test_elf_header_stub() noexcept {
        posix::ElfHeader64 hdr{};
        hdr.ident[0] = 0x7f;
        hdr.ident[1] = 'E';
        hdr.ident[2] = 'L';
        hdr.ident[3] = 'F';
        hdr.ident[4] = 2;
        hdr.ident[5] = 1;
        hdr.entry = 1;
        posix::ElfProgramHeader64 ph{};
        ph.type = posix::kElfPtLoad;
        ph.offset = 0;
        ph.filesz = 0;

        posix::ElfLoadConfig cfg{};
        cfg.image_base = &hdr;
        cfg.image_size = sizeof(hdr);
        cfg.load_base = nullptr;
        cfg.load_size = 0;
        cfg.load_align = 16;
        auto ok = posix::load_elf_image(cfg);
        check_true("elf-header-nosupport", !ok && ok.error() == util::Errc::not_supported);

        hdr.ident[0] = 0x00;
        auto bad = posix::load_elf_image(cfg);
        check_true("elf-header-bad", !bad && bad.error() == util::Errc::invalid_arg);

        hdr.ident[0] = 0x7f;
        hdr.phoff = sizeof(hdr) + 8;
        auto bad_phoff = posix::load_elf_image(cfg);
        check_true("elf-header-bad-phoff", !bad_phoff && bad_phoff.error() == util::Errc::invalid_arg);

        hdr.phoff = sizeof(hdr);
        hdr.phentsize = sizeof(posix::ElfProgramHeader64);
        hdr.phnum = 2;
        auto bad_phrange = posix::load_elf_image(cfg);
        check_true("elf-header-bad-phrange", !bad_phrange && bad_phrange.error() == util::Errc::invalid_arg);

        struct ElfStub {
            posix::ElfHeader64 header{};
            posix::ElfProgramHeader64 phdr{};
            util::u8 payload[4]{};
        } stub{};
        stub.header = hdr;
        stub.header.phoff = static_cast<util::u64>(offsetof(ElfStub, phdr));
        stub.header.phentsize = sizeof(posix::ElfProgramHeader64);
        stub.header.phnum = 1;
        stub.header.entry = 0x1002;
        stub.phdr = ph;
        stub.phdr.offset = static_cast<util::u64>(offsetof(ElfStub, payload));
        stub.phdr.vaddr = 0x1000;
        stub.phdr.filesz = sizeof(stub.payload);
        stub.phdr.memsz = sizeof(stub.payload) + 4;
        stub.payload[0] = 0x11;
        stub.payload[1] = 0x22;
        stub.payload[2] = 0x33;
        stub.payload[3] = 0x44;
        alignas(16) std::array<util::u8, 16> load_buf{};
        load_buf.fill(0xFF);
        cfg.image_base = &stub;
        cfg.image_size = sizeof(stub);
        cfg.load_base = load_buf.data();
        cfg.load_size = load_buf.size();
        cfg.load_align = 16;
        auto ok2 = posix::load_elf_image(cfg);
        check_true("elf-header-ptload", ok2);
        using EntryPtr = decltype(ok2.value().entry);
        auto expected_entry = reinterpret_cast<EntryPtr>(load_buf.data() + 2);
        check_true("elf-entry-addr", ok2.value().entry == expected_entry);
        check_true("elf-bss-zero", load_buf[4] == 0 && load_buf[5] == 0 && load_buf[6] == 0 && load_buf[7] == 0);

        stub.phdr.memsz = 2;
        stub.phdr.filesz = 4;
        auto bad_memsz = posix::load_elf_image(cfg);
        check_true("elf-load-memsz-too-small", !bad_memsz && bad_memsz.error() == util::Errc::invalid_arg);
        stub.phdr.filesz = sizeof(stub.payload);
        stub.phdr.memsz = sizeof(stub.payload) + 4;

        cfg.load_size = 2;
        auto too_small = posix::load_elf_image(cfg);
        check_true("elf-load-too-small", !too_small && too_small.error() == util::Errc::invalid_arg);

        cfg.load_base = load_buf.data() + 1;
        cfg.load_size = load_buf.size() - 1;
        auto unaligned = posix::load_elf_image(cfg);
        check_true("elf-load-unaligned", !unaligned && unaligned.error() == util::Errc::invalid_arg);

        stub.phdr.flags = posix::kElfPfW | posix::kElfPfX;
        cfg.load_base = load_buf.data();
        cfg.load_size = load_buf.size();
        auto wx = posix::load_elf_image(cfg);
        check_true("elf-load-wx", !wx && wx.error() == util::Errc::invalid_arg);

        stub.phdr.flags = 0;
        stub.header.entry = 0x2000;
        cfg.load_base = load_buf.data();
        cfg.load_size = load_buf.size();
        auto bad_entry = posix::load_elf_image(cfg);
        check_true("elf-entry-outside", !bad_entry && bad_entry.error() == util::Errc::invalid_arg);

        stub.header.entry = 0x1000;
        stub.phdr.align = 16;
        stub.phdr.offset = static_cast<util::u64>(offsetof(ElfStub, payload)) + 1;
        stub.phdr.vaddr = 0x1000;
        auto bad_align = posix::load_elf_image(cfg);
        check_true("elf-load-align-mismatch", !bad_align && bad_align.error() == util::Errc::invalid_arg);

        struct ElfOverlapStub {
            posix::ElfHeader64 header{};
            posix::ElfProgramHeader64 phdr[2]{};
            util::u8 payload[8]{};
        } overlap{};
        overlap.header = hdr;
        overlap.header.phoff = static_cast<util::u64>(offsetof(ElfOverlapStub, phdr));
        overlap.header.phentsize = sizeof(posix::ElfProgramHeader64);
        overlap.header.phnum = 2;
        overlap.header.entry = 0x1000;
        overlap.phdr[0] = ph;
        overlap.phdr[0].offset = static_cast<util::u64>(offsetof(ElfOverlapStub, payload));
        overlap.phdr[0].vaddr = 0x1000;
        overlap.phdr[0].filesz = 4;
        overlap.phdr[0].memsz = 8;
        overlap.phdr[1] = ph;
        overlap.phdr[1].offset = static_cast<util::u64>(offsetof(ElfOverlapStub, payload)) + 4;
        overlap.phdr[1].vaddr = 0x1004;
        overlap.phdr[1].filesz = 4;
        overlap.phdr[1].memsz = 4;
        cfg.image_base = &overlap;
        cfg.image_size = sizeof(overlap);
        cfg.load_base = load_buf.data();
        cfg.load_size = load_buf.size();
        auto bad_overlap = posix::load_elf_image(cfg);
        check_true("elf-load-overlap", !bad_overlap && bad_overlap.error() == util::Errc::invalid_arg);
    }

    void test_elf_file_spawn() noexcept {
        fs::clear_mounts();
        static RamFsMount<256, 32, 128> ramfs{};
        new (&ramfs) RamFsMount<256, 32, 128>();
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("elf-file-mount", st);

        struct ElfFileStub {
            posix::ElfHeader64 header{};
            posix::ElfProgramHeader64 phdr{};
            util::u8 payload[4]{};
        } stub{};
        stub.header.ident[0] = 0x7f;
        stub.header.ident[1] = 'E';
        stub.header.ident[2] = 'L';
        stub.header.ident[3] = 'F';
        stub.header.ident[4] = 2;
        stub.header.ident[5] = 1;
        stub.header.entry = 0x1000;
        stub.header.phoff = static_cast<util::u64>(offsetof(ElfFileStub, phdr));
        stub.header.phentsize = sizeof(posix::ElfProgramHeader64);
        stub.header.phnum = 1;
        stub.phdr.type = posix::kElfPtLoad;
        stub.phdr.offset = static_cast<util::u64>(offsetof(ElfFileStub, payload));
        stub.phdr.vaddr = 0x1000;
        stub.phdr.filesz = sizeof(stub.payload);
        stub.phdr.memsz = sizeof(stub.payload);
        stub.payload[0] = 0xAA;
        stub.payload[1] = 0xBB;
        stub.payload[2] = 0xCC;
        stub.payload[3] = 0xDD;

        Harness h{};
        h.procs.bind_file_service(h.files);
        h.procs.enable_elf_exec(true);
        h.procs.set_elf_exec_stub(&elf_entry_main);
        int fd = h.api.open("/elf_stub.bin", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("elf-file-open", fd >= 0);
        auto w = h.api.write(fd, &stub, sizeof(stub));
        check_true("elf-file-write", w == static_cast<posix::ssize_t>(sizeof(stub)));
        (void)h.api.close(fd);

        int pipefd[2]{-1, -1};
        check_eq("elf-file-pipe", h.api.pipe(pipefd), 0);
        check_true("elf-file-dup2", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"elf:/elf_stub.bin", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "elf:/elf_stub.bin";
        cfg.argv = std::span<const char* const>(argv, 1);
        auto img = h.procs.load_image(cfg);
        if (!img) {
            char buf[96]{};
            std::snprintf(buf, sizeof(buf), "[posix-smoke] programs elf-file-load fail: err=%lld",
                to_ll(img.error()));
            log_line(buf);
            fail();
        }
        log_step("elf-file-load", true);
        auto sp = h.procs.spawn(cfg);
        check_true("elf-file-spawn", sp);
        (void)h.procs.waitpid(sp.value().pid, 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }
        std::array<char, 8> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("elf-file-out", out, std::string_view{"mx\n"});
        h.procs.set_elf_exec_stub(nullptr);
        h.procs.enable_elf_exec(false);
    }


    void test_elf_file_samples_probe() noexcept {
        fs::clear_mounts();
        static RamFsMount<256, 64, 512> ramfs{};
        new (&ramfs) RamFsMount<256, 64, 512>();
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("cat-file-mount", st);

        Harness h{};
        h.procs.bind_file_service(h.files);
        h.procs.enable_elf_exec(true);
        h.procs.enable_elf_hostcalls(true);

        int hello_fd = h.api.open("/hello.elf", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("cat-file-hello-open", hello_fd >= 0);
        auto hello_write = h.api.write(hello_fd, hello_elf, hello_elf_len);
        check_true("cat-file-hello-write", hello_write == static_cast<posix::ssize_t>(hello_elf_len));
        check_eq("cat-file-hello-close", h.api.close(hello_fd), 0);

        const char* hello_argv[] = {"elf:/hello.elf", nullptr};
        posix::SpawnConfig hello_cfg{};
        hello_cfg.path = "elf:/hello.elf";
        hello_cfg.argv = std::span<const char* const>(hello_argv, 1);
        auto hello_img = h.procs.load_image(hello_cfg);
        if (!hello_img) {
            char buf[96]{};
            std::snprintf(buf, sizeof(buf), "[posix-smoke] programs cat-file-hello-load fail: err=%lld", to_ll(hello_img.error()));
            log_line(buf);
        }
        check_true("cat-file-hello-load", hello_img);

        int cat_fd = h.api.open("/cat.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("cat-file-data-open", cat_fd >= 0);
        const char cat_payload[] = "cat-data\ncat-data\ncat-data\ncat-data\ncat-data\ncat-data\ncat-data\ncat-data\ncat-data\n";
        auto cat_write = h.api.write(cat_fd, cat_payload, sizeof(cat_payload) - 1);
        check_true("cat-file-data-write", cat_write == static_cast<posix::ssize_t>(sizeof(cat_payload) - 1));
        check_eq("cat-file-data-close", h.api.close(cat_fd), 0);

        int cat_elf_fd = h.api.open("/cat_file.elf", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("cat-file-elf-open", cat_elf_fd >= 0);
        auto cat_elf_write = h.api.write(cat_elf_fd, cat_file_elf, cat_file_elf_len);
        check_true("cat-file-elf-write", cat_elf_write == static_cast<posix::ssize_t>(cat_file_elf_len));
        check_eq("cat-file-elf-close", h.api.close(cat_elf_fd), 0);

        const char* cat_argv[] = {"elf:/cat_file.elf", "/cat.txt", nullptr};
        posix::SpawnConfig cat_cfg{};
        cat_cfg.path = "elf:/cat_file.elf";
        cat_cfg.argv = std::span<const char* const>(cat_argv, 2);
        auto cat_img = h.procs.load_image(cat_cfg);
        if (!cat_img) {
            char buf[96]{};
            std::snprintf(buf, sizeof(buf), "[posix-smoke] programs cat-file-load fail: err=%lld", to_ll(cat_img.error()));
            log_line(buf);
        }
        check_true("cat-file-load", cat_img);

        std::array<char, 128> cat_stdout_buf{};
        TermStubCtx cat_stdout_ctx{};
        cat_stdout_ctx.capture = cat_stdout_buf.data();
        cat_stdout_ctx.capture_cap = cat_stdout_buf.size();
        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &kTermOps;
        term_entry.ctx = &cat_stdout_ctx;
        check_true("cat-file-attach-term", h.fds.attach(term_entry, 1));

        {
            int err_pipe[2]{-1, -1};
            check_eq("cat-file-err-pipe", h.api.pipe(err_pipe), 0);
            cat_cfg.stdio_err = err_pipe[1];

            auto cat_sp = h.procs.spawn(cat_cfg);
            if (!cat_sp) {
                char buf[96]{};
                std::snprintf(buf, sizeof(buf), "[posix-smoke] programs cat-file-spawn fail: err=%lld",
                    to_ll(cat_sp.error()));
                log_line(buf);
            }
            check_true("cat-file-spawn", cat_sp);
            auto cat_st = h.procs.waitpid(cat_sp.value().pid, 0);
            if (!cat_st) {
                char buf[96]{};
                std::snprintf(buf, sizeof(buf), "[posix-smoke] programs cat-file-wait fail: err=%lld",
                    to_ll(cat_st.error()));
                log_line(buf);
            }
            check_true("cat-file-wait", cat_st);
            check_eq("cat-file-code", cat_st.value().code, 0);

            (void)h.api.close(err_pipe[1]);
            std::array<char, 32> err_buf{};
            util::usize err_size = 0;
            auto err = read_from_fd(h.api, err_pipe[0], err_buf, err_size);
            check_eq("cat-file-err", err, std::string_view{"A\nB\nC\nD\nE\nF\nF\nG\n"});
            check_eq("cat-file-out",
                std::string_view{cat_stdout_buf.data(), cat_stdout_ctx.capture_size},
                std::string_view{cat_payload, sizeof(cat_payload) - 1});
        }

        {
            cat_stdout_ctx.capture_size = 0;
            constexpr char cat_stdin_payload[] = "stdin-data\nstdin-data\n";

            int in_pipe[2]{-1, -1};
            check_eq("cat-stdin-in-pipe", h.api.pipe(in_pipe), 0);
            auto stdin_write = h.api.write(in_pipe[1], cat_stdin_payload, sizeof(cat_stdin_payload) - 1);
            check_true("cat-stdin-in-write", stdin_write == static_cast<posix::ssize_t>(sizeof(cat_stdin_payload) - 1));
            check_eq("cat-stdin-in-close", h.api.close(in_pipe[1]), 0);

            int err_pipe[2]{-1, -1};
            check_eq("cat-stdin-err-pipe", h.api.pipe(err_pipe), 0);

            const char* stdin_argv[] = {"elf:/cat_file.elf", "-", nullptr};
            posix::SpawnConfig stdin_cfg{};
            stdin_cfg.path = "elf:/cat_file.elf";
            stdin_cfg.argv = std::span<const char* const>(stdin_argv, 2);
            stdin_cfg.stdio_in = in_pipe[0];
            stdin_cfg.stdio_err = err_pipe[1];

            auto stdin_sp = h.procs.spawn(stdin_cfg);
            if (!stdin_sp) {
                char buf[96]{};
                std::snprintf(buf, sizeof(buf), "[posix-smoke] programs cat-stdin-spawn fail: err=%lld",
                    to_ll(stdin_sp.error()));
                log_line(buf);
            }
            check_true("cat-stdin-spawn", stdin_sp);
            auto stdin_st = h.procs.waitpid(stdin_sp.value().pid, 0);
            if (!stdin_st) {
                char buf[96]{};
                std::snprintf(buf, sizeof(buf), "[posix-smoke] programs cat-stdin-wait fail: err=%lld",
                    to_ll(stdin_st.error()));
                log_line(buf);
            }
            check_true("cat-stdin-wait", stdin_st);
            check_eq("cat-stdin-code", stdin_st.value().code, 0);

            (void)h.api.close(err_pipe[1]);
            std::array<char, 32> err_buf{};
            util::usize err_size = 0;
            auto err = read_from_fd(h.api, err_pipe[0], err_buf, err_size);
            check_eq("cat-stdin-err", err, std::string_view{"A\nS\nC\nD\nE\nF\nG\n"});
            check_eq("cat-stdin-out",
                std::string_view{cat_stdout_buf.data(), cat_stdout_ctx.capture_size},
                std::string_view{cat_stdin_payload, sizeof(cat_stdin_payload) - 1});
        }

        h.procs.enable_elf_hostcalls(false);
        h.procs.enable_elf_exec(false);
    }

    void test_elf_file_direct_exec() noexcept {
        check_true("elf-direct-mount", fs::mount_count() > 0);

        Harness h{};
        h.procs.bind_file_service(h.files);
        h.procs.enable_elf_exec(true);
        h.procs.enable_elf_hostcalls(true);

        int hello_fd = h.api.open("/hello.elf", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("elf-direct-open", hello_fd >= 0);
        auto hello_write = h.api.write(hello_fd, hello_elf, hello_elf_len);
        check_true("elf-direct-write", hello_write == static_cast<posix::ssize_t>(hello_elf_len));
        check_eq("elf-direct-close", h.api.close(hello_fd), 0);

        int pipefd[2]{-1, -1};
        check_eq("elf-direct-pipe", h.api.pipe(pipefd), 0);

        const char* argv[] = {"hello", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "/hello.elf";
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.stdio_out = pipefd[1];

        auto sp = h.procs.spawn(cfg);
        check_true("elf-direct-spawn", sp);
        (void)h.procs.waitpid(sp.value().pid, 0);
        (void)h.api.close(pipefd[1]);

        std::array<char, 16> buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
        check_eq("elf-direct-out", out, std::string_view{"hello\n"});

        h.procs.enable_elf_hostcalls(false);
        h.procs.enable_elf_exec(false);
    }

    void test_elf_real_samples() noexcept {
        Harness h{};
        h.procs.enable_elf_exec(true);
        h.procs.enable_elf_hostcalls(true);
        auto spawn_checked = [&](const char* label, const posix::SpawnConfig& cfg) noexcept {
            auto sp = h.procs.spawn(cfg);
            if (!sp) {
                auto img = h.procs.load_image(cfg);
                if (!img) {
                    char buf2[96]{};
                    std::snprintf(buf2, sizeof(buf2),
                        "[posix-smoke] programs %s load fail: err=%lld", label, to_ll(img.error()));
                    log_line(buf2);
                }
                char buf[96]{};
                std::snprintf(buf, sizeof(buf),
                    "[posix-smoke] programs %s fail: err=%lld", label, to_ll(sp.error()));
                log_line(buf);
                log_step(label, false);
                fail();
            }
            log_step(label, true);
            return sp.value();
        };
        check_true("elf-real-reg-hello",
            h.procs.register_elf_mem("hello", hello_elf, hello_elf_len));
        check_true("elf-real-reg-argv",
            h.procs.register_elf_mem("argv_dump", argv_dump_elf, argv_dump_elf_len));
        check_true("elf-real-reg-env",
            h.procs.register_elf_mem("env_dump", env_dump_elf, env_dump_elf_len));
        check_true("elf-real-reg-stderr",
            h.procs.register_elf_mem("stderr_demo", stderr_demo_elf, stderr_demo_elf_len));
        check_true("elf-real-reg-exit",
            h.procs.register_elf_mem("exit_code", exit_code_elf, exit_code_elf_len));
        check_true("elf-real-reg-getpid",
            h.procs.register_elf_mem("getpid", getpid_elf, getpid_elf_len));
        check_true("elf-real-reg-sleep",
            h.procs.register_elf_mem("sleep", sleep_elf, sleep_elf_len));
        check_true("elf-real-reg-kill-self",
            h.procs.register_elf_mem("kill_self", kill_self_elf, kill_self_elf_len));

        {
            int pipefd[2]{-1, -1};
            check_eq("elf-real-hello-pipe", h.api.pipe(pipefd), 0);
            const char* argv[] = {"elfmem:hello", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:hello";
            cfg.argv = std::span<const char* const>(argv, 1);
            cfg.stdio_out = pipefd[1];
            auto sp = spawn_checked("elf-real-hello-spawn", cfg);
            auto st = h.procs.waitpid(sp.pid, 0);
            check_true("elf-real-hello-wait", st);
            check_eq("elf-real-hello-code", st.value().code, 0);
            (void)h.api.close(pipefd[1]);
            std::array<char, 16> buf{};
            util::usize out_size = 0;
            auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
            check_eq("elf-real-hello-out", out, std::string_view{"hello\n"});
        }

        {
            int pipefd[2]{-1, -1};
            check_eq("elf-real-argv-pipe", h.api.pipe(pipefd), 0);
            const char* argv[] = {"elfmem:argv_dump", "a", "b", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:argv_dump";
            cfg.argv = std::span<const char* const>(argv, 3);
            cfg.stdio_out = pipefd[1];
            auto sp = spawn_checked("elf-real-argv-spawn", cfg);
            auto st = h.procs.waitpid(sp.pid, 0);
            check_true("elf-real-argv-wait", st);
            check_eq("elf-real-argv-code", st.value().code, 0);
            (void)h.api.close(pipefd[1]);
            std::array<char, 96> buf{};
            util::usize out_size = 0;
            auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
            const char expected[] = "argv[0]=elfmem:argv_dump\nargv[1]=a\nargv[2]=b\n";
            check_eq("elf-real-argv-out", out, std::string_view{expected});
        }

        {
            Harness h_env{};
            h_env.procs.enable_elf_exec(true);
            h_env.procs.enable_elf_hostcalls(true);
            check_true("elf-real-env-reg",
                h_env.procs.register_elf_mem("env_dump", env_dump_elf, env_dump_elf_len));

            int out_fd = h_env.api.open("/env_out.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
            check_true("elf-real-env-open", out_fd >= 0);
            check_true("elf-real-env-dup2", h_env.fds.dup2(out_fd, 1));
            const char* argv[] = {"elfmem:env_dump", nullptr};
            const char* envp[] = {"PATH=/bin:/usr/bin", "FOO=BAR", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:env_dump";
            cfg.argv = std::span<const char* const>(argv, 1);
            cfg.envp = std::span<const char* const>(envp, 2);
            auto sp = h_env.procs.spawn(cfg);
            check_true("elf-real-env-spawn", sp);
            auto st = h_env.procs.waitpid(sp.value().pid, 0);
            check_true("elf-real-env-wait", st);
            check_eq("elf-real-env-code", st.value().code, 0);
            check_eq("elf-real-env-close-out", h_env.api.close(out_fd), 0);

            int read_fd = h_env.api.open("/env_out.txt", posix::O_RDONLY, 0);
            check_true("elf-real-env-open-read", read_fd >= 0);
            std::array<char, 128> buf{};
            util::usize out_size = 0;
            auto out = read_from_fd(h_env.api, read_fd, buf, out_size);
            const char expected[] = "env[0]=PATH=/bin:/usr/bin\nenv[1]=FOO=BAR\n";
            check_eq("elf-real-env-out", out, std::string_view{expected});
            (void)h_env.api.close(read_fd);
        }

        {
            Harness h_stderr{};
            h_stderr.procs.enable_elf_exec(true);
            h_stderr.procs.enable_elf_hostcalls(true);
            check_true("elf-real-stderr-reg",
                h_stderr.procs.register_elf_mem("stderr_demo", stderr_demo_elf, stderr_demo_elf_len));

            int out_pipe[2]{-1, -1};
            int err_pipe[2]{-1, -1};
            check_eq("elf-real-stderr-out-pipe", h_stderr.api.pipe(out_pipe), 0);
            check_eq("elf-real-stderr-err-pipe", h_stderr.api.pipe(err_pipe), 0);
            int err_read = err_pipe[0];
            if (err_read == 2) {
                check_true("elf-real-stderr-move-read", h_stderr.fds.dup2(err_read, 6));
                err_read = 6;
            }
            check_true("elf-real-stderr-dup2-out", h_stderr.fds.dup2(out_pipe[1], 1));
            check_true("elf-real-stderr-dup2-err", h_stderr.fds.dup2(err_pipe[1], 2));
            const char* argv[] = {"elfmem:stderr_demo", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:stderr_demo";
            cfg.argv = std::span<const char* const>(argv, 1);
            auto sp = h_stderr.procs.spawn(cfg);
            check_true("elf-real-stderr-spawn", sp);
            auto st = h_stderr.procs.waitpid(sp.value().pid, 0);
            check_true("elf-real-stderr-wait", st);
            check_eq("elf-real-stderr-code", st.value().code, 0);
            std::array<char, 16> out_buf{};
            std::array<char, 16> err_buf{};
            util::usize out_size = 0;
            util::usize err_size = 0;
            auto out = read_from_fd(h_stderr.api, out_pipe[0], out_buf, out_size);
            auto err = read_from_fd(h_stderr.api, err_read, err_buf, err_size);
            check_eq("elf-real-stderr-out", out, std::string_view{"out\n"});
            check_eq("elf-real-stderr-err", err, std::string_view{"err\n"});
        }

        {
            Harness h_merge{};
            h_merge.procs.enable_elf_exec(true);
            h_merge.procs.enable_elf_hostcalls(true);
            check_true("elf-real-stderr-merge-reg",
                h_merge.procs.register_elf_mem("stderr_demo", stderr_demo_elf, stderr_demo_elf_len));

            int pipefd[2]{-1, -1};
            check_eq("elf-real-stderr-merge-pipe", h_merge.api.pipe(pipefd), 0);
            const char* argv[] = {"elfmem:stderr_demo", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:stderr_demo";
            cfg.argv = std::span<const char* const>(argv, 1);
            cfg.stdio_out = pipefd[1];
            cfg.stdio_err = pipefd[1];
            auto sp = h_merge.procs.spawn(cfg);
            check_true("elf-real-stderr-merge-spawn", sp);
            auto st = h_merge.procs.waitpid(sp.value().pid, 0);
            check_true("elf-real-stderr-merge-wait", st);
            check_eq("elf-real-stderr-merge-code", st.value().code, 0);
            (void)h_merge.api.close(pipefd[1]);
            std::array<char, 16> buf{};
            util::usize out_size = 0;
            auto out = read_from_fd(h_merge.api, pipefd[0], buf, out_size);
            check_eq("elf-real-stderr-merge-out", out, std::string_view{"out\nerr\n"});
        }

        {
            Harness h_err_only{};
            h_err_only.procs.enable_elf_exec(true);
            h_err_only.procs.enable_elf_hostcalls(true);
            check_true("elf-real-stderr-only-reg",
                h_err_only.procs.register_elf_mem("stderr_demo", stderr_demo_elf, stderr_demo_elf_len));

            int null_fd = h_err_only.api.open("/dev/null", posix::O_WRONLY, 0);
            check_true("elf-real-stderr-only-null", null_fd >= 0);
            check_true("elf-real-stderr-only-dup2-null", h_err_only.fds.dup2(null_fd, 1));

            int err_pipe[2]{-1, -1};
            check_eq("elf-real-stderr-only-pipe", h_err_only.api.pipe(err_pipe), 0);
            int err_read = err_pipe[0];
            if (err_read == 2) {
                check_true("elf-real-stderr-only-move-read", h_err_only.fds.dup2(err_read, 7));
                err_read = 7;
            }
            const char* argv[] = {"elfmem:stderr_demo", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:stderr_demo";
            cfg.argv = std::span<const char* const>(argv, 1);
            cfg.stdio_err = err_pipe[1];
            auto sp = h_err_only.procs.spawn(cfg);
            check_true("elf-real-stderr-only-spawn", sp);
            auto st = h_err_only.procs.waitpid(sp.value().pid, 0);
            check_true("elf-real-stderr-only-wait", st);
            check_eq("elf-real-stderr-only-code", st.value().code, 0);
            (void)h_err_only.api.close(err_pipe[1]);
            (void)h_err_only.api.close(null_fd);
            std::array<char, 16> buf{};
            util::usize out_size = 0;
            auto out = read_from_fd(h_err_only.api, err_read, buf, out_size);
            check_eq("elf-real-stderr-only-out", out, std::string_view{"err\n"});
        }

        {
            const char* argv[] = {"elfmem:exit_code", "7", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:exit_code";
            cfg.argv = std::span<const char* const>(argv, 2);
            auto sp = spawn_checked("elf-real-exit-spawn", cfg);
            auto st2 = h.procs.waitpid(sp.pid, 0);
            check_true("elf-real-exit-wait", st2);
            check_eq("elf-real-exit-code", st2.value().code, 7);
        }

        {
            int pipefd[2]{-1, -1};
            check_eq("elf-real-getpid-pipe", h.api.pipe(pipefd), 0);
            const char* argv[] = {"elfmem:getpid", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:getpid";
            cfg.argv = std::span<const char* const>(argv, 1);
            cfg.stdio_out = pipefd[1];
            auto sp = spawn_checked("elf-real-getpid-spawn", cfg);
            auto st = h.procs.waitpid(sp.pid, 0);
            check_true("elf-real-getpid-wait", st);
            check_eq("elf-real-getpid-code", st.value().code, 0);
            (void)h.api.close(pipefd[1]);
            std::array<char, 16> buf{};
            util::usize out_size = 0;
            auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
            char expected[16]{};
            std::snprintf(expected, sizeof(expected), "%d\n", sp.pid.value);
            check_eq("elf-real-getpid-out", out, std::string_view{expected});
        }

        {
            int pipefd[2]{-1, -1};
            check_eq("elf-real-sleep-pipe", h.api.pipe(pipefd), 0);
            const char* argv[] = {"elfmem:sleep", "2", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:sleep";
            cfg.argv = std::span<const char* const>(argv, 2);
            cfg.stdio_out = pipefd[1];
            const auto sleep_before = test_clock_ticks_ms();
            auto sp = spawn_checked("elf-real-sleep-spawn", cfg);
            auto st = h.procs.waitpid(sp.pid, 0);
            check_true("elf-real-sleep-wait", st);
            check_eq("elf-real-sleep-code", st.value().code, 0);
            const auto sleep_after = test_clock_ticks_ms();
            check_true("elf-real-sleep-clock", sleep_after >= sleep_before + 2000);
            (void)h.api.close(pipefd[1]);
            std::array<char, 16> buf{};
            util::usize out_size = 0;
            auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
            check_eq("elf-real-sleep-out", out, std::string_view{"slept\n"});
        }

        {
            const auto run_kill_case = [&](const char* prefix, const char* signal_arg, int expected_signal) noexcept {
                int pipefd[2]{-1, -1};
                std::array<char, 64> label_buf{};
                const auto label = [&](const char* suffix) noexcept -> const char* {
                    std::snprintf(label_buf.data(), label_buf.size(), "%s-%s", prefix, suffix);
                    return label_buf.data();
                };

                check_eq(label("pipe"), h.api.pipe(pipefd), 0);
                const char* argv[] = {"elfmem:kill_self", signal_arg, nullptr};
                posix::SpawnConfig cfg{};
                cfg.path = "elfmem:kill_self";
                cfg.argv = signal_arg != nullptr ? std::span<const char* const>(argv, 2)
                                                 : std::span<const char* const>(argv, 1);
                cfg.stdio_out = pipefd[1];

                auto sp = spawn_checked(label("spawn"), cfg);
                auto st = h.procs.waitpid(sp.pid, 0);
                check_true(label("wait"), st);
                check_eq(label("kind"), st.value().kind, posix::WaitKind::signaled);
                check_eq(label("code"), st.value().code, expected_signal);
                (void)h.api.close(pipefd[1]);
                std::array<char, 32> buf{};
                util::usize out_size = 0;
                auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
                check_eq(label("out"), out, std::string_view{"before-kill\n"});
            };

            run_kill_case("elf-real-kill-self-term", nullptr, posix::SIGTERM);
            run_kill_case("elf-real-kill-self-int", "INT", posix::SIGINT);
            run_kill_case("elf-real-kill-self-kill", "KILL", posix::SIGKILL);
        }

        h.procs.enable_elf_hostcalls(false);
        h.procs.enable_elf_exec(false);
    }

} // namespace

export void run_posix_program_exec_smoke_tests() noexcept {
    using namespace posix::testsupport;
    log_line("[posix-smoke] programs phase hello begin");
    test_hello();
    log_line("[posix-smoke] programs phase hello end");
    log_line("[posix-smoke] programs phase argv-dump begin");
    test_argv_dump();
    log_line("[posix-smoke] programs phase argv-dump end");
    log_line("[posix-smoke] programs phase crt-probe begin");
    test_crt_probe();
    log_line("[posix-smoke] programs phase crt-probe end");
    log_line("[posix-smoke] programs phase crt-exit begin");
    test_crt_exit();
    log_line("[posix-smoke] programs phase crt-exit end");
    log_line("[posix-smoke] programs phase crt-errno begin");
    test_crt_errno();
    log_line("[posix-smoke] programs phase crt-errno end");
    log_line("[posix-smoke] programs phase crt-c-probe begin");
    test_crt_c_probe();
    log_line("[posix-smoke] programs phase crt-c-probe end");
    log_line("[posix-smoke] programs phase crt-c-exit begin");
    test_crt_c_exit();
    log_line("[posix-smoke] programs phase crt-c-exit end");
    log_line("[posix-smoke] programs phase crt-c-header-probe begin");
    test_crt_c_header_probe();
    log_line("[posix-smoke] programs phase crt-c-header-probe end");
    log_line("[posix-smoke] programs phase crt-c-header-exit begin");
    test_crt_c_header_exit();
    log_line("[posix-smoke] programs phase crt-c-header-exit end");
    log_line("[posix-smoke] programs phase crt-c-fs-header begin");
    test_crt_c_fs_header();
    log_line("[posix-smoke] programs phase crt-c-fs-header end");
    log_line("[posix-smoke] programs phase newlib-syscall begin");
    test_newlib_syscall_probe();
    log_line("[posix-smoke] programs phase newlib-syscall end");
    log_line("[posix-smoke] programs phase newlib-dup begin");
    test_newlib_dup();
    log_line("[posix-smoke] programs phase newlib-dup end");
    log_line("[posix-smoke] programs phase newlib-dup2 begin");
    test_newlib_dup2();
    log_line("[posix-smoke] programs phase newlib-dup2 end");
    log_line("[posix-smoke] programs phase newlib-fcntl begin");
    test_newlib_fcntl();
    log_line("[posix-smoke] programs phase newlib-fcntl end");
    log_line("[posix-smoke] programs phase newlib-pipe begin");
    test_newlib_pipe();
    log_line("[posix-smoke] programs phase newlib-pipe end");
    log_line("[posix-smoke] programs phase cloexec begin");
    test_spawn_cloexec();
    log_line("[posix-smoke] programs phase cloexec end");
    log_line("[posix-smoke] programs phase newlib-kill begin");
    test_newlib_kill_self();
    log_line("[posix-smoke] programs phase newlib-kill end");
    log_line("[posix-smoke] programs phase newlib-lseek begin");
    test_newlib_lseek();
    log_line("[posix-smoke] programs phase newlib-lseek end");
    log_line("[posix-smoke] programs phase newlib-path begin");
    test_newlib_path();
    log_line("[posix-smoke] programs phase newlib-path end");
    log_line("[posix-smoke] programs phase newlib-cwd begin");
    test_newlib_cwd();
    log_line("[posix-smoke] programs phase newlib-cwd end");
    log_line("[posix-smoke] programs phase exit-code begin");
    log_line("[posix-smoke] programs exit-register ok");
    log_line("[posix-smoke] programs phase exit-code end");
    log_line("[posix-smoke] programs phase explicit-exit begin");
    test_exit_code_explicit_exit();
    log_line("[posix-smoke] programs phase explicit-exit end");
    log_line("[posix-smoke] programs phase stderr-demo begin");
    test_stderr_demo();
    log_line("[posix-smoke] programs phase stderr-demo end");
    log_line("[posix-smoke] programs phase modulex begin");
    test_modulex_register();
    log_line("[posix-smoke] programs phase modulex end");
    log_line("[posix-smoke] programs phase elf-prefix begin");
    test_elf_prefix_stub();
    log_line("[posix-smoke] programs phase elf-prefix end");
    log_line("[posix-smoke] programs phase elf-header begin");
    test_elf_header_stub();
    log_line("[posix-smoke] programs phase elf-header end");
    log_line("[posix-smoke] programs phase elf-file-spawn begin");
    test_elf_file_spawn();
    log_line("[posix-smoke] programs phase elf-file-spawn end");
    log_line("[posix-smoke] programs phase file-samples begin");
    test_elf_file_samples_probe();
    log_line("[posix-smoke] programs phase file-samples end");
    log_line("[posix-smoke] programs phase direct-exec begin");
    test_elf_file_direct_exec();
    log_line("[posix-smoke] programs phase direct-exec end");
    log_line("[posix-smoke] programs phase real-samples begin");
    test_elf_real_samples();
    log_line("[posix-smoke] programs phase real-samples end");
}

#if defined(CHARM_POSIX_NEWLIB_STDIO_SMOKE) && CHARM_POSIX_NEWLIB_STDIO_SMOKE
export void run_posix_program_stdio_smoke_tests() noexcept {
    using namespace posix::testsupport;
    log_line("[posix-smoke] programs phase newlib-stdio begin");
    test_newlib_stdio();
    log_line("[posix-smoke] programs phase newlib-stdio end");
}
#endif

#endif
