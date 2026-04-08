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

export module posix.programs.shell.tests;

#if defined(POSIX_PROGRAMS_SMOKE_TEST) && POSIX_PROGRAMS_SMOKE_TEST

import posix.test_harness;

namespace {
    using namespace posix::testsupport;

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
        auto child_status = h.procs.waitpid(sp.value().pid, 0);
        check_true("sh-wait", child_status);
        check_eq("sh-code", child_status.value().code, 0);
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
        const char* argv[] = {"sh", "-c", "echo hi", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "sh";
        cfg.argv = std::span<const char* const>(argv, 3);
        cfg.stdio_out = pipefd[1];

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

export void run_posix_program_shell_smoke_tests() noexcept {
    using namespace posix::testsupport;
    log_line("[posix-smoke] programs phase echo-to-file begin");
    test_echo_to_file();
    log_line("[posix-smoke] programs phase echo-to-file end");
    log_line("[posix-smoke] programs phase cat-from-file begin");
    test_cat_from_file();
    log_line("[posix-smoke] programs phase cat-from-file end");
    log_line("[posix-smoke] programs phase pipe-cat begin");
    test_echo_pipe_cat();
    log_line("[posix-smoke] programs phase pipe-cat end");
    log_line("[posix-smoke] programs phase pipe-chain begin");
    test_echo_pipe_cat_chain();
    log_line("[posix-smoke] programs phase pipe-chain end");
    log_line("[posix-smoke] programs phase sh-echo begin");
    test_sh_c_echo();
    log_line("[posix-smoke] programs phase sh-echo end");
    log_line("[posix-smoke] programs phase sh-redir-pipe begin");
    test_sh_c_redir_and_pipe();
    log_line("[posix-smoke] programs phase sh-redir-pipe end");
}

#endif
