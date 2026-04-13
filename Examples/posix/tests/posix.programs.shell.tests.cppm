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

    inline int g_kill_tool_target_runs = 0;

    int kill_tool_target_main(int, char**, char**) {
        ++g_kill_tool_target_runs;
        return 17;
    }

    enum class KillHelperKind {
        shell,
        busybox,
    };

    struct KillToolHookCtx {
        ApiType* api{nullptr};
        KillHelperKind kind{KillHelperKind::shell};
        bool fired{false};
        int helper_pid{-1};
        int helper_wait_rc{-1};
        int helper_status{-1};
    };

    void kill_tool_on_enter(posix::ProcessId pid, void* ctx) noexcept {
        auto* state = static_cast<KillToolHookCtx*>(ctx);
        if (!state || !state->api) {
            return;
        }

        state->api->push_process(pid);
        if (state->fired) {
            return;
        }

        state->fired = true;
        char pid_buf[16]{};
        const auto written = std::snprintf(pid_buf, sizeof(pid_buf), "%d", pid.value);
        if (written <= 0 || static_cast<util::usize>(written) >= sizeof(pid_buf)) {
            return;
        }

        posix::SpawnConfig cfg{};
        cfg.envp = program_path_env();
        if (state->kind == KillHelperKind::shell) {
            char cmd_buf[32]{};
            const auto cmd_written = std::snprintf(cmd_buf, sizeof(cmd_buf), "kill %s", pid_buf);
            if (cmd_written <= 0 || static_cast<util::usize>(cmd_written) >= sizeof(cmd_buf)) {
                return;
            }
            const char* argv[] = {"sh", "-c", cmd_buf, nullptr};
            cfg.path = "sh";
            cfg.argv = std::span<const char* const>(argv, 3);
        } else {
            const char* argv[] = {"busybox", "kill", pid_buf, nullptr};
            cfg.path = "busybox";
            cfg.argv = std::span<const char* const>(argv, 3);
        }

        state->helper_pid = state->api->spawnp(cfg);
        if (state->helper_pid > 0) {
            state->helper_wait_rc = state->api->waitpid(posix::ProcessId{state->helper_pid}, &state->helper_status, 0);
        }
    }

    void kill_tool_on_exit(posix::ProcessId, void* ctx) noexcept {
        auto* state = static_cast<KillToolHookCtx*>(ctx);
        if (state && state->api) {
            state->api->pop_process();
        }
    }

    void test_echo_to_file() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("echo-mount", st);

        Harness h{};
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
    }

    void test_cat_from_file() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("cat-mount", st);

        Harness h{};
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
    }

    void test_echo_pipe_cat() noexcept {
        Harness h{};
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
    }

    void test_echo_pipe_cat_chain() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("chain-mount", st);

        Harness h{};
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
    }

    void test_sh_c_echo() noexcept {
        Harness h{};
        const auto path_env = program_path_env();
        auto reg_echo = h.procs.register_executable("/bin/echo", &echo_main);
        check_true("sh-register-echo", reg_echo);
        auto reg_sh = h.procs.register_executable("/bin/sh", &sh_main);
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
        cfg.envp = path_env;
        cfg.stdio_out = pipefd[1];

        int pid = h.api.spawnp(cfg);
        check_true("sh-spawn", pid > 0);
        int status = 0;
        check_eq("sh-waitpid", h.api.waitpid(posix::ProcessId{pid}, &status, 0), pid);
        check_eq("sh-status", (status >> 8) & 0xff, 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }

        std::array<char, 16> buf{};
        auto r = h.api.read(read_fd, buf.data(), buf.size());
        check_true("sh-read-len", r >= 3);
        check_eq("sh-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
    }

    void test_sh_c_redir_and_pipe() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("sh2-mount", st);

        Harness h{};
        const auto path_env = program_path_env();
        auto reg_echo = h.procs.register_executable("/bin/echo", &echo_main);
        check_true("sh2-register-echo", reg_echo);
        auto reg_cat = h.procs.register_executable("/bin/cat", &cat_main);
        check_true("sh2-register-cat", reg_cat);
        auto reg_stderr_demo = h.procs.register_executable("/bin/stderr_demo", &stderr_demo_main);
        check_true("sh2-register-stderr-demo", reg_stderr_demo);
        auto reg_sleep = h.procs.register_executable("/bin/sleep", &sleep_main);
        check_true("sh2-register-sleep", reg_sleep);
        auto reg_sh = h.procs.register_executable("/bin/sh", &sh_main);
        check_true("sh2-register-sh", reg_sh);

        const char* argv_redir[] = {"sh", "-c", "echo hi > out.txt", nullptr};
        posix::SpawnConfig cfg_redir{};
        cfg_redir.path = "sh";
        cfg_redir.argv = std::span<const char* const>(argv_redir, 3);
        cfg_redir.envp = path_env;
        int pid1 = h.api.spawnp(cfg_redir);
        check_true("sh2-spawn-redir", pid1 > 0);
        int status1 = 0;
        check_eq("sh2-waitpid-redir", h.api.waitpid(posix::ProcessId{pid1}, &status1, 0), pid1);
        check_eq("sh2-status-redir", (status1 >> 8) & 0xff, 0);

        int rfd = h.api.open("/out.txt", posix::O_RDONLY, 0);
        check_true("sh2-open-read", rfd >= 0);
        std::array<char, 16> buf{};
        auto r = h.api.read(rfd, buf.data(), buf.size());
        check_true("sh2-read-len", r >= 3);
        check_eq("sh2-read-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
        (void)h.api.close(rfd);

        const char* argv_append[] = {"sh", "-c", "echo ho >> out.txt", nullptr};
        posix::SpawnConfig cfg_append{};
        cfg_append.path = "sh";
        cfg_append.argv = std::span<const char* const>(argv_append, 3);
        cfg_append.envp = path_env;
        int pid_append = h.api.spawnp(cfg_append);
        check_true("sh2-spawn-append", pid_append > 0);
        int status_append = 0;
        check_eq("sh2-waitpid-append", h.api.waitpid(posix::ProcessId{pid_append}, &status_append, 0), pid_append);
        check_eq("sh2-status-append", (status_append >> 8) & 0xff, 0);

        rfd = h.api.open("/out.txt", posix::O_RDONLY, 0);
        check_true("sh2-open-read-append", rfd >= 0);
        std::array<char, 16> append_buf{};
        auto append_read = h.api.read(rfd, append_buf.data(), append_buf.size());
        check_true("sh2-append-read-len", append_read >= 6);
        check_eq("sh2-append-read-text", std::string_view{append_buf.data(), 6}, std::string_view{"hi\nho\n"});
        (void)h.api.close(rfd);

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
        cfg_pipe.envp = path_env;
        int pid2 = h.api.spawnp(cfg_pipe);
        check_true("sh2-spawn-pipe", pid2 > 0);
        int status2 = 0;
        check_eq("sh2-waitpid-pipe", h.api.waitpid(posix::ProcessId{pid2}, &status2, 0), pid2);
        check_eq("sh2-status-pipe", (status2 >> 8) & 0xff, 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }
        auto r2 = h.api.read(read_fd, buf.data(), buf.size());
        check_true("sh2-read2-len", r2 >= 3);
        check_eq("sh2-read2-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});

        int in_fd = h.api.open("/input.txt", posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
        check_true("sh2-input-open", in_fd >= 0);
        check_eq("sh2-input-write", h.api.write(in_fd, "cat-in\n", 7), 7);
        check_eq("sh2-input-close", h.api.close(in_fd), 0);

        int pipe_in[2]{-1, -1};
        check_eq("sh2-input-pipe", h.api.pipe(pipe_in), 0);
        int input_read_fd = pipe_in[0];
        if (input_read_fd == 0 || input_read_fd == 1 || input_read_fd == 2) {
            check_true("sh2-input-move-read", h.fds.dup2(input_read_fd, 12));
            input_read_fd = 12;
        }

        const char* argv_input[] = {"sh", "-c", "cat < input.txt", nullptr};
        posix::SpawnConfig cfg_input{};
        cfg_input.path = "sh";
        cfg_input.argv = std::span<const char* const>(argv_input, 3);
        cfg_input.envp = path_env;
        cfg_input.stdio_out = pipe_in[1];
        int pid3 = h.api.spawnp(cfg_input);
        check_true("sh2-spawn-input", pid3 > 0);
        int status3 = 0;
        check_eq("sh2-waitpid-input", h.api.waitpid(posix::ProcessId{pid3}, &status3, 0), pid3);
        check_eq("sh2-status-input", (status3 >> 8) & 0xff, 0);
        if (pipe_in[1] != 1) {
            (void)h.api.close(pipe_in[1]);
        }
        auto r3 = h.api.read(input_read_fd, buf.data(), buf.size());
        check_true("sh2-read3-len", r3 >= 7);
        check_eq("sh2-read3-text", std::string_view{buf.data(), 7}, std::string_view{"cat-in\n"});
        if (input_read_fd != pipe_in[0]) {
            (void)h.api.close(input_read_fd);
        } else {
            (void)h.api.close(pipe_in[0]);
        }

        int pipe_out[2]{-1, -1};
        check_eq("sh2-stderr-pipe", h.api.pipe(pipe_out), 0);
        int stderr_out_read = pipe_out[0];
        if (stderr_out_read == 0 || stderr_out_read == 1 || stderr_out_read == 2) {
            check_true("sh2-stderr-move-read", h.fds.dup2(stderr_out_read, 13));
            stderr_out_read = 13;
        }

        const char* argv_stderr[] = {"sh", "-c", "stderr_demo 2> err.txt", nullptr};
        posix::SpawnConfig cfg_stderr{};
        cfg_stderr.path = "sh";
        cfg_stderr.argv = std::span<const char* const>(argv_stderr, 3);
        cfg_stderr.envp = path_env;
        cfg_stderr.stdio_out = pipe_out[1];
        int pid4 = h.api.spawnp(cfg_stderr);
        check_true("sh2-spawn-stderr", pid4 > 0);
        int status4 = 0;
        check_eq("sh2-waitpid-stderr", h.api.waitpid(posix::ProcessId{pid4}, &status4, 0), pid4);
        check_eq("sh2-status-stderr", (status4 >> 8) & 0xff, 0);
        if (pipe_out[1] != 1) {
            (void)h.api.close(pipe_out[1]);
        }
        auto r4 = h.api.read(stderr_out_read, buf.data(), buf.size());
        check_true("sh2-read4-len", r4 >= 4);
        check_eq("sh2-read4-text", std::string_view{buf.data(), 4}, std::string_view{"out\n"});
        if (stderr_out_read != pipe_out[0]) {
            (void)h.api.close(stderr_out_read);
        } else {
            (void)h.api.close(pipe_out[0]);
        }

        int err_fd = h.api.open("/err.txt", posix::O_RDONLY, 0);
        check_true("sh2-err-open", err_fd >= 0);
        std::array<char, 16> err_buf{};
        auto err_read = h.api.read(err_fd, err_buf.data(), err_buf.size());
        check_true("sh2-err-read-len", err_read >= 4);
        check_eq("sh2-err-read-text", std::string_view{err_buf.data(), 4}, std::string_view{"err\n"});
        (void)h.api.close(err_fd);

        const char* argv_merge[] = {"sh", "-c", "stderr_demo > both.txt 2>&1", nullptr};
        posix::SpawnConfig cfg_merge{};
        cfg_merge.path = "sh";
        cfg_merge.argv = std::span<const char* const>(argv_merge, 3);
        cfg_merge.envp = path_env;
        int pid5 = h.api.spawnp(cfg_merge);
        check_true("sh2-spawn-merge", pid5 > 0);
        int status5 = 0;
        check_eq("sh2-waitpid-merge", h.api.waitpid(posix::ProcessId{pid5}, &status5, 0), pid5);
        check_eq("sh2-status-merge", (status5 >> 8) & 0xff, 0);

        int both_fd = h.api.open("/both.txt", posix::O_RDONLY, 0);
        check_true("sh2-both-open", both_fd >= 0);
        std::array<char, 16> both_buf{};
        auto both_read = h.api.read(both_fd, both_buf.data(), both_buf.size());
        check_true("sh2-both-read-len", both_read >= 8);
        check_eq("sh2-both-read-text", std::string_view{both_buf.data(), 8}, std::string_view{"out\nerr\n"});
        (void)h.api.close(both_fd);

        const char* argv_merge_append[] = {"sh", "-c", "stderr_demo >> both.txt 2>&1", nullptr};
        posix::SpawnConfig cfg_merge_append{};
        cfg_merge_append.path = "sh";
        cfg_merge_append.argv = std::span<const char* const>(argv_merge_append, 3);
        cfg_merge_append.envp = path_env;
        int pid6 = h.api.spawnp(cfg_merge_append);
        check_true("sh2-spawn-merge-append", pid6 > 0);
        int status6 = 0;
        check_eq("sh2-waitpid-merge-append", h.api.waitpid(posix::ProcessId{pid6}, &status6, 0), pid6);
        check_eq("sh2-status-merge-append", (status6 >> 8) & 0xff, 0);

        both_fd = h.api.open("/both.txt", posix::O_RDONLY, 0);
        check_true("sh2-both-open-append", both_fd >= 0);
        std::array<char, 24> both_append_buf{};
        auto both_append_read = h.api.read(both_fd, both_append_buf.data(), both_append_buf.size());
        check_true("sh2-both-append-read-len", both_append_read >= 16);
        check_eq("sh2-both-append-read-text", std::string_view{both_append_buf.data(), 16}, std::string_view{"out\nerr\nout\nerr\n"});
        (void)h.api.close(both_fd);

        const char* argv_sleep[] = {"sh", "-c", "sleep 2", nullptr};
        posix::SpawnConfig cfg_sleep{};
        cfg_sleep.path = "sh";
        cfg_sleep.argv = std::span<const char* const>(argv_sleep, 3);
        cfg_sleep.envp = path_env;
        const auto sleep_before = test_clock_ticks_ms();
        int pid_sleep = h.api.spawnp(cfg_sleep);
        check_true("sh2-spawn-sleep", pid_sleep > 0);
        int status_sleep = 0;
        check_eq("sh2-waitpid-sleep", h.api.waitpid(posix::ProcessId{pid_sleep}, &status_sleep, 0), pid_sleep);
        check_eq("sh2-status-sleep", (status_sleep >> 8) & 0xff, 0);
        const auto sleep_after = test_clock_ticks_ms();
        check_true("sh2-sleep-clock", sleep_after >= sleep_before + 2000);
    }

    void test_sh_c_relative_redir_uses_cwd() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("sh-cwd-mount", st);

        Harness h{};
        const auto path_env = program_path_env();
        auto reg_echo = h.procs.register_executable("/bin/echo", &echo_main);
        check_true("sh-cwd-register-echo", reg_echo);
        auto reg_sh = h.procs.register_executable("/bin/sh", &sh_main);
        check_true("sh-cwd-register-sh", reg_sh);

        check_eq("sh-cwd-mkdir-work", h.api.mkdir("/work"), 0);

        const char* argv[] = {"sh", "-c", "echo hi > out.txt", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "sh";
        cfg.argv = std::span<const char* const>(argv, 3);
        cfg.envp = path_env;
        cfg.cwd = "/work";

        int pid = h.api.spawnp(cfg);
        check_true("sh-cwd-spawn", pid > 0);
        int status = 0;
        check_eq("sh-cwd-waitpid", h.api.waitpid(posix::ProcessId{pid}, &status, 0), pid);
        check_eq("sh-cwd-status", (status >> 8) & 0xff, 0);

        int rfd = h.api.open("/work/out.txt", posix::O_RDONLY, 0);
        check_true("sh-cwd-open-work", rfd >= 0);
        std::array<char, 16> buf{};
        auto r = h.api.read(rfd, buf.data(), buf.size());
        check_true("sh-cwd-read-len", r >= 3);
        check_eq("sh-cwd-read-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
        (void)h.api.close(rfd);

        check_true("sh-cwd-root-not-created", h.api.open("/out.txt", posix::O_RDONLY, 0) < 0);
    }

    void test_busybox_relative_args_and_ls_use_cwd() noexcept {
        fs::clear_mounts();
        RamFsMount<64, 32, 64> ramfs{};
        auto st = fs::add_mount("", ramfs.mount_point());
        check_true("bb-cwd-mount", st);

        Harness h{};
        const auto path_env = program_path_env();
        auto reg_busybox = h.procs.register_executable("/bin/busybox", &busybox_main);
        check_true("bb-cwd-register-busybox", reg_busybox);

        check_eq("bb-cwd-mkdir-work", h.api.mkdir("/work"), 0);

        const char* mkdir_argv[] = {"busybox", "mkdir", "sub", nullptr};
        posix::SpawnConfig mkdir_cfg{};
        mkdir_cfg.path = "busybox";
        mkdir_cfg.argv = std::span<const char* const>(mkdir_argv, 3);
        mkdir_cfg.envp = path_env;
        mkdir_cfg.cwd = "/work";

        int mkdir_pid = h.api.spawnp(mkdir_cfg);
        check_true("bb-cwd-mkdir-spawn", mkdir_pid > 0);
        int mkdir_status = 0;
        check_eq("bb-cwd-mkdir-waitpid", h.api.waitpid(posix::ProcessId{mkdir_pid}, &mkdir_status, 0), mkdir_pid);
        check_eq("bb-cwd-mkdir-status", (mkdir_status >> 8) & 0xff, 0);

        auto* sub_dir = h.api.opendir("/work/sub");
        check_true("bb-cwd-sub-open", sub_dir != nullptr);
        if (sub_dir) {
            check_eq("bb-cwd-sub-close", h.api.closedir(sub_dir), 0);
        }

        int pipefd[2]{-1, -1};
        check_eq("bb-cwd-pipe", h.api.pipe(pipefd), 0);
        int read_fd = pipefd[0];
        if (read_fd == 0 || read_fd == 1 || read_fd == 2) {
            check_true("bb-cwd-move-read", h.fds.dup2(read_fd, 14));
            read_fd = 14;
        }

        const char* ls_argv[] = {"busybox", "ls", nullptr};
        posix::SpawnConfig ls_cfg{};
        ls_cfg.path = "busybox";
        ls_cfg.argv = std::span<const char* const>(ls_argv, 2);
        ls_cfg.envp = path_env;
        ls_cfg.cwd = "/work";
        ls_cfg.stdio_out = pipefd[1];

        int ls_pid = h.api.spawnp(ls_cfg);
        check_true("bb-cwd-ls-spawn", ls_pid > 0);
        int ls_status = 0;
        check_eq("bb-cwd-ls-waitpid", h.api.waitpid(posix::ProcessId{ls_pid}, &ls_status, 0), ls_pid);
        check_eq("bb-cwd-ls-status", (ls_status >> 8) & 0xff, 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }

        std::array<char, 64> buf{};
        auto r = h.api.read(read_fd, buf.data(), buf.size());
        check_true("bb-cwd-ls-read-len", r > 0);
        const auto text = std::string_view{buf.data(), static_cast<util::usize>(r)};
        check_true("bb-cwd-ls-has-sub", text.find("sub\n") != std::string_view::npos);
        check_true("bb-cwd-ls-not-root", text.find("work\n") == std::string_view::npos);
    }

    void test_busybox_sh_by_argv0() noexcept {
        Harness h{};
        const auto path_env = program_path_env();
        auto reg_sh = h.procs.register_executable("/bin/sh", &busybox_main);
        check_true("bb-argv0-register-sh", reg_sh);
        auto reg_echo = h.procs.register_executable("/bin/echo", &busybox_main);
        check_true("bb-argv0-register-echo", reg_echo);

        int pipefd[2]{-1, -1};
        check_eq("bb-argv0-pipe", h.api.pipe(pipefd), 0);
        int read_fd = pipefd[0];
        if (read_fd == 0 || read_fd == 1 || read_fd == 2) {
            check_true("bb-argv0-move-read", h.fds.dup2(read_fd, 9));
            read_fd = 9;
        }

        const char* argv[] = {"sh", "-c", "echo hi", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "sh";
        cfg.argv = std::span<const char* const>(argv, 3);
        cfg.envp = path_env;
        cfg.stdio_out = pipefd[1];

        int pid = h.api.spawnp(cfg);
        check_true("bb-argv0-spawn", pid > 0);
        int status = 0;
        check_eq("bb-argv0-waitpid", h.api.waitpid(posix::ProcessId{pid}, &status, 0), pid);
        check_eq("bb-argv0-status", (status >> 8) & 0xff, 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }

        std::array<char, 16> buf{};
        auto r = h.api.read(read_fd, buf.data(), buf.size());
        check_true("bb-argv0-read-len", r >= 3);
        check_eq("bb-argv0-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
    }

    void test_sh_c_ps() noexcept {
        Harness h{};
        const auto path_env = program_path_env();
        auto reg_sh = h.procs.register_executable("/bin/sh", &busybox_main);
        check_true("ps-register-sh", reg_sh);
        auto reg_ps = h.procs.register_executable("/bin/ps", &busybox_main);
        check_true("ps-register-ps", reg_ps);

        int pipefd[2]{-1, -1};
        check_eq("ps-pipe", h.api.pipe(pipefd), 0);
        int read_fd = pipefd[0];
        if (read_fd == 0 || read_fd == 1 || read_fd == 2) {
            check_true("ps-move-read", h.fds.dup2(read_fd, 11));
            read_fd = 11;
        }

        const char* argv[] = {"sh", "-c", "ps", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "sh";
        cfg.argv = std::span<const char* const>(argv, 3);
        cfg.envp = path_env;
        cfg.stdio_out = pipefd[1];

        int pid = h.api.spawnp(cfg);
        check_true("ps-spawn", pid > 0);
        int status = 0;
        check_eq("ps-waitpid", h.api.waitpid(posix::ProcessId{pid}, &status, 0), pid);
        check_eq("ps-status", (status >> 8) & 0xff, 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }

        std::array<char, 128> buf{};
        auto r = h.api.read(read_fd, buf.data(), buf.size());
        check_true("ps-read-len", r > 0);
        const auto text = std::string_view{buf.data(), static_cast<util::usize>(r)};
        check_true("ps-header", text.find("PID STATE CMD\n") == 0);
        check_true("ps-has-sh", text.find(" sh\n") != std::string_view::npos);
        check_true("ps-has-ps", text.find(" ps\n") != std::string_view::npos);
    }

    void test_sh_c_kill() noexcept {
        Harness h{};
        auto reg_sh = h.procs.register_executable("/bin/sh", &busybox_main);
        check_true("sh-kill-register-sh", reg_sh);
        auto reg_kill = h.procs.register_executable("/bin/kill", &busybox_main);
        check_true("sh-kill-register-kill", reg_kill);
        auto reg_target = h.procs.register_executable("kill-target", &kill_tool_target_main);
        check_true("sh-kill-register-target", reg_target);

        KillToolHookCtx ctx{};
        ctx.api = &h.api;
        ctx.kind = KillHelperKind::shell;
        h.procs.bind_process_hooks(&kill_tool_on_enter, &kill_tool_on_exit, &ctx);

        g_kill_tool_target_runs = 0;
        const char* argv[] = {"kill-target", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "kill-target";
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.path_mode = posix::PathMode::exact;

        auto spawn = h.procs.spawn(cfg);
        check_true("sh-kill-spawn", spawn);
        check_true("sh-kill-hook-fired", ctx.fired);
        check_true("sh-kill-helper-spawn", ctx.helper_pid > 0);
        check_eq("sh-kill-helper-wait", ctx.helper_wait_rc, ctx.helper_pid);
        check_eq("sh-kill-helper-status", (ctx.helper_status >> 8) & 0xff, 0);
        check_eq("sh-kill-target-not-run", g_kill_tool_target_runs, 0);

        auto st = h.procs.waitpid(spawn.value().pid, 0);
        check_true("sh-kill-wait", st);
        check_eq("sh-kill-wait-kind", st.value().kind, posix::WaitKind::signaled);
        check_eq("sh-kill-wait-code", st.value().code, posix::SIGTERM);
    }

    void test_busybox_sh_via_busybox() noexcept {
        Harness h{};
        const auto path_env = program_path_env();
        auto reg_busybox = h.procs.register_executable("/bin/busybox", &busybox_main);
        check_true("bb-dispatch-register-busybox", reg_busybox);
        auto reg_echo = h.procs.register_executable("/bin/echo", &busybox_main);
        check_true("bb-dispatch-register-echo", reg_echo);

        int pipefd[2]{-1, -1};
        check_eq("bb-dispatch-pipe", h.api.pipe(pipefd), 0);
        int read_fd = pipefd[0];
        if (read_fd == 0 || read_fd == 1 || read_fd == 2) {
            check_true("bb-dispatch-move-read", h.fds.dup2(read_fd, 10));
            read_fd = 10;
        }

        const char* argv[] = {"busybox", "sh", "-c", "echo hi", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "busybox";
        cfg.argv = std::span<const char* const>(argv, 4);
        cfg.envp = path_env;
        cfg.stdio_out = pipefd[1];

        int pid = h.api.spawnp(cfg);
        check_true("bb-dispatch-spawn", pid > 0);
        int status = 0;
        check_eq("bb-dispatch-waitpid", h.api.waitpid(posix::ProcessId{pid}, &status, 0), pid);
        check_eq("bb-dispatch-status", (status >> 8) & 0xff, 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }

        std::array<char, 16> buf{};
        auto r = h.api.read(read_fd, buf.data(), buf.size());
        check_true("bb-dispatch-read-len", r >= 3);
        check_eq("bb-dispatch-text", std::string_view{buf.data(), 3}, std::string_view{"hi\n"});
    }

    void test_busybox_ps_via_busybox() noexcept {
        Harness h{};
        const auto path_env = program_path_env();
        auto reg_busybox = h.procs.register_executable("/bin/busybox", &busybox_main);
        check_true("bb-ps-register-busybox", reg_busybox);

        int pipefd[2]{-1, -1};
        check_eq("bb-ps-pipe", h.api.pipe(pipefd), 0);
        int read_fd = pipefd[0];
        if (read_fd == 0 || read_fd == 1 || read_fd == 2) {
            check_true("bb-ps-move-read", h.fds.dup2(read_fd, 12));
            read_fd = 12;
        }

        const char* argv[] = {"busybox", "ps", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "busybox";
        cfg.argv = std::span<const char* const>(argv, 2);
        cfg.envp = path_env;
        cfg.stdio_out = pipefd[1];

        int pid = h.api.spawnp(cfg);
        check_true("bb-ps-spawn", pid > 0);
        int status = 0;
        check_eq("bb-ps-waitpid", h.api.waitpid(posix::ProcessId{pid}, &status, 0), pid);
        check_eq("bb-ps-status", (status >> 8) & 0xff, 0);
        if (pipefd[1] != 1) {
            (void)h.api.close(pipefd[1]);
        }

        std::array<char, 128> buf{};
        auto r = h.api.read(read_fd, buf.data(), buf.size());
        check_true("bb-ps-read-len", r > 0);
        const auto text = std::string_view{buf.data(), static_cast<util::usize>(r)};
        check_true("bb-ps-header", text.find("PID STATE CMD\n") == 0);
        check_true("bb-ps-has-busybox", text.find(" busybox\n") != std::string_view::npos);
    }

    void test_busybox_sleep_via_busybox() noexcept {
        Harness h{};
        const auto path_env = program_path_env();
        auto reg_busybox = h.procs.register_executable("/bin/busybox", &busybox_main);
        check_true("bb-sleep-register-busybox", reg_busybox);

        const char* argv[] = {"busybox", "sleep", "2", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "busybox";
        cfg.argv = std::span<const char* const>(argv, 3);
        cfg.envp = path_env;

        const auto sleep_before = test_clock_ticks_ms();
        int pid = h.api.spawnp(cfg);
        check_true("bb-sleep-spawn", pid > 0);
        int status = 0;
        check_eq("bb-sleep-waitpid", h.api.waitpid(posix::ProcessId{pid}, &status, 0), pid);
        check_eq("bb-sleep-status", (status >> 8) & 0xff, 0);
        const auto sleep_after = test_clock_ticks_ms();
        check_true("bb-sleep-clock", sleep_after >= sleep_before + 2000);
    }

    void test_busybox_kill_via_busybox() noexcept {
        Harness h{};
        auto reg_busybox = h.procs.register_executable("/bin/busybox", &busybox_main);
        check_true("bb-kill-register-busybox", reg_busybox);
        auto reg_target = h.procs.register_executable("kill-target", &kill_tool_target_main);
        check_true("bb-kill-register-target", reg_target);

        KillToolHookCtx ctx{};
        ctx.api = &h.api;
        ctx.kind = KillHelperKind::busybox;
        h.procs.bind_process_hooks(&kill_tool_on_enter, &kill_tool_on_exit, &ctx);

        g_kill_tool_target_runs = 0;
        const char* argv[] = {"kill-target", nullptr};
        posix::SpawnConfig cfg{};
        cfg.path = "kill-target";
        cfg.argv = std::span<const char* const>(argv, 1);
        cfg.path_mode = posix::PathMode::exact;

        auto spawn = h.procs.spawn(cfg);
        check_true("bb-kill-spawn", spawn);
        check_true("bb-kill-hook-fired", ctx.fired);
        check_true("bb-kill-helper-spawn", ctx.helper_pid > 0);
        check_eq("bb-kill-helper-wait", ctx.helper_wait_rc, ctx.helper_pid);
        check_eq("bb-kill-helper-status", (ctx.helper_status >> 8) & 0xff, 0);
        check_eq("bb-kill-target-not-run", g_kill_tool_target_runs, 0);

        auto st = h.procs.waitpid(spawn.value().pid, 0);
        check_true("bb-kill-wait", st);
        check_eq("bb-kill-wait-kind", st.value().kind, posix::WaitKind::signaled);
        check_eq("bb-kill-wait-code", st.value().code, posix::SIGTERM);
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
    log_line("[posix-smoke] programs phase sh-cwd begin");
    test_sh_c_relative_redir_uses_cwd();
    log_line("[posix-smoke] programs phase sh-cwd end");
    log_line("[posix-smoke] programs phase sh-ps begin");
    test_sh_c_ps();
    log_line("[posix-smoke] programs phase sh-ps end");
    log_line("[posix-smoke] programs phase sh-kill begin");
    test_sh_c_kill();
    log_line("[posix-smoke] programs phase sh-kill end");
    log_line("[posix-smoke] programs phase busybox-argv0 begin");
    test_busybox_sh_by_argv0();
    log_line("[posix-smoke] programs phase busybox-argv0 end");
    log_line("[posix-smoke] programs phase busybox-dispatch begin");
    test_busybox_relative_args_and_ls_use_cwd();
    test_busybox_sh_via_busybox();
    test_busybox_ps_via_busybox();
    test_busybox_sleep_via_busybox();
    test_busybox_kill_via_busybox();
    log_line("[posix-smoke] programs phase busybox-dispatch end");
}

#endif
