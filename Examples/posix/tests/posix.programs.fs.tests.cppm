//
// Program-driven smoke tests for BusyBox Phase 1 style FS applets.
//

module;
#include <array>
#include <span>
#include <string_view>

#include "../elf_samples/write_file.elf.inc"

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

    void test_real_elf_write_file() noexcept {
        fs::clear_mounts();
        RamFsMount<128, 64, 256> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("elfwrite-mount", st);

        Harness h{};
        h.procs.bind_file_service(h.files);
        h.procs.enable_elf_exec(true);
        h.procs.enable_elf_hostcalls(true);

        int elf_fd = h.api.open("/write_file.elf", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("elfwrite-elf-open", elf_fd >= 0);
        auto elf_write = h.api.write(elf_fd, write_file_elf, write_file_elf_len);
        check_true("elfwrite-elf-write", elf_write == static_cast<posix::ssize_t>(write_file_elf_len));
        check_eq("elfwrite-elf-close", h.api.close(elf_fd), 0);

        int seed_fd = h.api.open("/write-out.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("elfwrite-seed-open", seed_fd >= 0);
        check_eq("elfwrite-seed-write", h.api.write(seed_fd, "old-data", 8), 8);
        check_eq("elfwrite-seed-close", h.api.close(seed_fd), 0);

        int out_pipe[2]{-1, -1};
        check_eq("elfwrite-out-pipe", h.api.pipe(out_pipe), 0);

        int err_pipe[2]{-1, -1};
        check_eq("elfwrite-err-pipe", h.api.pipe(err_pipe), 0);

        const char payload[] = "elf-write\n";
        const char* argv[] = {"elf:/write_file.elf", "/write-out.txt", payload, nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "elf:/write_file.elf";
        cfg.argv = std::span<const char* const>(argv, 3);
        cfg.stdio_out = out_pipe[1];
        cfg.stdio_err = err_pipe[1];

        auto sp = h.procs.spawn(cfg);
        check_true("elfwrite-spawn", sp);
        auto wait = h.procs.waitpid(sp.value().pid, 0);
        check_true("elfwrite-wait", wait);
        check_eq("elfwrite-code", wait.value().code, 0);

        (void)h.api.close(out_pipe[1]);
        (void)h.api.close(err_pipe[1]);

        std::array<char, 32> out_buf{};
        util::usize out_size = 0;
        auto out = read_from_fd(h.api, out_pipe[0], out_buf, out_size);
        check_eq("elfwrite-out", out, std::string_view{"write-ok\n"});
        check_eq("elfwrite-out-close", h.api.close(out_pipe[0]), 0);

        std::array<char, 32> err_buf{};
        util::usize err_size = 0;
        auto err = read_from_fd(h.api, err_pipe[0], err_buf, err_size);
        check_eq("elfwrite-err", err, std::string_view{});
        check_eq("elfwrite-err-close", h.api.close(err_pipe[0]), 0);

        posix::PosixStat file_st{};
        check_eq("elfwrite-stat", h.api.stat("/write-out.txt", &file_st), 0);
        check_eq("elfwrite-stat-size", file_st.size, static_cast<util::u64>(sizeof(payload) - 1));

        int read_fd = h.api.open("/write-out.txt", posix::O_RDONLY, 0);
        check_true("elfwrite-read-open", read_fd >= 0);
        std::array<char, 32> file_buf{};
        util::usize file_size = 0;
        auto file_text = read_from_fd(h.api, read_fd, file_buf, file_size);
        check_eq("elfwrite-file-text", file_text, std::string_view{payload, sizeof(payload) - 1});
        check_eq("elfwrite-read-close", h.api.close(read_fd), 0);

        h.procs.enable_elf_hostcalls(false);
        h.procs.enable_elf_exec(false);
    }

    void test_busybox_fs_slice() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("bbfs-mount", st);

        Harness h{};
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
    }
} // namespace

export void run_posix_program_fs_smoke_tests() noexcept {
    using namespace posix::testsupport;
    log_line("[posix-smoke] programs phase fs begin");
    test_real_elf_write_file();
    test_busybox_fs_slice();
    log_line("[posix-smoke] programs phase fs end");
}

#endif
