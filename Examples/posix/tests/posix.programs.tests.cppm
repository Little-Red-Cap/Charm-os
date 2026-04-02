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
#include "../elf_samples/stderr_demo.elf.inc"
#include "../elf_samples/exit_code.elf.inc"
#include "../elf_samples/cat_file.elf.inc"
#include "../elf_samples/fd_probe.elf.inc"
#include "../elf_samples/stat_probe.elf.inc"

export module posix.programs.tests;

#if defined(POSIX_PROGRAMS_SMOKE_TEST) && POSIX_PROGRAMS_SMOKE_TEST

import posix.api;
import posix.fd_table;
import posix.file;
import posix.pipe;
import posix.proc;
import posix.errno;
import posix.program_image_elf;
import posix.program_image_modulex;
import module_core;
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
    using ProcServiceType = posix::ProcService<4, 4, 16, 16, 128, 16, 16, 256, 64 * 1024, 8192>;
    using ApiType = posix::Api<16, 8, 64, 4, 4, 16, 128, 16, 16, 256, 64 * 1024, 8192>;

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

    inline void on_process_enter(posix::ProcessId pid, void* ctx) noexcept {
        auto* api = static_cast<ApiType*>(ctx);
        if (api) {
            api->bind_process(pid);
        }
    }

    inline void on_process_exit(posix::ProcessId, void* ctx) noexcept {
        auto* api = static_cast<ApiType*>(ctx);
        if (api) {
            api->unbind_process();
        }
    }

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
        int value = 0;
        for (const char* p = argv[1]; p && *p; ++p) {
            if (*p < '0' || *p > '9') break;
            value = value * 10 + (*p - '0');
        }
        return value;
    }

    int modulex_entry_main(int, char**) {
        return write_text(1, "mx\n");
    }

    int elf_entry_main(int argc, char** argv, char**) {
        return modulex_entry_main(argc, argv);
    }

    bool resolve_modulex_entry(std::string_view name, modulex::Addr& out_addr) noexcept {
        if (name == "entry") {
            out_addr = modulex::to_addr(reinterpret_cast<const void*>(&modulex_entry_main));
            return true;
        }
        return false;
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

    int sh_main(int argc, char** argv) {
        if (!ProgramEnv::api) return 1;
        if (argc < 3 || !argv || !argv[1] || !argv[2]) return 2;
        if (std::string_view{argv[1]} != "-c") return 2;
        std::string_view cmd{argv[2]};
        if (cmd.rfind("echo ", 0) != 0 || cmd.size() <= 5) return 127;

        auto spawn_echo = [&](std::string_view arg, int stdio_out) noexcept -> int {
            char arg_buf[64]{};
            const auto n = arg.size() < (sizeof(arg_buf) - 1) ? arg.size() : (sizeof(arg_buf) - 1);
            for (util::usize i = 0; i < n; ++i) {
                arg_buf[i] = arg[i];
            }
            const char* echo_argv[] = {"echo", arg_buf, nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "echo";
            cfg.argv = std::span<const char* const>(echo_argv, 2);
            cfg.stdio_out = stdio_out;
            const int pid = ProgramEnv::api->spawn(cfg);
            if (pid < 0) return -1;
            int status = 0;
            const int wpid = ProgramEnv::api->waitpid(posix::ProcessId{pid}, &status, 0);
            if (wpid != pid) return -2;
            return (status >> 8) & 0xff;
        };

        const auto pipe_pos = cmd.find(" | cat");
        if (pipe_pos != std::string_view::npos) {
            const std::string_view arg = cmd.substr(5, pipe_pos - 5);
            int fds[2]{-1, -1};
            if (ProgramEnv::api->pipe(fds) != 0) return 5;
            char buf[68]{};
            const auto n = arg.size() < (sizeof(buf) - 2) ? arg.size() : (sizeof(buf) - 2);
            for (util::usize i = 0; i < n; ++i) {
                buf[i] = arg[i];
            }
            buf[n] = '\n';
            const auto w = ProgramEnv::api->write(fds[1], buf, static_cast<util::usize>(n + 1));
            (void)ProgramEnv::api->close(fds[1]);
            if (w < 0) return 6;
            auto r = ProgramEnv::api->read(fds[0], buf, sizeof(buf));
            if (r < 0) return 7;
            auto w2 = ProgramEnv::api->write(1, buf, static_cast<util::usize>(r));
            if (w2 < 0) return 8;
            return 0;
        }

        const auto redir_pos = cmd.find(" > ");
        if (redir_pos != std::string_view::npos) {
            const std::string_view arg = cmd.substr(5, redir_pos - 5);
            const std::string_view path = cmd.substr(redir_pos + 3);
            char path_buf[64]{};
            util::usize offset = 0;
            if (!path.empty() && path[0] != '/') {
                path_buf[0] = '/';
                offset = 1;
            }
            const auto pn = path.size() < (sizeof(path_buf) - 1 - offset)
                ? path.size()
                : (sizeof(path_buf) - 1 - offset);
            for (util::usize i = 0; i < pn; ++i) {
                path_buf[offset + i] = path[i];
            }
            const int fd = ProgramEnv::api->open(path_buf, posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
            if (fd < 0) return 9;
            char buf[68]{};
            const auto n = arg.size() < (sizeof(buf) - 2) ? arg.size() : (sizeof(buf) - 2);
            for (util::usize i = 0; i < n; ++i) {
                buf[i] = arg[i];
            }
            buf[n] = '\n';
            auto w = ProgramEnv::api->write(fd, buf, static_cast<util::usize>(n + 1));
            (void)ProgramEnv::api->close(fd);
            return w < 0 ? 10 : 0;
        }

        const std::string_view arg = cmd.substr(5);
        const int rc = spawn_echo(arg, -1);
        return rc < 0 ? 10 : rc;
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
        ProcServiceType procs{};
        ApiType api;

        Harness() : api(fds, files, pipes, procs) {
            fds.init();
            files.init();
            pipes.init();
            procs.init();
            procs.bind_fd_table(fds);
            procs.bind_process_hooks(&on_process_enter, &on_process_exit, &api);
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

    [[maybe_unused]] void test_exit_code() noexcept {
        Harness h{};
        h.bind_env();
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

        cfg_echo.stdio_out = p1[1];
        auto sp_echo = h.procs.spawn(cfg_echo);
        check_true("pipe-spawn-echo", sp_echo);
        (void)h.procs.waitpid(sp_echo.value().pid, 0);
        (void)h.api.close(p1[1]);

        const char* argv_cat[] = {"cat", nullptr};
        posix::SpawnConfig cfg_cat{};
        cfg_cat.path = "cat";
        cfg_cat.argv = std::span<const char* const>(argv_cat, 1);

        cfg_cat.stdio_in = p1[0];
        cfg_cat.stdio_out = p2[1];
        auto sp_cat = h.procs.spawn(cfg_cat);
        check_true("pipe-spawn-cat", sp_cat);
        (void)h.procs.waitpid(sp_cat.value().pid, 0);

        (void)h.api.close(p1[0]);
        (void)h.api.close(p2[1]);

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
        cfg_echo.stdio_out = p1[1];
        auto sp_echo = h.procs.spawn(cfg_echo);
        check_true("chain-spawn-echo", sp_echo);
        auto st_echo = h.procs.waitpid(sp_echo.value().pid, 0);
        check_true("chain-wait-echo", st_echo);
        check_eq("chain-exit-echo", st_echo.value().code, 0);
        (void)h.api.close(p1[1]);

        const char* argv_cat[] = {"cat", nullptr};
        posix::SpawnConfig cfg_cat1{};
        cfg_cat1.path = "cat";
        cfg_cat1.argv = std::span<const char* const>(argv_cat, 1);
        cfg_cat1.stdio_in = p1[0];
        cfg_cat1.stdio_out = p2[1];
        auto sp_cat1 = h.procs.spawn(cfg_cat1);
        check_true("chain-spawn-cat1", sp_cat1);
        auto st_cat1 = h.procs.waitpid(sp_cat1.value().pid, 0);
        check_true("chain-wait-cat1", st_cat1);
        check_eq("chain-exit-cat1", st_cat1.value().code, 0);
        (void)h.api.close(p1[0]);
        (void)h.api.close(p2[1]);

        int ofd = h.api.open("/out.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("chain-open-out", ofd >= 0);
        int out_fd = ofd;
        if (out_fd == 0 || out_fd == 1 || out_fd == 2) {
            check_true("chain-move-out", h.fds.dup2(out_fd, 7));
            out_fd = 7;
        }

        posix::SpawnConfig cfg_cat2{};
        cfg_cat2.path = "cat";
        cfg_cat2.argv = std::span<const char* const>(argv_cat, 1);
        cfg_cat2.stdio_in = p2[0];
        cfg_cat2.stdio_out = out_fd;
        auto sp_cat2 = h.procs.spawn(cfg_cat2);
        check_true("chain-spawn-cat2", sp_cat2);
        auto st_cat2 = h.procs.waitpid(sp_cat2.value().pid, 0);
        check_true("chain-wait-cat2", st_cat2);
        check_eq("chain-exit-cat2", st_cat2.value().code, 0);
        (void)h.api.close(ofd);
        if (out_fd != ofd) {
            (void)h.api.close(out_fd);
        }

        int rfd = h.api.open("/out.txt", posix::O_RDONLY, 0);
        check_true("chain-open-read", rfd >= 0);
        std::array<char, 16> buf{};
        auto r = h.api.read(rfd, buf.data(), buf.size());
        check_true("chain-read-len", r >= 3);
        check_eq("chain-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
        h.unbind_env();
    }

    void test_sh_c_echo() noexcept {
        Harness h{};
        h.bind_env();
        auto reg_echo = h.procs.register_executable("echo", &echo_main);
        check_true("sh-register-echo", reg_echo);
        auto reg_sh = h.procs.register_executable("sh", &sh_main);
        check_true("sh-register-sh", reg_sh);

        int pipefd[2]{-1, -1};
        check_eq("sh-pipe", h.api.pipe(pipefd), 0);
        int read_fd = pipefd[0];
        if (read_fd == 0 || read_fd == 1 || read_fd == 2) {
            check_true("sh-move-read", h.fds.dup2(read_fd, 7));
            read_fd = 7;
        }
        check_true("sh-dup2-out", h.fds.dup2(pipefd[1], 1));

        const char* argv[] = {"sh", "-c", "echo hi", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "sh";
        cfg.argv = std::span<const char* const>(argv, 3);

        auto sp = h.procs.spawn(cfg);
        check_true("sh-spawn", sp);
        (void)h.procs.waitpid(sp.value().pid, 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }

        std::array<char, 16> buf{};
        auto r = h.api.read(read_fd, buf.data(), buf.size());
        check_true("sh-read-len", r >= 3);
        check_eq("sh-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
        h.unbind_env();
    }

    void test_modulex_register() noexcept {
        Harness h{};
        h.bind_env();

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

        h.unbind_env();
    }

    void test_elf_prefix_stub() noexcept {
        Harness h{};
        h.bind_env();

        const char* argv[] = {"elf:/hello", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "elf:/hello";
        cfg.argv = std::span<const char* const>(argv, 1);
        auto sp = h.procs.spawn(cfg);
        check_true("elf-not-supported", !sp && sp.error() == util::Errc::not_supported);

        h.unbind_env();
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
        h.bind_env();
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
        h.unbind_env();
    }


    void test_elf_file_samples_probe() noexcept {
        fs::clear_mounts();
        static RamFsMount<256, 64, 512> ramfs{};
        new (&ramfs) RamFsMount<256, 64, 512>();
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("cat-file-mount", st);

        Harness h{};
        h.bind_env();
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
        const char cat_payload[] = "cat-data\n";
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

        posix::FdOps term_ops{};
        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &term_ops;
        check_true("cat-file-attach-term", h.fds.attach(term_entry, 1));

        {
            int err_pipe[2]{-1, -1};
            check_eq("cat-file-err-pipe", h.api.pipe(err_pipe), 0);
            int err_read = err_pipe[0];
            if (err_read == 2) {
                check_true("cat-file-err-move-read", h.fds.dup2(err_read, 7));
                err_read = 7;
            }
            check_true("cat-file-dup2-err", h.fds.dup2(err_pipe[1], 2));

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

            if (err_pipe[1] != 2) {
                (void)h.api.close(err_pipe[1]);
            }
            std::array<char, 32> err_buf{};
            util::usize err_size = 0;
            auto err = read_from_fd(h.api, err_read, err_buf, err_size);
            check_eq("cat-file-err", err, std::string_view{"A\nB\nC\nD\nE\nF\n"});
        }

        h.procs.enable_elf_hostcalls(false);
        h.procs.enable_elf_exec(false);
        h.unbind_env();
    }

    void test_elf_file_direct_exec() noexcept {
        check_true("elf-direct-mount", fs::mount_count() > 0);

        Harness h{};
        h.bind_env();
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
        h.unbind_env();
    }

    void test_elf_file_fd_probe() noexcept {
        fs::clear_mounts();
        static RamFsMount<256, 64, 512> ramfs{};
        new (&ramfs) RamFsMount<256, 64, 512>();
        auto mount_status = fs::add_mount("", ramfs.mount_point());
        check_true("fd-probe-mount", mount_status);

        int cat_fd = -1;
        {
            Harness h{};
            h.bind_env();
            h.procs.bind_file_service(h.files);

            cat_fd = h.api.open("/cat.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
            check_true("fd-probe-data-open", cat_fd >= 0);
            const char cat_payload[] = "cat-data\n";
            auto cat_write = h.api.write(cat_fd, cat_payload, sizeof(cat_payload) - 1);
            check_true("fd-probe-data-write", cat_write == static_cast<posix::ssize_t>(sizeof(cat_payload) - 1));
            check_eq("fd-probe-data-close", h.api.close(cat_fd), 0);
            h.unbind_env();
        }

        Harness h{};
        h.bind_env();
        h.procs.bind_file_service(h.files);
        h.procs.enable_elf_exec(true);
        h.procs.enable_elf_hostcalls(true);

        int fd_elf = h.api.open("/fd_probe.elf", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("fd-probe-elf-open", fd_elf >= 0);
        auto elf_write = h.api.write(fd_elf, fd_probe_elf, fd_probe_elf_len);
        check_true("fd-probe-elf-write", elf_write == static_cast<posix::ssize_t>(fd_probe_elf_len));
        check_eq("fd-probe-elf-close", h.api.close(fd_elf), 0);

        posix::PosixStat st{};
        (void)h.api.stat("/fd_probe.elf", &st);

        int fd_read = h.api.open("/fd_probe.elf", posix::O_RDONLY, 0);
        if (fd_read >= 0) {
            util::usize total = 0;
            std::array<char, 64> rbuf{};
            while (total < 8192) {
                auto r = h.api.read(fd_read, rbuf.data(), rbuf.size());
                if (r <= 0) break;
                total += static_cast<util::usize>(r);
            }
            (void)h.api.close(fd_read);
        }

        posix::FdOps term_ops{};
        posix::FdEntry term_entry{};
        term_entry.kind = posix::FdKind::term;
        term_entry.ops = &term_ops;
        check_true("fd-probe-attach-stdin", h.fds.attach(term_entry, 0));
        check_true("fd-probe-attach-stdout", h.fds.attach(term_entry, 1));
        check_true("fd-probe-attach-stderr", h.fds.attach(term_entry, 2));

        int out_pipe[2]{-1, -1};
        check_eq("fd-probe-out-pipe", h.api.pipe(out_pipe), 0);
        check_true("fd-probe-dup2-out", h.fds.dup2(out_pipe[1], 1));

        const char* argv[] = {"elf:/fd_probe.elf", "/cat.txt", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "elf:/fd_probe.elf";
        cfg.argv = std::span<const char* const>(argv, 2);
        auto img = h.procs.load_image(cfg);
        check_true("fd-probe-load", img);
        auto sp = h.procs.spawn(cfg);
        check_true("fd-probe-spawn", sp);
        auto st2 = h.procs.waitpid(sp.value().pid, 0);
        check_true("fd-probe-wait", st2);
        check_eq("fd-probe-code", st2.value().code, 0);

        if (out_pipe[1] != 1) {
            (void)h.api.close(out_pipe[1]);
        }
        std::array<char, 128> out_buf{};
        util::usize out_size = 0;
        while (out_size < out_buf.size()) {
            auto r = h.api.read(out_pipe[0], out_buf.data() + out_size, out_buf.size() - out_size);
            if (r <= 0) break;
            out_size += static_cast<util::usize>(r);
        }
        auto out = std::string_view{out_buf.data(), out_size};
        check_eq("fd-probe-out", out, std::string_view{
            "t0=1\n"
            "t1=0\n"
            "t2=1\n"
            "bt=0\n"
            "fs=0\n"
            "ft=0\n"
            "bs=-1\n"
        });

        (void)h.fds.dup2(2, 1);
        h.procs.enable_elf_hostcalls(false);
        h.procs.enable_elf_exec(false);
        h.unbind_env();
    }

    [[maybe_unused]] void test_elf_file_stat_probe() noexcept {
        static RamFsMount<256, 64, 512> ramfs{};
        if (fs::mount_count() == 0) {
            auto mount_status = fs::add_mount("", ramfs.mount_point());
            check_true("stat-probe-mount", mount_status);
        }

        int data_fd = -1;
        {
            Harness h{};
            h.bind_env();
            h.procs.bind_file_service(h.files);

            data_fd = h.api.open("/stat.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
            check_true("stat-probe-data-open", data_fd >= 0);
            const char payload[] = "stat-data\n";
            auto w = h.api.write(data_fd, payload, sizeof(payload) - 1);
            check_true("stat-probe-data-write", w == static_cast<posix::ssize_t>(sizeof(payload) - 1));
            check_eq("stat-probe-data-close", h.api.close(data_fd), 0);
            h.unbind_env();
        }

        Harness h{};
        h.bind_env();
        h.procs.bind_file_service(h.files);
        h.procs.enable_elf_exec(true);
        h.procs.enable_elf_hostcalls(true);

        int fd_elf = h.api.open("/stat_probe.elf", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("stat-probe-elf-open", fd_elf >= 0);
        auto elf_write = h.api.write(fd_elf, stat_probe_elf, stat_probe_elf_len);
        check_true("stat-probe-elf-write", elf_write == static_cast<posix::ssize_t>(stat_probe_elf_len));
        check_eq("stat-probe-elf-close", h.api.close(fd_elf), 0);

        posix::PosixStat st{};
        (void)h.api.stat("/stat_probe.elf", &st);

        int out_pipe[2]{-1, -1};
        check_eq("stat-probe-out-pipe", h.api.pipe(out_pipe), 0);
        check_true("stat-probe-dup2-out", h.fds.dup2(out_pipe[1], 1));

        const char* argv[] = {"elf:/stat_probe.elf", "/stat.txt", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "elf:/stat_probe.elf";
        cfg.argv = std::span<const char* const>(argv, 2);
        auto img = h.procs.load_image(cfg);
        check_true("stat-probe-load", img);
        auto sp = h.procs.spawn(cfg);
        check_true("stat-probe-spawn", sp);
        auto st2 = h.procs.waitpid(sp.value().pid, 0);
        check_true("stat-probe-wait", st2);
        check_eq("stat-probe-code", st2.value().code, 0);

        if (out_pipe[1] != 1) {
            (void)h.api.close(out_pipe[1]);
        }
        std::array<char, 64> out_buf{};
        util::usize out_size = 0;
        while (out_size < out_buf.size()) {
            auto r = h.api.read(out_pipe[0], out_buf.data() + out_size, out_buf.size() - out_size);
            if (r <= 0) break;
            out_size += static_cast<util::usize>(r);
        }
        auto out = std::string_view{out_buf.data(), out_size};
        check_eq("stat-probe-out", out, std::string_view{"rc=0\nsz=10\nbs=-1\n"});

        h.procs.enable_elf_hostcalls(false);
        h.procs.enable_elf_exec(false);
        h.unbind_env();
    }
    void test_elf_real_samples() noexcept {
        Harness h{};
        h.bind_env();
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
        check_true("elf-real-reg-stderr",
            h.procs.register_elf_mem("stderr_demo", stderr_demo_elf, stderr_demo_elf_len));
        check_true("elf-real-reg-exit",
            h.procs.register_elf_mem("exit_code", exit_code_elf, exit_code_elf_len));

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
            int pipefd[2]{-1, -1};
            check_eq("elf-real-env-pipe", h.api.pipe(pipefd), 0);
            check_true("elf-real-env-dup2", h.fds.dup2(pipefd[1], 1));
            const char* argv[] = {"elfmem:argv_dump", "env", nullptr};
            const char* envp[] = {"PATH=/bin:/usr/bin", "FOO=BAR", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:argv_dump";
            cfg.argv = std::span<const char* const>(argv, 2);
            cfg.envp = std::span<const char* const>(envp, 2);
            auto sp = spawn_checked("elf-real-env-spawn", cfg);
            auto st = h.procs.waitpid(sp.pid, 0);
            check_true("elf-real-env-wait", st);
            check_eq("elf-real-env-code", st.value().code, 0);
            if (pipefd[1] != 1) (void)h.api.close(pipefd[1]);
            std::array<char, 96> buf{};
            util::usize out_size = 0;
            auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
            const char expected[] = "argv[0]=elfmem:argv_dump\nargv[1]=env\n";
            check_eq("elf-real-env-out", out, std::string_view{expected});
        }

        {
            int out_pipe[2]{-1, -1};
            int err_pipe[2]{-1, -1};
            check_eq("elf-real-stderr-out-pipe", h.api.pipe(out_pipe), 0);
            check_eq("elf-real-stderr-err-pipe", h.api.pipe(err_pipe), 0);
            int err_read = err_pipe[0];
            if (err_read == 2) {
                check_true("elf-real-stderr-move-read", h.fds.dup2(err_read, 6));
                err_read = 6;
            }
            check_true("elf-real-stderr-dup2-out", h.fds.dup2(out_pipe[1], 1));
            check_true("elf-real-stderr-dup2-err", h.fds.dup2(err_pipe[1], 2));
            const char* argv[] = {"elfmem:stderr_demo", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:stderr_demo";
            cfg.argv = std::span<const char* const>(argv, 1);
            auto sp = spawn_checked("elf-real-stderr-spawn", cfg);
            auto st = h.procs.waitpid(sp.pid, 0);
            check_true("elf-real-stderr-wait", st);
            check_eq("elf-real-stderr-code", st.value().code, 0);
            std::array<char, 16> out_buf{};
            std::array<char, 16> err_buf{};
            util::usize out_size = 0;
            util::usize err_size = 0;
            auto out = read_from_fd(h.api, out_pipe[0], out_buf, out_size);
            auto err = read_from_fd(h.api, err_read, err_buf, err_size);
            check_eq("elf-real-stderr-out", out, std::string_view{"out\n"});
            check_eq("elf-real-stderr-err", err, std::string_view{"err\n"});
        }

        {
            int pipefd[2]{-1, -1};
            check_eq("elf-real-stderr-merge-pipe", h.api.pipe(pipefd), 0);
            const char* argv[] = {"elfmem:stderr_demo", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:stderr_demo";
            cfg.argv = std::span<const char* const>(argv, 1);
            cfg.stdio_out = pipefd[1];
            cfg.stdio_err = pipefd[1];
            auto sp = spawn_checked("elf-real-stderr-merge-spawn", cfg);
            auto st = h.procs.waitpid(sp.pid, 0);
            check_true("elf-real-stderr-merge-wait", st);
            check_eq("elf-real-stderr-merge-code", st.value().code, 0);
            (void)h.api.close(pipefd[1]);
            std::array<char, 16> buf{};
            util::usize out_size = 0;
            auto out = read_from_fd(h.api, pipefd[0], buf, out_size);
            check_eq("elf-real-stderr-merge-out", out, std::string_view{"out\nerr\n"});
        }

        {
            int err_pipe[2]{-1, -1};
            check_eq("elf-real-stderr-only-pipe", h.api.pipe(err_pipe), 0);
            int err_read = err_pipe[0];
            if (err_read == 2) {
                check_true("elf-real-stderr-only-move-read", h.fds.dup2(err_read, 7));
                err_read = 7;
            }
            const char* argv[] = {"elfmem:stderr_demo", nullptr};
            posix::SpawnConfig cfg{};
            cfg.path = "elfmem:stderr_demo";
            cfg.argv = std::span<const char* const>(argv, 1);
            cfg.stdio_err = err_pipe[1];
            auto sp = spawn_checked("elf-real-stderr-only-spawn", cfg);
            auto st = h.procs.waitpid(sp.pid, 0);
            check_true("elf-real-stderr-only-wait", st);
            check_eq("elf-real-stderr-only-code", st.value().code, 0);
            (void)h.api.close(err_pipe[1]);
            std::array<char, 16> buf{};
            util::usize out_size = 0;
            auto out = read_from_fd(h.api, err_read, buf, out_size);
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

        h.procs.enable_elf_hostcalls(false);
        h.procs.enable_elf_exec(false);
        h.unbind_env();
    }

    void test_sh_c_redir_and_pipe() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("sh2-mount", st);

        Harness h{};
        h.bind_env();
        auto reg_echo = h.procs.register_executable("echo", &echo_main);
        check_true("sh2-register-echo", reg_echo);
        auto reg_cat = h.procs.register_executable("cat", &cat_main);
        check_true("sh2-register-cat", reg_cat);
        auto reg_sh = h.procs.register_executable("sh", &sh_main);
        check_true("sh2-register-sh", reg_sh);

        const char* argv_redir[] = {"sh", "-c", "echo hi > out.txt", nullptr};
        posix::SpawnConfig cfg_redir{};
        cfg_redir.path = "sh";
        cfg_redir.argv = std::span<const char* const>(argv_redir, 3);
        auto sp1 = h.procs.spawn(cfg_redir);
        check_true("sh2-spawn-redir", sp1);
        (void)h.procs.waitpid(sp1.value().pid, 0);

        int rfd = h.api.open("/out.txt", posix::O_RDONLY, 0);
        check_true("sh2-open-read", rfd >= 0);
        std::array<char, 16> buf{};
        auto r = h.api.read(rfd, buf.data(), buf.size());
        check_true("sh2-read-len", r >= 3);
        check_eq("sh2-read-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});

        int pipefd[2]{-1, -1};
        check_eq("sh2-pipe", h.api.pipe(pipefd), 0);
        int read_fd = pipefd[0];
        if (read_fd == 0 || read_fd == 1 || read_fd == 2) {
            check_true("sh2-move-read", h.fds.dup2(read_fd, 8));
            read_fd = 8;
        }
        check_true("sh2-dup2-out", h.fds.dup2(pipefd[1], 1));

        const char* argv_pipe[] = {"sh", "-c", "echo hi | cat", nullptr};
        posix::SpawnConfig cfg_pipe{};
        cfg_pipe.path = "sh";
        cfg_pipe.argv = std::span<const char* const>(argv_pipe, 3);
        auto sp2 = h.procs.spawn(cfg_pipe);
        check_true("sh2-spawn-pipe", sp2);
        (void)h.procs.waitpid(sp2.value().pid, 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }
        auto r2 = h.api.read(read_fd, buf.data(), buf.size());
        check_true("sh2-read2-len", r2 >= 3);
        check_eq("sh2-read2-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
        h.unbind_env();
    }
} // namespace

export void run_posix_programs_smoke_tests() noexcept {
    log_line("[posix-smoke] programs begin");
    test_hello();
    test_argv_dump();
    test_exit_code();
    test_stderr_demo();
    test_modulex_register();
    test_elf_prefix_stub();
    test_elf_header_stub();
    test_elf_file_spawn();
    test_elf_file_samples_probe();
    test_elf_file_direct_exec();
    test_elf_file_fd_probe();
    test_elf_file_stat_probe();
    test_elf_real_samples();
    test_echo_to_file();
    test_cat_from_file();
    test_echo_pipe_cat();
    test_echo_pipe_cat_chain();
    test_sh_c_echo();
    test_sh_c_redir_and_pipe();
    log_line("[posix-smoke] programs end ok");
}

#endif
