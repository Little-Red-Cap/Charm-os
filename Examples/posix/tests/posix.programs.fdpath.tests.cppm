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

#include "../elf_samples/fd_probe.elf.inc"
#include "../elf_samples/stat_probe.elf.inc"

export module posix.programs.fdpath.tests;

#if defined(POSIX_PROGRAMS_SMOKE_TEST) && POSIX_PROGRAMS_SMOKE_TEST

import posix.test_harness;

namespace {
    using namespace posix::testsupport;

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

            cat_fd = h.api.open("/empty.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
            check_true("fd-probe-data-open", cat_fd >= 0);
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

        const char* argv[] = {"elf:/fd_probe.elf", "/empty.txt", nullptr};
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
        check_eq("fd-probe-out", out, std::string_view{"rw-ok\n"});

        (void)h.fds.dup2(2, 1);
        h.procs.enable_elf_hostcalls(false);
        h.procs.enable_elf_exec(false);
        h.unbind_env();
    }

    void test_open_path_type_errors() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto mount_status = fs::add_mount("", ramfs.mount_point());
        check_true("path-open-mount", mount_status);
        check_true("path-open-mkdir", ramfs.fs.mkdir("/adir"));

        Harness h{};
        h.bind_env();

        int wfd = h.api.open("/afile", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("path-open-file", wfd >= 0);
        check_eq("path-open-close-file", h.api.close(wfd), 0);

        posix::set_errno(0);
        auto dir_fd = h.api.open("/adir", posix::O_WRONLY, 0);
        check_eq("path-open-dir-fd", dir_fd, -1);
        check_eq("path-open-dir-errno", posix::get_errno(), posix::EISDIR);

        posix::set_errno(0);
        auto child_fd = h.api.open("/afile/child", posix::O_RDONLY, 0);
        check_eq("path-open-child-fd", child_fd, -1);
        check_eq("path-open-child-errno", posix::get_errno(), posix::ENOTDIR);

        h.unbind_env();
    }

    void test_elf_file_stat_probe() noexcept {
        fs::clear_mounts();
        RamFsMount<256, 64, 512> ramfs{};
        auto mount_status = fs::add_mount("", ramfs.mount_point());
        check_true("stat-probe-mount", mount_status);

        {
            Harness h{};
            h.bind_env();
            h.procs.bind_file_service(h.files);

            int data_fd = h.api.open("/stat.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
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

        int out_pipe[2]{-1, -1};
        check_eq("stat-probe-out-pipe", h.api.pipe(out_pipe), 0);
        check_true("stat-probe-dup2-out", h.fds.dup2(out_pipe[1], 1));

        const char* argv[] = {"elf:/stat_probe.elf", "/stat.txt", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "elf:/stat_probe.elf";
        cfg.argv = std::span<const char* const>(argv, 2);
        auto sp = h.procs.spawn(cfg);
        check_true("stat-probe-spawn", sp);
        auto st = h.procs.waitpid(sp.value().pid, 0);
        check_true("stat-probe-wait", st);
        check_eq("stat-probe-code", st.value().code, 0);

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

} // namespace

export void run_posix_program_fdpath_smoke_tests() noexcept {
    using namespace posix::testsupport;
    log_line("[posix-smoke] programs phase fd-probe begin");
    test_elf_file_fd_probe();
    log_line("[posix-smoke] programs phase fd-probe end");
    log_line("[posix-smoke] programs phase path-open-errors begin");
    test_open_path_type_errors();
    log_line("[posix-smoke] programs phase path-open-errors end");
    log_line("[posix-smoke] programs phase stat-probe begin");
    test_elf_file_stat_probe();
    log_line("[posix-smoke] programs phase stat-probe end");
}

#endif
