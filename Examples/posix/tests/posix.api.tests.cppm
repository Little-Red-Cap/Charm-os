//
// Minimal smoke tests for posix.api (no framework).
//

module;
#include <array>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string_view>
#include <type_traits>

export module posix.api.tests;

#if defined(POSIX_API_SMOKE_TEST) && POSIX_API_SMOKE_TEST

import posix.api;
import posix.errno;
import posix.file;
import posix.pipe;
import posix.proc;
import posix.fd_table;
import charm.system.clock;
import charm.system.time;
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
        std::snprintf(buf, sizeof(buf), "[posix-smoke] api %s %s", label, ok ? "ok" : "fail");
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
                    "[posix-smoke] api %s fail: expected=\"%.*s\" actual=\"%.*s\"",
                    label,
                    static_cast<int>(b.size()), b.data(),
                    static_cast<int>(a.size()), a.data());
            } else {
                std::snprintf(buf, sizeof(buf),
                    "[posix-smoke] api %s fail: expected=%lld actual=%lld",
                    label, to_ll(b), to_ll(a));
            }
            log_line(buf);
            fail();
        }
        log_step(label, true);
    }

    fs::Status dummy_node_read(fs::Node&, std::span<util::u8>) noexcept { return fs::Status{fs::Errc::ok}; }
    fs::Status dummy_node_write(fs::Node&, std::span<const util::u8>) noexcept { return fs::Status{fs::Errc::ok}; }
    fs::Status dummy_node_seek(fs::Node&, util::i64) noexcept { return fs::Status{fs::Errc::ok}; }
    fs::Status dummy_node_flush(fs::Node&) noexcept { return fs::Status{fs::Errc::ok}; }
    fs::Status dummy_node_close(fs::Node&) noexcept { return fs::Status{fs::Errc::ok}; }

    fs::NodeOps dummy_node_ops{
        &dummy_node_read,
        &dummy_node_write,
        &dummy_node_seek,
        &dummy_node_flush,
        &dummy_node_close
    };

    fs::Status dummy_mount_open(fs::Mount*, std::string_view, fs::File& out, fs::OpenFlags) noexcept {
        out.node.type = fs::NodeType::file;
        out.node.ops = &dummy_node_ops;
        out.node.data = nullptr;
        out.node.size = 0;
        out.node.offset = 0;
        return fs::Status{fs::Errc::ok};
    }

    fs::MountOps dummy_mount_ops{
        &dummy_mount_open,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    };

    template <util::usize BlockSize, util::usize MaxFiles, util::usize MaxBlocks>
    struct ApiRamFsMount {
        fs::RamFs<BlockSize, MaxFiles, MaxBlocks> fs{};
        fs::Mount mount{};

        ApiRamFsMount() noexcept {
            mount.ops = &ops_;
            mount.data = this;
        }

        fs::Mount* mount_point() noexcept { return &mount; }

        static fs::Status open_impl(fs::Mount* m, std::string_view path, fs::File& out, fs::OpenFlags flags) noexcept {
            auto* self = static_cast<ApiRamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.open(path, out, flags);
        }

        static fs::Status mkdir_impl(fs::Mount* m, std::string_view path) noexcept {
            auto* self = static_cast<ApiRamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.mkdir(path);
        }

        static fs::Status unlink_impl(fs::Mount* m, std::string_view path) noexcept {
            auto* self = static_cast<ApiRamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.unlink(path);
        }

        static fs::Status truncate_impl(fs::Mount* m, std::string_view path, util::u64 size) noexcept {
            auto* self = static_cast<ApiRamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.truncate(path, size);
        }

        static fs::Status rename_impl(fs::Mount* m, std::string_view from, std::string_view to) noexcept {
            auto* self = static_cast<ApiRamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.rename(from, to);
        }

        static fs::Status list_impl(fs::Mount* m, std::string_view path, void* ctx, fs::MountOps::ListFn fn) noexcept {
            auto* self = static_cast<ApiRamFsMount*>(m ? m->data : nullptr);
            if (!self) return fs::Status{fs::Errc::inval};
            return self->fs.list(path, ctx, fn);
        }

        static fs::MountOps ops_;
    };

    template <util::usize BlockSize, util::usize MaxFiles, util::usize MaxBlocks>
    fs::MountOps ApiRamFsMount<BlockSize, MaxFiles, MaxBlocks>::ops_{
        &ApiRamFsMount::open_impl,
        nullptr,
        nullptr,
        &ApiRamFsMount::unlink_impl,
        &ApiRamFsMount::rename_impl,
        &ApiRamFsMount::truncate_impl,
        &ApiRamFsMount::mkdir_impl,
        &ApiRamFsMount::list_impl
    };

    int demo_main(int argc, char** argv) {
        if (argc < 1 || argv == nullptr) return 7;
        return 3;
    }

    void test_api_open_close() noexcept {
        fs::clear_mounts();
        fs::Mount mount{};
        mount.ops = &dummy_mount_ops;
        mount.data = nullptr;
        auto st = fs::add_mount("", &mount);
        check_true("open-mount", st);

        posix::FdTable<8> fds{};
        posix::FileService<4> files{};
        posix::PipeService<2, 8> pipes{};
        posix::ProcService<4, 4, 8, 4> procs{};
        fds.init();
        files.init();
        pipes.init();
        procs.init();

        posix::Api<8, 2, 8, 4, 4, 4> api{fds, files, pipes, procs};
        int fd = api.open("/tmp/x", posix::O_WRONLY | posix::O_CREAT, 0);
        check_true("open-basic", fd >= 0);
        check_eq("close-basic", api.close(fd), 0);
    }

    void test_api_pipe_rw() noexcept {
        posix::FdTable<8> fds{};
        posix::FileService<4> files{};
        posix::PipeService<2, 8> pipes{};
        posix::ProcService<4, 4, 8, 4> procs{};
        fds.init();
        files.init();
        pipes.init();
        procs.init();

        posix::Api<8, 2, 8, 4, 4, 4> api{fds, files, pipes, procs};
        int fds_arr[2]{-1, -1};
        check_eq("pipe-create", api.pipe(fds_arr), 0);
        const char msg[] = "hi";
        auto w = api.write(fds_arr[1], msg, 2);
        check_eq("pipe-write", w, 2);
        char out[2]{};
        auto r = api.read(fds_arr[0], out, 2);
        check_eq("pipe-read", r, 2);
    }

    void test_api_spawn_wait() noexcept {
        posix::FdTable<8> fds{};
        posix::FileService<4> files{};
        posix::PipeService<2, 8> pipes{};
        posix::ProcService<4, 4, 8, 4> procs{};
        fds.init();
        files.init();
        pipes.init();
        procs.init();
        procs.bind_fd_table(fds);
        procs.bind_file_service(files);
        auto rreg = procs.register_executable("demo", &demo_main);
        check_true("spawn-register", rreg);

        posix::Api<8, 2, 8, 4, 4, 4> api{fds, files, pipes, procs};
        const char* argv[] = {"demo", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "demo";
        cfg.argv = std::span<const char* const>(argv, 1);
        int pid = api.spawn(cfg);
        check_true("spawn-basic", pid > 0);
        api.bind_process(posix::ProcessId{pid});
        log_line("[posix-smoke] api spawn-child-open begin");
        int newfd = api.open("/dev/null", posix::O_WRONLY, 0);
        log_line("[posix-smoke] api spawn-child-open end");
        check_true("spawn-child-open", newfd >= 0);
        api.unbind_process();
        auto parent_entry = fds.get(newfd);
        check_true("spawn-parent-unchanged", !parent_entry);
        int status = 0;
        int wpid = api.waitpid(posix::ProcessId{pid}, &status, 0);
        check_eq("spawn-waitpid", wpid, pid);
    }

    void test_api_spawnp_wait() noexcept {
        posix::FdTable<8> fds{};
        posix::FileService<4> files{};
        posix::PipeService<2, 8> pipes{};
        posix::ProcService<4, 4, 8, 4> procs{};
        fds.init();
        files.init();
        pipes.init();
        procs.init();
        procs.bind_fd_table(fds);
        procs.bind_file_service(files);
        auto rreg = procs.register_executable("/bin/demo", &demo_main);
        check_true("spawnp-register", rreg);

        posix::Api<8, 2, 8, 4, 4, 4> api{fds, files, pipes, procs};
        const char* argv[] = {"demo", nullptr};
        const char* envp[] = {"PATH=/bin:/usr/bin", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "demo";
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.envp = std::span<const char* const>(envp, 1);

        int pid = api.spawnp(cfg);
        check_true("spawnp-basic", pid > 0);
        int status = 0;
        check_eq("spawnp-waitpid", api.waitpid(posix::ProcessId{pid}, &status, 0), pid);
        check_eq("spawnp-code", (status >> 8) & 0xff, 3);

        cfg.path = nullptr;
        int pid2 = api.spawnp(cfg);
        check_true("spawnp-argv0-basic", pid2 > 0);
        int status2 = 0;
        check_eq("spawnp-argv0-waitpid", api.waitpid(posix::ProcessId{pid2}, &status2, 0), pid2);
        check_eq("spawnp-argv0-code", (status2 >> 8) & 0xff, 3);

        const char* miss_envp[] = {"PATH=/usr/local/bin", nullptr};
        cfg.envp = std::span<const char* const>(miss_envp, 1);
        posix::set_errno(0);
        check_eq("spawnp-miss", api.spawnp(cfg), -1);
        check_eq("spawnp-miss-errno", posix::get_errno(), posix::ENOENT);
    }

    void test_api_getpid() noexcept {
        posix::FdTable<8> fds{};
        posix::FileService<4> files{};
        posix::PipeService<2, 8> pipes{};
        posix::ProcService<4, 4, 8, 4> procs{};
        fds.init();
        files.init();
        pipes.init();
        procs.init();

        posix::Api<8, 2, 8, 4, 4, 4> api{fds, files, pipes, procs};
        check_eq("getpid-unbound", api.getpid(), 0);
        api.bind_process(posix::ProcessId{42});
        check_eq("getpid-bound", api.getpid(), 42);
        api.unbind_process();
        check_eq("getpid-unbound-after", api.getpid(), 0);
    }

    void test_api_sleep() noexcept {
        struct TestClockState {
            charm::system::ClockTick ticks_ms{0};
        };
        static TestClockState state{};
        static charm::system::Clock clock{
            &state,
            {
                [](void* ctx) noexcept -> charm::system::ClockTick {
                    auto* s = static_cast<TestClockState*>(ctx);
                    return s ? s->ticks_ms++ : 0;
                },
                [](void* ctx) noexcept -> charm::system::ClockTick {
                    auto* s = static_cast<TestClockState*>(ctx);
                    return s ? (s->ticks_ms++ * 1000u) : 0;
                }
            }
        };
        state.ticks_ms = 0;
        charm::system::time::bind(clock);

        posix::FdTable<8> fds{};
        posix::FileService<4> files{};
        posix::PipeService<2, 8> pipes{};
        posix::ProcService<4, 4, 8, 4> procs{};
        fds.init();
        files.init();
        pipes.init();
        procs.init();

        posix::Api<8, 2, 8, 4, 4, 4> api{fds, files, pipes, procs};
        check_eq("sleep-zero", api.sleep(0), 0);
        check_eq("sleep-one", api.sleep(1), 0);
    }

    void test_api_fs_basics_and_readdir() noexcept {
        fs::clear_mounts();
        ApiRamFsMount<64, 32, 64> ramfs{};
        auto mount_st = fs::add_mount("", ramfs.mount_point());
        check_true("fs-mount", mount_st);

        posix::FdTable<8> fds{};
        posix::FileService<8> files{};
        posix::PipeService<2, 8> pipes{};
        posix::ProcService<4, 4, 8, 8> procs{};
        fds.init();
        files.init();
        pipes.init();
        procs.init();

        posix::Api<8, 2, 8, 4, 4, 8> api{fds, files, pipes, procs};
        check_eq("fs-mkdir", api.mkdir("/work"), 0);

        int fd = api.open("/work/a.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("fs-open", fd >= 0);
        check_eq("fs-write", api.write(fd, "z", 1), 1);
        check_eq("fs-close", api.close(fd), 0);

        check_eq("fs-rename", api.rename("/work/a.txt", "/work/b.txt"), 0);

        auto* root_dir = api.opendir("/");
        check_true("fs-opendir-root", root_dir != nullptr);
        bool saw_work = false;
        posix::set_errno(0);
        while (const auto* ent = api.readdir(root_dir)) {
            if (std::string_view{ent->d_name.data()} == "work") {
                saw_work = true;
                check_eq("fs-root-dir-mode", ent->d_mode & posix::S_IFMT, posix::S_IFDIR);
            }
        }
        check_eq("fs-closedir-root", api.closedir(root_dir), 0);
        check_true("fs-saw-work", saw_work);

        auto* work_dir = api.opendir("/work");
        check_true("fs-opendir-work", work_dir != nullptr);
        bool saw_file = false;
        posix::set_errno(0);
        while (const auto* ent = api.readdir(work_dir)) {
            if (std::string_view{ent->d_name.data()} == "b.txt") {
                saw_file = true;
                check_eq("fs-work-file-mode", ent->d_mode & posix::S_IFMT, posix::S_IFREG);
                check_eq("fs-work-file-size", ent->d_size, 1u);
            }
        }
        check_eq("fs-closedir-work", api.closedir(work_dir), 0);
        check_true("fs-saw-file", saw_file);

        check_eq("fs-unlink", api.unlink("/work/b.txt"), 0);
        posix::set_errno(0);
        check_eq("fs-unlink-missing", api.unlink("/work/b.txt"), -1);
        check_eq("fs-unlink-missing-errno", posix::get_errno(), posix::ENOENT);

        work_dir = api.opendir("/work");
        check_true("fs-opendir-empty", work_dir != nullptr);
        posix::set_errno(0);
        check_true("fs-empty-readdir", api.readdir(work_dir) == nullptr);
        check_eq("fs-empty-readdir-errno", posix::get_errno(), 0);
        check_eq("fs-closedir-empty", api.closedir(work_dir), 0);

        fd = api.open("/append.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("fs-append-open-base", fd >= 0);
        check_eq("fs-append-write-base", api.write(fd, "a\n", 2), 2);
        check_eq("fs-append-close-base", api.close(fd), 0);

        fd = api.open("/append.txt", posix::O_WRONLY | posix::O_APPEND, 0);
        check_true("fs-append-open-append", fd >= 0);
        check_eq("fs-append-write-append", api.write(fd, "b\n", 2), 2);
        check_eq("fs-append-close-append", api.close(fd), 0);

        fd = api.open("/append.txt", posix::O_RDONLY, 0);
        check_true("fs-append-open-read", fd >= 0);
        std::array<char, 8> append_buf{};
        auto append_read = api.read(fd, append_buf.data(), append_buf.size());
        check_eq("fs-append-read", append_read, static_cast<posix::ssize_t>(4));
        check_eq("fs-append-text", std::string_view{append_buf.data(), 4}, std::string_view{"a\nb\n"});
        check_eq("fs-append-close-read", api.close(fd), 0);
    }

    void test_api_dev_null_and_isatty() noexcept {
        fs::clear_mounts();
        fs::Mount mount{};
        mount.ops = &dummy_mount_ops;
        mount.data = nullptr;
        auto mount_st = fs::add_mount("", &mount);
        check_true("devnull-mount", mount_st);

        posix::FdTable<8> fds{};
        posix::FileService<4> files{};
        posix::PipeService<2, 8> pipes{};
        posix::ProcService<4, 4, 8, 4> procs{};
        fds.init();
        files.init();
        pipes.init();
        procs.init();

        posix::Api<8, 2, 8, 4, 4, 4> api{fds, files, pipes, procs};
        int fd = api.open("/dev/null", posix::O_WRONLY, 0);
        check_true("devnull-open", fd >= 0);
        posix::PosixStat stat_out{};
        check_eq("devnull-stat", api.stat("/dev/null", &stat_out), 0);
        check_eq("devnull-stat-size", stat_out.size, 0u);
        check_eq("devnull-stat-mode", stat_out.mode & posix::S_IFMT, posix::S_IFCHR);
        posix::PosixStat fst{};
        check_eq("devnull-fstat", api.fstat(fd, &fst), 0);
        check_eq("devnull-fstat-size", fst.size, 0u);
        check_eq("devnull-fstat-mode", fst.mode & posix::S_IFMT, posix::S_IFCHR);
        const char msg[] = "abc";
        auto w = api.write(fd, msg, 3);
        check_eq("devnull-write", w, 3);
        auto r = api.read(fd, nullptr, 0);
        check_eq("devnull-read0", r, 0);
        check_eq("devnull-close", api.close(fd), 0);

        static const posix::FdOps kOps{
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        };
        posix::FdEntry term{};
        term.kind = posix::FdKind::term;
        term.flags = posix::FdFlags::read_write;
        term.ops = &kOps;
        term.ctx = nullptr;
        auto rfd = fds.attach(term, 3);
        check_true("isatty-attach", rfd);
        check_eq("isatty-term", api.isatty(3), 1);

        int file_fd = api.open("/tmp/isatty.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("isatty-file-open", file_fd >= 0);
        posix::PosixStat file_st{};
        check_eq("isatty-file-fstat", api.fstat(file_fd, &file_st), 0);
        check_eq("isatty-file-mode", file_st.mode & posix::S_IFMT, posix::S_IFREG);
        posix::set_errno(0);
        check_eq("isatty-file", api.isatty(file_fd), 0);
        check_eq("isatty-file-errno", posix::get_errno(), 0);

        int pipefd[2]{-1, -1};
        check_eq("isatty-pipe-create", api.pipe(pipefd), 0);
        posix::PosixStat pipe_st{};
        check_eq("isatty-pipe-fstat-r", api.fstat(pipefd[0], &pipe_st), 0);
        check_eq("isatty-pipe-mode-r", pipe_st.mode & posix::S_IFMT, posix::S_IFIFO);
        check_eq("isatty-pipe-fstat-w", api.fstat(pipefd[1], &pipe_st), 0);
        check_eq("isatty-pipe-mode-w", pipe_st.mode & posix::S_IFMT, posix::S_IFIFO);
        posix::set_errno(0);
        check_eq("isatty-pipe-read", api.isatty(pipefd[0]), 0);
        check_eq("isatty-pipe-read-errno", posix::get_errno(), 0);
        posix::set_errno(0);
        check_eq("isatty-pipe-write", api.isatty(pipefd[1]), 0);
        check_eq("isatty-pipe-write-errno", posix::get_errno(), 0);

        check_eq("isatty-nonterm", api.isatty(4), 0);
        check_eq("isatty-badfd", api.isatty(-1), 0);

        check_eq("isatty-file-close", api.close(file_fd), 0);
        check_eq("isatty-pipe-close-r", api.close(pipefd[0]), 0);
        check_eq("isatty-pipe-close-w", api.close(pipefd[1]), 0);
    }

    void test_api_errno_contracts() noexcept {
        fs::clear_mounts();
        fs::Mount mount{};
        mount.ops = &dummy_mount_ops;
        mount.data = nullptr;
        auto mount_st = fs::add_mount("", &mount);
        check_true("errno-mount", mount_st);

        {
            posix::FdTable<4> fds{};
            posix::FileService<4> files{};
            posix::PipeService<2, 8> pipes{};
            posix::ProcService<4, 4, 4, 4> procs{};
            fds.init();
            files.init();
            pipes.init();
            procs.init();

            posix::Api<4, 2, 8, 4, 4, 4> api{fds, files, pipes, procs};
            char ch{};
            posix::PosixStat st{};

            posix::set_errno(0);
            check_eq("errno-close-bad-rc", api.close(-1), -1);
            check_eq("errno-close-bad", posix::get_errno(), posix::EBADF);

            posix::set_errno(0);
            check_eq("errno-read-bad-rc", api.read(-1, &ch, 1), static_cast<posix::ssize_t>(-1));
            check_eq("errno-read-bad", posix::get_errno(), posix::EBADF);

            posix::set_errno(0);
            check_eq("errno-write-bad-rc", api.write(-1, &ch, 1), static_cast<posix::ssize_t>(-1));
            check_eq("errno-write-bad", posix::get_errno(), posix::EBADF);

            posix::set_errno(0);
            check_eq("errno-fstat-bad-rc", api.fstat(-1, &st), -1);
            check_eq("errno-fstat-bad", posix::get_errno(), posix::EBADF);
        }

        {
            posix::FdTable<1> fds{};
            posix::FileService<4> files{};
            posix::PipeService<2, 8> pipes{};
            posix::ProcService<4, 4, 1, 4> procs{};
            fds.init();
            files.init();
            pipes.init();
            procs.init();

            posix::Api<1, 2, 8, 4, 4, 4> api{fds, files, pipes, procs};
            int first = api.open("/tmp/a", posix::O_WRONLY | posix::O_CREAT, 0);
            check_true("errno-open-first", first >= 0);
            posix::set_errno(0);
            check_eq("errno-open-full-rc", api.open("/tmp/b", posix::O_WRONLY | posix::O_CREAT, 0), -1);
            check_eq("errno-open-full", posix::get_errno(), posix::EMFILE);
            check_eq("errno-open-first-close", api.close(first), 0);
        }

        {
            posix::FdTable<8> fds{};
            posix::FileService<4> files{};
            posix::PipeService<1, 8> pipes{};
            posix::ProcService<4, 4, 8, 4> procs{};
            fds.init();
            files.init();
            pipes.init();
            procs.init();

            posix::Api<8, 1, 8, 4, 4, 4> api{fds, files, pipes, procs};
            int first_pipe[2]{-1, -1};
            int second_pipe[2]{-1, -1};
            check_eq("errno-pipe-first", api.pipe(first_pipe), 0);
            posix::set_errno(0);
            check_eq("errno-pipe-full-rc", api.pipe(second_pipe), -1);
            check_eq("errno-pipe-full", posix::get_errno(), posix::ENOSPC);

            check_eq("errno-pipe-close-read", api.close(first_pipe[0]), 0);
            const char msg[] = "x";
            posix::set_errno(0);
            check_eq("errno-pipe-epipe-rc", api.write(first_pipe[1], msg, 1), static_cast<posix::ssize_t>(-1));
            check_eq("errno-pipe-epipe", posix::get_errno(), posix::EPIPE);
            check_eq("errno-pipe-close-write", api.close(first_pipe[1]), 0);
        }
    }
} // namespace

export void run_posix_api_smoke_tests() noexcept {
    log_line("[posix-smoke] api begin");
    test_api_open_close();
    test_api_pipe_rw();
    test_api_spawn_wait();
    test_api_spawnp_wait();
    test_api_getpid();
    test_api_sleep();
    test_api_fs_basics_and_readdir();
    test_api_dev_null_and_isatty();
    test_api_errno_contracts();
    log_line("[posix-smoke] api end ok");
}

#endif
