//
// Program-driven smoke tests for minimal userland targets.
//

module;
#include <array>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string_view>
#include <type_traits>

export module posix.programs.tests;

#if defined(POSIX_PROGRAMS_SMOKE_TEST) && POSIX_PROGRAMS_SMOKE_TEST

import posix.api;
import posix.fd_table;
import posix.file;
import posix.pipe;
import posix.proc;
import util.core;
import util.error;

namespace {
#if defined(POSIX_SMOKE_USE_UART) && POSIX_SMOKE_USE_UART
    extern "C" void posix_smoke_emit(const char* msg) noexcept;
#endif
    using ApiType = posix::Api<16, 8, 64, 4, 4, 16>;

    [[noreturn]] inline void fail() noexcept { std::abort(); }
    inline void log_line(const char* msg) noexcept {
#if defined(POSIX_SMOKE_USE_UART) && POSIX_SMOKE_USE_UART
        posix_smoke_emit(msg);
#else
        std::printf("%s\n", msg);
#endif
    }
    inline void log_step(const char* label, bool ok) noexcept {
        char buf[96]{};
        std::snprintf(buf, sizeof(buf), "[posix-smoke] programs %s %s", label, ok ? "ok" : "fail");
        log_line(buf);
    }
    template <class T>
    inline long long to_ll(const T& v) noexcept {
        if constexpr (std::is_enum_v<T>) {
            return static_cast<long long>(static_cast<std::underlying_type_t<T>>(v));
        } else {
            return static_cast<long long>(v);
        }
    }
    template <class T>
    inline void check_true(const char* label, const T& v) noexcept {
        if (!static_cast<bool>(v)) {
            log_step(label, false);
            fail();
        }
        log_step(label, true);
    }
    template <class A, class B>
    inline void check_eq(const char* label, const A& a, const B& b) noexcept {
        if (!(a == b)) {
            char buf[160]{};
            if constexpr (std::is_same_v<std::remove_cvref_t<A>, std::string_view> &&
                          std::is_same_v<std::remove_cvref_t<B>, std::string_view>) {
                std::snprintf(buf, sizeof(buf),
                    "[posix-smoke] programs %s fail: expected=\"%.*s\" actual=\"%.*s\"",
                    label,
                    static_cast<int>(b.size()), b.data(),
                    static_cast<int>(a.size()), a.data());
            } else {
                std::snprintf(buf, sizeof(buf),
                    "[posix-smoke] programs %s fail: expected=%lld actual=%lld",
                    label, to_ll(b), to_ll(a));
            }
            log_line(buf);
            fail();
        }
        log_step(label, true);
    }

    struct ProgramEnv {
        static inline ApiType* api{nullptr};
    };

    inline int write_text(int fd, std::string_view text) noexcept {
        if (!ProgramEnv::api) return 1;
        auto w = ProgramEnv::api->write(fd, text.data(), text.size());
        if (w != static_cast<posix::ssize_t>(text.size())) return 2;
        return 0;
    }

    int hello_main(int, char**) {
        return write_text(1, "hello\n");
    }

    int argv_dump_main(int argc, char** argv) {
        if (!ProgramEnv::api) return 1;
        for (int i = 0; i < argc; ++i) {
            const char* arg = argv && argv[i] ? argv[i] : "";
            char buf[128]{};
            std::snprintf(buf, sizeof(buf), "argv[%d]=%s\n", i, arg);
            auto w = ProgramEnv::api->write(1, buf, std::char_traits<char>::length(buf));
            if (w < 0) return 2;
        }
        return 0;
    }

    int stderr_demo_main(int, char**) {
        const int r1 = write_text(1, "out\n");
        const int r2 = write_text(2, "err\n");
        return r1 == 0 && r2 == 0 ? 0 : 3;
    }

    int exit_code_main(int argc, char** argv) {
        if (argc < 2 || !argv || !argv[1]) return 0;
        return std::strtol(argv[1], nullptr, 10);
    }

    struct Harness {
        posix::FdTable<16> fds{};
        posix::FileService<16> files{};
        posix::PipeService<8, 64> pipes{};
        posix::ProcService<4, 4, 16, 16> procs{};
        ApiType api;

        Harness() : api(fds, files, pipes, procs) {
            fds.init();
            files.init();
            pipes.init();
            procs.init();
            procs.bind_fd_table(fds);
        }

        void bind_env() noexcept { ProgramEnv::api = &api; }
        void unbind_env() noexcept { ProgramEnv::api = nullptr; }
    };

    std::string_view read_from_fd(ApiType& api, int fd, std::span<char> out, util::usize& out_size) noexcept {
        auto r = api.read(fd, out.data(), out.size());
        if (r < 0) {
            out_size = 0;
            return {};
        }
        out_size = static_cast<util::usize>(r);
        return std::string_view{out.data(), out_size};
    }

    void test_hello() noexcept {
        Harness h{};
        h.bind_env();
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
        h.unbind_env();
    }

    void test_argv_dump() noexcept {
        Harness h{};
        h.bind_env();
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
        h.unbind_env();
    }

    void test_stderr_demo() noexcept {
        Harness h{};
        h.bind_env();
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
        h.unbind_env();
    }

    void test_exit_code() noexcept {
        Harness h{};
        h.bind_env();
        auto rreg = h.procs.register_executable("exit_code", &exit_code_main);
        check_true("exit-register", rreg);

        const char* argv[] = {"exit_code", "7", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "exit_code";
        cfg.argv = std::span<const char* const>(argv, 2);

        auto sp = h.procs.spawn(cfg);
        check_true("exit-spawn", sp);
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("exit-wait", st);
        check_eq("exit-code", st.value().code, 7);
        h.unbind_env();
    }
} // namespace

export void run_posix_programs_smoke_tests() noexcept {
    log_line("[posix-smoke] programs begin");
    test_hello();
    test_argv_dump();
    test_stderr_demo();
    test_exit_code();
    log_line("[posix-smoke] programs end ok");
}

#endif
