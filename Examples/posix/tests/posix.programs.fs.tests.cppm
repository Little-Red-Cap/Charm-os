//
// Program-driven smoke tests for BusyBox Phase 1 style FS applets.
//

module;
#include <array>
#include <span>
#include <string_view>

export module posix.programs.fs.tests;

#if defined(POSIX_PROGRAMS_SMOKE_TEST) && POSIX_PROGRAMS_SMOKE_TEST

import posix.test_harness;

namespace {
    using namespace posix::testsupport;

    int spawn_path_program(Harness& h,
                           const char* path,
                           std::span<const char* const> argv,
                           int stdio_in,
                           int stdio_out) noexcept {
        posix::SpawnConfig cfg{};
        cfg.path = path;
        cfg.argv = argv;
        cfg.envp = program_path_env();
        cfg.stdio_in = stdio_in;
        cfg.stdio_out = stdio_out;
        return h.api.spawnp(cfg);
    }

    int wait_ok(Harness& h, int pid, const char* label) noexcept {
        check_true(label, pid > 0);
        int status = 0;
        check_eq(label, h.api.waitpid(posix::ProcessId{pid}, &status, 0), pid);
        return (status >> 8) & 0xff;
    }

    std::string_view capture_stdout(Harness& h,
                                    const char* path,
                                    std::span<const char* const> argv,
                                    std::span<char> out,
                                    util::usize& out_size,
                                    const char* spawn_label,
                                    const char* wait_label) noexcept {
        int pipefd[2]{-1, -1};
        check_eq(spawn_label, h.api.pipe(pipefd), 0);
        int read_fd = pipefd[0];
        if (read_fd == 0 || read_fd == 1 || read_fd == 2) {
            check_true("fs-move-read", h.fds.dup2(read_fd, 11));
            read_fd = 11;
        }
        const int pid = spawn_path_program(h, path, argv, -1, pipefd[1]);
        check_eq(wait_label, wait_ok(h, pid, wait_label), 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }
        auto text = read_from_fd(h.api, read_fd, out, out_size);
        if (read_fd != pipefd[0]) {
            (void)h.api.close(read_fd);
        } else {
            (void)h.api.close(pipefd[0]);
        }
        return text;
    }

    void test_busybox_fs_slice() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("bbfs-mount", st);

        Harness h{};
        h.bind_env();
        auto reg_busybox = h.procs.register_executable("/bin/busybox", &busybox_main);
        check_true("bbfs-register-busybox", reg_busybox);

        const char* mkdir_argv[] = {"busybox", "mkdir", "work", nullptr};
        check_eq("bbfs-mkdir", wait_ok(h,
            spawn_path_program(h, "busybox", std::span<const char* const>(mkdir_argv, 3), -1, -1),
            "bbfs-mkdir-wait"), 0);

        int fd = h.api.open("/work/a.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("bbfs-open", fd >= 0);
        check_eq("bbfs-write", h.api.write(fd, "x", 1), 1);
        check_eq("bbfs-close", h.api.close(fd), 0);

        std::array<char, 64> buf{};
        util::usize out_size = 0;
        const char* ls_root_argv[] = {"busybox", "ls", "/", nullptr};
        auto root_text = capture_stdout(h,
            "busybox",
            std::span<const char* const>(ls_root_argv, 3),
            buf,
            out_size,
            "bbfs-ls-root-pipe",
            "bbfs-ls-root-wait");
        check_true("bbfs-root-has-work", root_text.find("work\n") != std::string_view::npos);

        const char* mv_argv[] = {"busybox", "mv", "/work/a.txt", "/work/b.txt", nullptr};
        check_eq("bbfs-mv", wait_ok(h,
            spawn_path_program(h, "busybox", std::span<const char* const>(mv_argv, 4), -1, -1),
            "bbfs-mv-wait"), 0);

        const char* ls_work_argv[] = {"busybox", "ls", "/work", nullptr};
        auto work_text = capture_stdout(h,
            "busybox",
            std::span<const char* const>(ls_work_argv, 3),
            buf,
            out_size,
            "bbfs-ls-work-pipe",
            "bbfs-ls-work-wait");
        check_eq("bbfs-ls-work-text", work_text, std::string_view{"b.txt\n"});

        const char* rm_argv[] = {"busybox", "rm", "/work/b.txt", nullptr};
        check_eq("bbfs-rm", wait_ok(h,
            spawn_path_program(h, "busybox", std::span<const char* const>(rm_argv, 3), -1, -1),
            "bbfs-rm-wait"), 0);

        auto empty_text = capture_stdout(h,
            "busybox",
            std::span<const char* const>(ls_work_argv, 3),
            buf,
            out_size,
            "bbfs-ls-empty-pipe",
            "bbfs-ls-empty-wait");
        check_eq("bbfs-ls-empty-text", empty_text, std::string_view{});
        h.unbind_env();
    }
} // namespace

export void run_posix_program_fs_smoke_tests() noexcept {
    using namespace posix::testsupport;
    log_line("[posix-smoke] programs phase fs begin");
    test_busybox_fs_slice();
    log_line("[posix-smoke] programs phase fs end");
}

#endif
