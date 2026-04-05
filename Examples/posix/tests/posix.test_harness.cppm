//
// Shared POSIX program smoke test harness and host-side sample executables.
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

export module posix.test_harness;

#if defined(POSIX_PROGRAMS_SMOKE_TEST) && POSIX_PROGRAMS_SMOKE_TEST

export import posix.api;
export import posix.fd_table;
export import posix.file;
export import posix.pipe;
export import posix.proc;
export import posix.errno;
export import posix.program_image_elf;
export import posix.program_image_modulex;
export import module_core;
export import fs_core;
export import fs_errno;
export import fs_ramfs;
export import fs_stream;
export import fs_vfs;
export import util.core;
export import util.error;

export namespace posix::testsupport {
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

}

#endif
