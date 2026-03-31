//
// Minimal smoke tests for posix.proc (no framework).
//

module;
#include <array>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string_view>
#include <type_traits>

export module posix.proc.tests;

#if defined(POSIX_PROC_SMOKE_TEST) && POSIX_PROC_SMOKE_TEST

import posix.proc;
import posix.fd_table;
import posix.file;
import posix.env;
import fs_core;
import fs_errno;
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
        std::snprintf(buf, sizeof(buf), "[posix-smoke] proc %s %s", label, ok ? "ok" : "fail");
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
            char buf[128]{};
            std::snprintf(buf, sizeof(buf),
                "[posix-smoke] proc %s fail: expected=%lld actual=%lld",
                label, to_ll(b), to_ll(a));
            log_line(buf);
            fail();
        }
        log_step(label, true);
    }

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
    util::Result<void> dummy_dup(void*) noexcept { return {}; }

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

    void test_spawn_wait() noexcept {
        posix::ProcService<4, 4, 8, 4> procs{};
        posix::FdTable<8> table{};
        table.init();
        procs.init();
        procs.bind_fd_table(table);
        auto rreg = procs.register_executable("demo", &demo_main);
        check_true("spawn-register", rreg);

        const char* argv[] = {"demo", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "demo";
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.path_mode = posix::PathMode::exact;

        auto spawn = procs.spawn(cfg);
        check_true("spawn-basic", spawn);
        const auto pid = spawn.value().pid;

        auto st = procs.waitpid(pid, 0);
        check_true("wait-basic", st);
        check_eq("wait-pid", st.value().pid.value, pid.value);
        check_eq("wait-code", st.value().code, 42);
        check_eq("wait-kind", st.value().kind, posix::WaitKind::exited);
    }

    void test_search_path_argv0() noexcept {
        posix::ProcService<4, 4, 8, 4> procs{};
        posix::FdTable<8> table{};
        table.init();
        procs.init();
        procs.bind_fd_table(table);
        auto rreg = procs.register_executable("/bin/hello", &demo_main);
        check_true("search-register", rreg);

        const char* argv[] = {"hello", nullptr};
        const char* envp[] = {"PATH=/bin:/usr/bin", nullptr};
        posix::SpawnConfig cfg{};
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.envp = std::span<const char* const>(envp, 1);
        cfg.path_mode = posix::PathMode::search_path;

        const auto path_list = posix::envp_get(cfg.envp, posix::kPathKey);
        check_true("search-envp-path", !path_list.empty());

        auto spawn = procs.spawn(cfg);
        if (!spawn) {
            char buf[96]{};
            std::snprintf(buf, sizeof(buf), "[posix-smoke] proc search-spawn err=%lld", to_ll(spawn.error()));
            log_line(buf);
        }
        check_true("search-spawn", spawn);
        auto st = procs.waitpid(spawn.value().pid, 0);
        check_true("search-wait", st);
    }

    void test_stdio_and_actions() noexcept {
        posix::ProcService<4, 4, 8, 4> procs{};
        posix::FdTable<8> table{};
        table.init();
        procs.init();
        procs.bind_fd_table(table);

        auto rreg = procs.register_executable("demo", &demo_main);
        check_true("stdio-register", rreg);

        static const posix::FdOps kOps{
            &dummy_read,
            &dummy_write,
            &dummy_close,
            &dummy_stat,
            &dummy_dup
        };

        posix::FdEntry entry{};
        entry.kind = posix::FdKind::file;
        entry.flags = posix::FdFlags::read_write;
        entry.ops = &kOps;
        entry.ctx = nullptr;

        auto rfd3 = table.attach(entry, 3);
        check_true("stdio-attach-3", rfd3);
        auto rfd4 = table.attach(entry, 4);
        check_true("stdio-attach-4", rfd4);

        posix::FileActions<16> actions{};
        check_true("stdio-action-dup2", actions.add_dup2(3, 1));
        check_true("stdio-action-close", actions.add_close(4));

        const char* argv[] = {"demo", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "demo";
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.file_actions = &actions;
        cfg.stdio_in = 3;
        cfg.stdio_out = 4;

        auto spawn = procs.spawn(cfg);
        check_true("stdio-spawn", spawn);
        auto st = procs.waitpid(spawn.value().pid, 0);
        check_true("stdio-wait", st);

        // parent table should remain unchanged after spawn
        auto entry1 = table.get(1);
        check_true("stdio-parent-1", !entry1);
        auto entry4 = table.get(4);
        check_true("stdio-parent-4", entry4);
        check_eq("stdio-parent-4-id", entry4.value()->id, 4);

        posix::FileActions<16> open_actions{};
        check_true("stdio-open-action", open_actions.add_open(5, "/tmp/x", 0, 0));
        cfg.file_actions = &open_actions;
        auto spawn_open = procs.spawn(cfg);
        check_true("stdio-open-fail", !spawn_open);
        check_eq("stdio-open-err", spawn_open.error(), util::Errc::not_supported);
    }

    void test_open_action() noexcept {
        fs::clear_mounts();
        fs::Mount mount{};
        mount.ops = &dummy_mount_ops;
        mount.data = nullptr;
        auto st = fs::add_mount("", &mount);
        check_true("open-mount", st);

        posix::ProcService<4, 4, 8, 4> procs{};
        posix::FdTable<8> table{};
        posix::FileService<4> files{};
        table.init();
        files.init();
        procs.init();
        procs.bind_fd_table(table);
        procs.bind_file_service(files);

        auto rreg = procs.register_executable("demo", &demo_main);
        check_true("open-register", rreg);

        posix::FileActions<16> actions{};
        check_true("open-action", actions.add_open(3, "/tmp/x", posix::O_WRONLY | posix::O_CREAT, 0));

        const char* argv[] = {"demo", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "demo";
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.file_actions = &actions;

        auto spawn = procs.spawn(cfg);
        check_true("open-spawn", spawn);
        auto fd_entry = table.get(3);
        check_true("open-parent-unchanged", !fd_entry);
    }
} // namespace

export void run_posix_proc_smoke_tests() noexcept {
    log_line("[posix-smoke] proc begin");
    test_spawn_wait();
    test_search_path_argv0();
    test_stdio_and_actions();
    test_open_action();
    log_line("[posix-smoke] proc end ok");
}

#endif
