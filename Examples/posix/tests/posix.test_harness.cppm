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

    inline constexpr char kProgramPathEntry[] = "PATH=/bin:/usr/bin";
    inline constexpr std::array<const char*, 1> kProgramPathEnv{kProgramPathEntry};

    inline std::span<const char* const> program_path_env() noexcept {
        return std::span<const char* const>(kProgramPathEnv.data(), kProgramPathEnv.size());
    }

    inline void on_process_enter(posix::ProcessId pid, void* ctx) noexcept {
        auto* api = static_cast<ApiType*>(ctx);
        if (api) {
            api->push_process(pid);
        }
    }

    inline void on_process_exit(posix::ProcessId, void* ctx) noexcept {
        auto* api = static_cast<ApiType*>(ctx);
        if (api) {
            api->pop_process();
        }
    }

    inline int write_text(int fd, std::string_view text) noexcept {
        if (!ProgramEnv::api) return 1;
        auto w = ProgramEnv::api->write(fd, text.data(), text.size());
        if (w != static_cast<posix::ssize_t>(text.size())) return 2;
        return 0;
    }

    template <util::usize N>
    bool make_absolute_path(std::string_view in, std::array<char, N>& out) noexcept {
        if (in.empty() || N < 2) return false;
        util::usize pos = 0;
        if (in[0] != '/') {
            out[pos++] = '/';
        }
        if (pos + in.size() >= out.size()) {
            return false;
        }
        for (char ch : in) {
            out[pos++] = ch;
        }
        out[pos] = '\0';
        return true;
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
        const auto path_env = program_path_env();

        auto spawn_command = [&](const char* file,
                                 std::span<const char* const> child_argv,
                                 int stdio_in,
                                 int stdio_out) noexcept -> int {
            posix::SpawnConfig cfg{};
            cfg.path = file;
            cfg.argv = child_argv;
            cfg.envp = path_env;
            cfg.stdio_in = stdio_in;
            cfg.stdio_out = stdio_out;
            const int pid = ProgramEnv::api->spawnp(cfg);
            if (pid < 0) return posix::get_errno();
            int status = 0;
            const int wpid = ProgramEnv::api->waitpid(posix::ProcessId{pid}, &status, 0);
            if (wpid != pid) return 127;
            return (status >> 8) & 0xff;
        };

        auto spawn_echo = [&](std::string_view arg, int stdio_out) noexcept -> int {
            char arg_buf[64]{};
            const auto n = arg.size() < (sizeof(arg_buf) - 1) ? arg.size() : (sizeof(arg_buf) - 1);
            for (util::usize i = 0; i < n; ++i) {
                arg_buf[i] = arg[i];
            }
            const char* echo_argv[] = {"echo", arg_buf, nullptr};
            return spawn_command("echo", std::span<const char* const>(echo_argv, 2), -1, stdio_out);
        };

        auto spawn_cat = [&](int stdio_in, int stdio_out) noexcept -> int {
            const char* cat_argv[] = {"cat", nullptr};
            return spawn_command("cat", std::span<const char* const>(cat_argv, 1), stdio_in, stdio_out);
        };

        const auto pipe_pos = cmd.find(" | cat");
        if (pipe_pos != std::string_view::npos) {
            const std::string_view arg = cmd.substr(5, pipe_pos - 5);
            int fds[2]{-1, -1};
            if (ProgramEnv::api->pipe(fds) != 0) return 5;
            const int echo_rc = spawn_echo(arg, fds[1]);
            (void)ProgramEnv::api->close(fds[1]);
            if (echo_rc != 0) {
                (void)ProgramEnv::api->close(fds[0]);
                return echo_rc;
            }
            const int cat_rc = spawn_cat(fds[0], -1);
            (void)ProgramEnv::api->close(fds[0]);
            return cat_rc;
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
            const int rc = spawn_echo(arg, fd);
            (void)ProgramEnv::api->close(fd);
            return rc;
        }

        const std::string_view arg = cmd.substr(5);
        const int rc = spawn_echo(arg, -1);
        return rc;
    }

    int mkdir_main(int argc, char** argv) {
        if (!ProgramEnv::api) return 1;
        if (argc < 2 || !argv || !argv[1]) return 2;
        std::array<char, 64> path_buf{};
        if (!make_absolute_path(std::string_view{argv[1]}, path_buf)) return 3;
        return ProgramEnv::api->mkdir(path_buf.data()) == 0 ? 0 : 4;
    }

    int rm_main(int argc, char** argv) {
        if (!ProgramEnv::api) return 1;
        if (argc < 2 || !argv || !argv[1]) return 2;
        std::array<char, 64> path_buf{};
        if (!make_absolute_path(std::string_view{argv[1]}, path_buf)) return 3;
        return ProgramEnv::api->unlink(path_buf.data()) == 0 ? 0 : 4;
    }

    int mv_main(int argc, char** argv) {
        if (!ProgramEnv::api) return 1;
        if (argc < 3 || !argv || !argv[1] || !argv[2]) return 2;
        std::array<char, 64> from_buf{};
        std::array<char, 64> to_buf{};
        if (!make_absolute_path(std::string_view{argv[1]}, from_buf)) return 3;
        if (!make_absolute_path(std::string_view{argv[2]}, to_buf)) return 3;
        return ProgramEnv::api->rename(from_buf.data(), to_buf.data()) == 0 ? 0 : 4;
    }

    int ls_main(int argc, char** argv) {
        if (!ProgramEnv::api) return 1;
        std::array<char, 64> path_buf{};
        const char* path = "/";
        if (argc > 1 && argv && argv[1]) {
            if (!make_absolute_path(std::string_view{argv[1]}, path_buf)) return 2;
            path = path_buf.data();
        }
        auto* dir = ProgramEnv::api->opendir(path);
        if (!dir) return 3;
        posix::set_errno(0);
        while (const auto* ent = ProgramEnv::api->readdir(dir)) {
            const std::string_view name{ent->d_name.data()};
            if (write_text(1, name) != 0) {
                (void)ProgramEnv::api->closedir(dir);
                return 4;
            }
            if (write_text(1, "\n") != 0) {
                (void)ProgramEnv::api->closedir(dir);
                return 4;
            }
        }
        const int err = posix::get_errno();
        if (ProgramEnv::api->closedir(dir) != 0) return 5;
        return err == 0 ? 0 : 6;
    }

    inline std::string_view command_basename(std::string_view name) noexcept {
        const auto pos = name.find_last_of('/');
        return pos == std::string_view::npos ? name : name.substr(pos + 1);
    }

    int busybox_main(int argc, char** argv) {
        if (argc < 1 || !argv || !argv[0]) return 127;

        auto dispatch_applet = [&](std::string_view applet_name,
                                   int applet_argc,
                                   char** applet_argv) noexcept -> int {
            if (applet_name == "echo") {
                return echo_main(applet_argc, applet_argv);
            }
            if (applet_name == "cat") {
                return cat_main(applet_argc, applet_argv);
            }
            if (applet_name == "sh") {
                return sh_main(applet_argc, applet_argv);
            }
            if (applet_name == "mkdir") {
                return mkdir_main(applet_argc, applet_argv);
            }
            if (applet_name == "rm") {
                return rm_main(applet_argc, applet_argv);
            }
            if (applet_name == "mv") {
                return mv_main(applet_argc, applet_argv);
            }
            if (applet_name == "ls") {
                return ls_main(applet_argc, applet_argv);
            }
            return 127;
        };

        const auto argv0_name = command_basename(std::string_view{argv[0]});
        if (argv0_name != "busybox") {
            return dispatch_applet(argv0_name, argc, argv);
        }

        if (argc < 2 || !argv[1]) {
            return 127;
        }

        std::array<char*, 16> shifted_argv{};
        shifted_argv[0] = argv[1];
        int shifted_argc = 1;
        for (int src = 2; src < argc && shifted_argc < static_cast<int>(shifted_argv.size()) - 1; ++src) {
            shifted_argv[shifted_argc++] = argv[src];
        }
        shifted_argv[shifted_argc] = nullptr;

        const auto applet_name = command_basename(std::string_view{argv[1]});
        return dispatch_applet(applet_name, shifted_argc, shifted_argv.data());
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
            procs.bind_file_service(files);
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
