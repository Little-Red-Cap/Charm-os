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
import fs_core;
import fs_errno;
import fs_ramfs;
import fs_stream;
import fs_vfs;
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

    int echo_main(int argc, char** argv) {
        if (!ProgramEnv::api) return 1;
        if (argc > 1 && argv && argv[1]) {
            if (write_text(1, std::string_view{argv[1]}) != 0) return 2;
        }
        return write_text(1, "\n");
    }

    int cat_main(int, char**) {
        if (!ProgramEnv::api) return 1;
        std::array<char, 16> buf{};
        while (true) {
            auto r = ProgramEnv::api->read(0, buf.data(), buf.size());
            if (r == 0) break;
            if (r < 0) return 2;
            auto w = ProgramEnv::api->write(1, buf.data(), static_cast<util::usize>(r));
            if (w != r) return 3;
        }
        return 0;
    }

    template <util::usize BlockSize, util::usize MaxFiles, util::usize MaxBlocks>
    struct RamFsMount {
        fs::RamFs<BlockSize, MaxFiles, MaxBlocks> fs{};
        fs::Mount mount{};

        RamFsMount() noexcept {
            mount.ops = &ops_;
            mount.data = this;
        }

        fs::Mount* mount_point() noexcept { return &mount; }

        static fs::Status open_impl(fs::Mount* m, std::string_view path, fs::File& out, fs::OpenFlags flags) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.open(path, out, flags);
        }

        static fs::Status mkdir_impl(fs::Mount* m, std::string_view path) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.mkdir(path);
        }

        static fs::Status unlink_impl(fs::Mount* m, std::string_view path) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.unlink(path);
        }

        static fs::Status truncate_impl(fs::Mount* m, std::string_view path, util::u64 size) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.truncate(path, size);
        }

        static fs::Status rename_impl(fs::Mount* m, std::string_view from, std::string_view to) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.rename(from, to);
        }

        static fs::Status list_impl(fs::Mount* m, std::string_view path, void* ctx, fs::MountOps::ListFn fn) noexcept {
            auto* self = static_cast<RamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.list(path, ctx, fn);
        }

        static fs::MountOps ops_;
    };

    template <util::usize BlockSize, util::usize MaxFiles, util::usize MaxBlocks>
    fs::MountOps RamFsMount<BlockSize, MaxFiles, MaxBlocks>::ops_{
        &RamFsMount::open_impl,
        nullptr,
        nullptr,
        &RamFsMount::unlink_impl,
        &RamFsMount::rename_impl,
        &RamFsMount::truncate_impl,
        &RamFsMount::mkdir_impl,
        &RamFsMount::list_impl
    };

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

    void test_echo_to_file() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("echo-mount", st);

        Harness h{};
        h.bind_env();
        auto rreg = h.procs.register_executable("echo", &echo_main);
        check_true("echo-register", rreg);

        int fd = h.api.open("/out.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("echo-open", fd >= 0);
        check_true("echo-dup2", h.fds.dup2(fd, 1));

        const char* argv[] = {"echo", "hi", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "echo";
        cfg.argv = std::span<const char* const>(argv, 2);

        auto sp = h.procs.spawn(cfg);
        check_true("echo-spawn", sp);
        auto w = h.procs.waitpid(sp.value().pid, 0);
        check_true("echo-wait", w);

        check_eq("echo-close", h.api.close(fd), 0);

        int rfd = h.api.open("/out.txt", posix::O_RDONLY, 0);
        check_true("echo-read-open", rfd >= 0);
        std::array<char, 16> buf{};
        auto r = h.api.read(rfd, buf.data(), buf.size());
        check_eq("echo-read", r, 3);
        check_eq("echo-read-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
        (void)h.api.close(rfd);
        h.unbind_env();
    }

    void test_cat_from_file() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("cat-mount", st);

        Harness h{};
        h.bind_env();
        auto rreg = h.procs.register_executable("cat", &cat_main);
        check_true("cat-register", rreg);

        int wfd = h.api.open("/in.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("cat-open", wfd >= 0);
        check_eq("cat-write", h.api.write(wfd, "ok\n", 3), 3);
        (void)h.api.close(wfd);

        int rfd = h.api.open("/in.txt", posix::O_RDONLY, 0);
        check_true("cat-open-r", rfd >= 0);
        int ofd = h.api.open("/out.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("cat-open-out", ofd >= 0);
        check_true("cat-dup2-in", h.fds.dup2(rfd, 0));
        check_true("cat-dup2-out", h.fds.dup2(ofd, 1));

        const char* argv[] = {"cat", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "cat";
        cfg.argv = std::span<const char* const>(argv, 1);

        auto sp = h.procs.spawn(cfg);
        check_true("cat-spawn", sp);
        (void)h.procs.waitpid(sp.value().pid, 0);
        std::array<char, 16> buf{};
        int rfd2 = h.api.open("/out.txt", posix::O_RDONLY, 0);
        check_true("cat-read-open", rfd2 >= 0);
        auto r = h.api.read(rfd2, buf.data(), buf.size());
        check_true("cat-read-len", r >= 3);
        check_eq("cat-text", std::string_view{buf.data(), 3}, std::string_view{"ok\n"});
        h.unbind_env();
    }

    void test_echo_pipe_cat() noexcept {
        Harness h{};
        h.bind_env();
        auto reg_echo = h.procs.register_executable("echo", &echo_main);
        check_true("pipe-register-echo", reg_echo);
        auto reg_cat = h.procs.register_executable("cat", &cat_main);
        check_true("pipe-register-cat", reg_cat);

        int p1[2]{-1, -1};
        int p2[2]{-1, -1};
        check_eq("pipe-p1", h.api.pipe(p1), 0);
        check_eq("pipe-p2", h.api.pipe(p2), 0);

        const char* argv_echo[] = {"echo", "hi", nullptr};
        posix::SpawnConfig cfg_echo{};
        cfg_echo.path = "echo";
        cfg_echo.argv = std::span<const char* const>(argv_echo, 2);

        check_true("pipe-dup2-echo", h.fds.dup2(p1[1], 1));
        auto sp_echo = h.procs.spawn(cfg_echo);
        check_true("pipe-spawn-echo", sp_echo);
        (void)h.procs.waitpid(sp_echo.value().pid, 0);
        if (p1[1] != 1) {
            (void)h.api.close(p1[1]);
        }

        const char* argv_cat[] = {"cat", nullptr};
        posix::SpawnConfig cfg_cat{};
        cfg_cat.path = "cat";
        cfg_cat.argv = std::span<const char* const>(argv_cat, 1);

        check_true("pipe-dup2-cat-in", h.fds.dup2(p1[0], 0));
        check_true("pipe-dup2-cat-out", h.fds.dup2(p2[1], 1));
        auto sp_cat = h.procs.spawn(cfg_cat);
        check_true("pipe-spawn-cat", sp_cat);
        (void)h.procs.waitpid(sp_cat.value().pid, 0);

        if (p1[0] != 0) {
            (void)h.api.close(p1[0]);
        }
        if (p2[1] != 1) {
            (void)h.api.close(p2[1]);
        }

        std::array<char, 16> buf{};
        int p2_read = p2[0];
        if (p2_read == 0 || p2_read == 1 || p2_read == 2) {
            check_true("pipe-move-read", h.fds.dup2(p2_read, 7));
            p2_read = 7;
        }
        auto r = h.api.read(p2_read, buf.data(), buf.size());
        check_true("pipe-read-len", r >= 3);
        check_eq("pipe-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
        h.unbind_env();
    }

    void test_echo_pipe_cat_chain() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("chain-mount", st);

        Harness h{};
        h.bind_env();
        auto reg_echo = h.procs.register_executable("echo", &echo_main);
        check_true("chain-register-echo", reg_echo);
        auto reg_cat = h.procs.register_executable("cat", &cat_main);
        check_true("chain-register-cat", reg_cat);

        int p1[2]{-1, -1};
        int p2[2]{-1, -1};
        check_eq("chain-p1", h.api.pipe(p1), 0);
        check_eq("chain-p2", h.api.pipe(p2), 0);

        const char* argv_echo[] = {"echo", "hi", nullptr};
        posix::SpawnConfig cfg_echo{};
        cfg_echo.path = "echo";
        cfg_echo.argv = std::span<const char* const>(argv_echo, 2);
        check_true("chain-dup2-echo", h.fds.dup2(p1[1], 1));
        auto sp_echo = h.procs.spawn(cfg_echo);
        check_true("chain-spawn-echo", sp_echo);
        (void)h.procs.waitpid(sp_echo.value().pid, 0);
        if (p1[1] != 1) {
            (void)h.api.close(p1[1]);
        }

        const char* argv_cat[] = {"cat", nullptr};
        posix::SpawnConfig cfg_cat1{};
        cfg_cat1.path = "cat";
        cfg_cat1.argv = std::span<const char* const>(argv_cat, 1);
        check_true("chain-dup2-cat1-in", h.fds.dup2(p1[0], 0));
        check_true("chain-dup2-cat1-out", h.fds.dup2(p2[1], 1));
        auto sp_cat1 = h.procs.spawn(cfg_cat1);
        check_true("chain-spawn-cat1", sp_cat1);
        (void)h.procs.waitpid(sp_cat1.value().pid, 0);
        if (p1[0] != 0) {
            (void)h.api.close(p1[0]);
        }
        if (p2[1] != 1) {
            (void)h.api.close(p2[1]);
        }

        int ofd = h.api.open("/out.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("chain-open-out", ofd >= 0);

        posix::SpawnConfig cfg_cat2{};
        cfg_cat2.path = "cat";
        cfg_cat2.argv = std::span<const char* const>(argv_cat, 1);
        check_true("chain-dup2-cat2-in", h.fds.dup2(p2[0], 0));
        check_true("chain-dup2-cat2-out", h.fds.dup2(ofd, 1));
        auto sp_cat2 = h.procs.spawn(cfg_cat2);
        check_true("chain-spawn-cat2", sp_cat2);
        (void)h.procs.waitpid(sp_cat2.value().pid, 0);
        (void)h.api.close(ofd);

        int rfd = h.api.open("/out.txt", posix::O_RDONLY, 0);
        check_true("chain-open-read", rfd >= 0);
        std::array<char, 16> buf{};
        auto r = h.api.read(rfd, buf.data(), buf.size());
        check_true("chain-read-len", r >= 3);
        check_eq("chain-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
        h.unbind_env();
    }
} // namespace

export void run_posix_programs_smoke_tests() noexcept {
    log_line("[posix-smoke] programs begin");
    test_hello();
    test_argv_dump();
    test_stderr_demo();
    test_exit_code();
    test_echo_to_file();
    test_cat_from_file();
    test_echo_pipe_cat();
    test_echo_pipe_cat_chain();
    log_line("[posix-smoke] programs end ok");
}

#endif
