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
export import posix.user_crt;
export import posix.user_crt_c;
export import posix.user_runtime;
export import charm.system.clock;
export import charm.system.time;
export import module_core;
export import fs_core;
export import fs_errno;
export import fs_ramfs;
export import fs_stream;
export import fs_vfs;
export import util.core;
export import util.error;

extern "C" int charm_posix_c_header_probe_entry(void);
    extern "C" int charm_posix_c_header_exit_entry(void);
    extern "C" int charm_posix_newlib_syscall_probe_entry(void);
    extern "C" int charm_posix_newlib_dup_entry(void);
    extern "C" int charm_posix_newlib_dup2_entry(void);
    extern "C" int charm_posix_newlib_kill_self_entry(void);
    extern "C" int charm_posix_newlib_lseek_entry(void);
extern "C" int charm_posix_newlib_path_entry(void);
extern "C" int charm_posix_newlib_cwd_entry(void);
#if defined(CHARM_POSIX_NEWLIB_STDIO_SMOKE) && CHARM_POSIX_NEWLIB_STDIO_SMOKE
extern "C" int charm_posix_newlib_stdio_entry(void);
#endif

export namespace posix::testsupport {
#if defined(POSIX_SMOKE_USE_UART) && POSIX_SMOKE_USE_UART
    extern "C" void posix_smoke_emit(const char* msg) noexcept;
#endif
    using ProcServiceType = posix::ProcService<4, 8, 16, 16, 128, 16, 16, 256, 64 * 1024, 8192>;
    using ApiType = posix::Api<16, 8, 64, 4, 8, 16, 128, 16, 16, 256, 64 * 1024, 8192>;

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

    inline constexpr char kProgramPathEntry[] = "PATH=/bin:/usr/bin";
    inline constexpr std::array<const char*, 1> kProgramPathEnv{kProgramPathEntry};

    inline std::span<const char* const> program_path_env() noexcept {
        return std::span<const char* const>(kProgramPathEnv.data(), kProgramPathEnv.size());
    }

    struct TestClockState {
        charm::system::ClockTick ticks_ms{0};
    };

    inline TestClockState& shared_test_clock_state() noexcept {
        static TestClockState state{};
        return state;
    }

    inline charm::system::ClockTick test_clock_now_ms(void* ctx) noexcept {
        auto* state = static_cast<TestClockState*>(ctx);
        if (!state) return 0;
        return state->ticks_ms++;
    }

    inline charm::system::ClockTick test_clock_now_us(void* ctx) noexcept {
        return test_clock_now_ms(ctx) * 1000u;
    }

    inline void bind_test_clock() noexcept {
        auto& state = shared_test_clock_state();
        static charm::system::Clock clock{&state, {.now_ms = &test_clock_now_ms, .now_us = &test_clock_now_us}};
        state.ticks_ms = 0;
        charm::system::time::bind(clock);
    }

    inline charm::system::ClockTick test_clock_ticks_ms() noexcept {
        return shared_test_clock_state().ticks_ms;
    }

    inline int write_text(int fd, std::string_view text) noexcept {
        if (!posix::user::has_runtime()) return 1;
        auto w = posix::user::write(fd, text.data(), text.size());
        if (w != static_cast<posix::ssize_t>(text.size())) return 2;
        return 0;
    }

    template <util::usize N>
    bool copy_path_arg(std::string_view in, std::array<char, N>& out) noexcept {
        if (in.empty() || N < 2) return false;
        if (in.size() >= out.size()) {
            return false;
        }
        util::usize pos = 0;
        for (char ch : in) {
            out[pos++] = ch;
        }
        out[pos] = '\0';
        return true;
    }

    inline std::string_view trim_ascii_spaces(std::string_view text) noexcept {
        while (!text.empty() && text.front() == ' ') {
            text.remove_prefix(1);
        }
        while (!text.empty() && text.back() == ' ') {
            text.remove_suffix(1);
        }
        return text;
    }

    template <util::usize MaxWords>
    int split_shell_words(std::string_view in,
                          std::array<std::string_view, MaxWords>& out) noexcept {
        int count = 0;
        util::usize pos = 0;
        while (pos < in.size()) {
            while (pos < in.size() && in[pos] == ' ') {
                ++pos;
            }
            if (pos >= in.size()) {
                break;
            }
            const auto start = pos;
            while (pos < in.size() && in[pos] != ' ') {
                ++pos;
            }
            if (count >= static_cast<int>(out.size())) {
                return -1;
            }
            out[static_cast<util::usize>(count++)] = in.substr(start, pos - start);
        }
        return count;
    }

    int hello_main(int, char**, char**) {
        return write_text(1, "hello\n");
    }

    int argv_dump_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        for (int i = 0; i < argc; ++i) {
            const char* arg = argv && argv[i] ? argv[i] : "";
            char buf[128]{};
            std::snprintf(buf, sizeof(buf), "argv[%d]=%s\n", i, arg);
            auto w = posix::user::write(1, buf, std::char_traits<char>::length(buf));
            if (w < 0) return 2;
        }
        return 0;
    }

    int crt_probe_main(int argc, char** argv, char** envp) {
        if (!posix::user::has_runtime()) return 11;
        if (!posix::user::has_startup_context()) return 12;
        if (posix::user::argc() != argc) return 13;
        if (posix::user::argv() != argv) return 14;
        if (posix::user::envp() != envp) return 15;
        if (!argv || !argv[0] || std::string_view{argv[0]} != "crt_probe") return 16;
        if (std::string_view{posix::user::argv0()} != "crt_probe") return 17;
        if (!argv[1] || std::string_view{posix::user::arg(1)} != "alpha") return 18;
        if (envp == nullptr || envp[0] == nullptr) return 19;
        if (std::string_view{posix::user::env_entry(0)} != "FOO=BAR") return 20;
        if (posix::user::getenv("FOO") != std::string_view{"BAR"}) return 21;
        if (posix::user::getenv("BAR") != std::string_view{"BAZ"}) return 22;
        if (posix::user::environ() != envp) return 23;
        const char* foo_value = posix::user::getenv_cstr("FOO");
        if (!foo_value || std::string_view{foo_value} != "BAR") return 24;
        return write_text(1, "crt-ok\n");
    }

    int crt_exit_main(int, char**, char**) {
        if (write_text(1, "before-exit\n") != 0) return 31;
        posix::user::exit(23);
    }

    int crt_errno_main(int, char**, char**) {
        char ch{};
        if (posix::user::read(-1, &ch, 1) != -1) return 41;
        auto* err = posix::user::errno_location();
        if (!err) return 42;
        if (*err != posix::EBADF) return 43;
        return write_text(1, "errno-ok\n");
    }

    int crt_c_probe_main(int argc, char**, char**) {
        if (charm_posix_argc() != argc) return 51;
        auto** argv = charm_posix_argv();
        auto** envp = charm_posix_envp();
        if (!argv || !envp) return 52;
        if (!argv[0] || std::string_view{argv[0]} != "crt_c_probe") return 53;
        if (!argv[1] || std::string_view{argv[1]} != "beta") return 54;
        if (charm_posix_environ() != envp) return 55;
        const char* foo = charm_posix_getenv("FOO");
        if (!foo || std::string_view{foo} != "BAR") return 56;
        char ch{};
        if (charm_posix_read(-1, &ch, 1) != -1) return 57;
        auto* err = charm_posix_errno_location();
        if (!err || *err != posix::EBADF) return 58;
        if (charm_posix_getpid() <= 0) return 59;
        return charm_posix_write(1, "crt-c-ok\n", 9) == 9 ? 0 : 60;
    }

    int crt_c_exit_main(int, char**, char**) {
        if (charm_posix_write(1, "crt-c-exit\n", 11) != 11) return 61;
        charm_posix_exit(29);
    }

    int crt_c_header_probe_main(int, char**, char**) {
        return charm_posix_c_header_probe_entry();
    }

    int crt_c_header_exit_main(int, char**, char**) {
        return charm_posix_c_header_exit_entry();
    }

    int newlib_syscall_probe_main(int, char**, char**) {
        return charm_posix_newlib_syscall_probe_entry();
    }

    int newlib_dup_main(int, char**, char**) {
        return charm_posix_newlib_dup_entry();
    }

    int newlib_dup2_main(int, char**, char**) {
        return charm_posix_newlib_dup2_entry();
    }

    int newlib_kill_self_main(int, char**, char**) {
        return charm_posix_newlib_kill_self_entry();
    }

    int newlib_lseek_main(int, char**, char**) {
        return charm_posix_newlib_lseek_entry();
    }

    int newlib_path_main(int, char**, char**) {
        return charm_posix_newlib_path_entry();
    }

    int newlib_cwd_main(int, char**, char**) {
        return charm_posix_newlib_cwd_entry();
    }

#if defined(CHARM_POSIX_NEWLIB_STDIO_SMOKE) && CHARM_POSIX_NEWLIB_STDIO_SMOKE
    int newlib_stdio_main(int, char**, char**) {
        return charm_posix_newlib_stdio_entry();
    }
#endif

    int stderr_demo_main(int, char**, char**) {
        const int r1 = write_text(1, "out\n");
        const int r2 = write_text(2, "err\n");
        return r1 == 0 && r2 == 0 ? 0 : 3;
    }

    int sleep_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        if (argc < 2 || !argv || !argv[1]) return 2;
        unsigned seconds = 0;
        for (const char* p = argv[1]; p && *p; ++p) {
            if (*p < '0' || *p > '9') return 2;
            seconds = seconds * 10u + static_cast<unsigned>(*p - '0');
        }
        return posix::user::sleep(seconds) == 0 ? 0 : 4;
    }

    bool parse_decimal_arg(std::string_view text, int& value) noexcept {
        if (text.empty()) return false;
        int parsed = 0;
        for (char ch : text) {
            if (ch < '0' || ch > '9') return false;
            parsed = parsed * 10 + static_cast<int>(ch - '0');
        }
        value = parsed;
        return true;
    }

    bool parse_signal_arg(std::string_view text, int& sig) noexcept {
        if (text.empty()) return false;
        if (text[0] == '-') {
            text.remove_prefix(1);
        }
        if (text == "2" || text == "INT" || text == "SIGINT") {
            sig = posix::SIGINT;
            return true;
        }
        if (text == "9" || text == "KILL" || text == "SIGKILL") {
            sig = posix::SIGKILL;
            return true;
        }
        if (text == "15" || text == "TERM" || text == "SIGTERM") {
            sig = posix::SIGTERM;
            return true;
        }
        return false;
    }

    int kill_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        if (argc < 2 || !argv || !argv[1]) return 2;
        int sig = posix::SIGTERM;
        int pid_index = 1;
        if (argv[1][0] == '-') {
            if (!parse_signal_arg(std::string_view{argv[1]}, sig)) return 2;
            pid_index = 2;
        }
        if (argc <= pid_index || !argv[pid_index]) return 2;
        if (argc > pid_index + 1 && argv[pid_index + 1]) return 2;
        int pid = -1;
        if (!parse_decimal_arg(std::string_view{argv[pid_index]}, pid)) return 2;
        return posix::user::kill(pid, sig) == 0 ? 0 : 4;
    }

    int ps_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        if (argc > 1 && argv && argv[1]) return 2;
        std::array<posix::ProcessSnapshot, 8> entries{};
        const int count = posix::user::list_processes(
            std::span<posix::ProcessSnapshot>(entries.data(), entries.size()));
        if (count < 0) return 3;
        if (write_text(1, "PID STATE CMD\n") != 0) return 4;
        for (int i = 0; i < count; ++i) {
            const auto state = entries[static_cast<util::usize>(i)].state == posix::ProcessState::running ? 'R' : 'Z';
            char line[96]{};
            std::snprintf(line,
                          sizeof(line),
                          "%d %c %s\n",
                          entries[static_cast<util::usize>(i)].pid.value,
                          state,
                          entries[static_cast<util::usize>(i)].name.data());
            if (write_text(1, line) != 0) return 4;
        }
        return 0;
    }

    int exit_code_main(int argc, char** argv, char**) {
        if (argc < 2 || !argv || !argv[1]) return 0;
        int value = 0;
        for (const char* p = argv[1]; p && *p; ++p) {
            if (*p < '0' || *p > '9') break;
            value = value * 10 + (*p - '0');
        }
        return value;
    }

    int modulex_entry_main(int, char**, char**) {
        return write_text(1, "mx\n");
    }

    int elf_entry_main(int argc, char** argv, char** envp) {
        return modulex_entry_main(argc, argv, envp);
    }

    bool resolve_modulex_entry(std::string_view name, modulex::Addr& out_addr) noexcept {
        if (name == "entry") {
            out_addr = modulex::to_addr(reinterpret_cast<const void*>(&modulex_entry_main));
            return true;
        }
        return false;
    }

    int echo_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        if (argc > 1 && argv && argv[1]) {
            if (write_text(1, std::string_view{argv[1]}) != 0) return 2;
        }
        return write_text(1, "\n");
    }

    int cat_main(int, char**, char**) {
        if (!posix::user::has_runtime()) return 1;
        std::array<char, 16> buf{};
        while (true) {
            auto r = posix::user::read(0, buf.data(), buf.size());
            if (r == 0) break;
            if (r < 0) return 2;
            auto w = posix::user::write(1, buf.data(), static_cast<util::usize>(r));
            if (w != r) return 3;
        }
        return 0;
    }

    int pwd_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        if (argc > 1 && argv && argv[1]) return 2;
        std::array<char, 64> cwd_buf{};
        if (!posix::user::getcwd(cwd_buf.data(), cwd_buf.size())) return 3;
        if (write_text(1, std::string_view{cwd_buf.data()}) != 0) return 4;
        return write_text(1, "\n");
    }

    int sh_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        if (argc < 3 || !argv || !argv[1] || !argv[2]) return 2;
        if (std::string_view{argv[1]} != "-c") return 2;
        std::string_view cmd{argv[2]};
        const auto path_env = program_path_env();

        auto spawn_command = [&](const char* file,
                                 std::span<const char* const> child_argv,
                                 int stdio_in,
                                 int stdio_out,
                                 int stdio_err) noexcept -> int {
            posix::SpawnConfig cfg{};
            cfg.path = file;
            cfg.argv = child_argv;
            cfg.envp = path_env;
            cfg.stdio_in = stdio_in;
            cfg.stdio_out = stdio_out;
            cfg.stdio_err = stdio_err;
            const int pid = posix::user::spawnp(cfg);
            if (pid < 0) return posix::get_errno();
            int status = 0;
            const int wpid = posix::user::waitpid(posix::ProcessId{pid}, &status, 0);
            if (wpid != pid) return 127;
            return (status >> 8) & 0xff;
        };

        auto spawn_echo = [&](std::string_view arg,
                              int stdio_in,
                              int stdio_out,
                              int stdio_err) noexcept -> int {
            char arg_buf[64]{};
            const auto n = arg.size() < (sizeof(arg_buf) - 1) ? arg.size() : (sizeof(arg_buf) - 1);
            for (util::usize i = 0; i < n; ++i) {
                arg_buf[i] = arg[i];
            }
            if (n == 0) {
                const char* echo_argv[] = {"echo", nullptr};
                return spawn_command("echo", std::span<const char* const>(echo_argv, 1),
                                     stdio_in, stdio_out, stdio_err);
            }
            const char* echo_argv[] = {"echo", arg_buf, nullptr};
            return spawn_command("echo", std::span<const char* const>(echo_argv, 2),
                                 stdio_in, stdio_out, stdio_err);
        };

        auto spawn_cat = [&](int stdio_in, int stdio_out, int stdio_err) noexcept -> int {
            const char* cat_argv[] = {"cat", nullptr};
            return spawn_command("cat", std::span<const char* const>(cat_argv, 1),
                                 stdio_in, stdio_out, stdio_err);
        };

        auto spawn_stderr_demo = [&](int stdio_in, int stdio_out, int stdio_err) noexcept -> int {
            const char* stderr_argv[] = {"stderr_demo", nullptr};
            return spawn_command("stderr_demo", std::span<const char* const>(stderr_argv, 1),
                                 stdio_in, stdio_out, stdio_err);
        };

        auto spawn_sleep = [&](std::string_view seconds,
                               int stdio_in,
                               int stdio_out,
                               int stdio_err) noexcept -> int {
            char sec_buf[32]{};
            const auto n = seconds.size() < (sizeof(sec_buf) - 1) ? seconds.size() : (sizeof(sec_buf) - 1);
            for (util::usize i = 0; i < n; ++i) {
                sec_buf[i] = seconds[i];
            }
            const char* sleep_argv[] = {"sleep", sec_buf, nullptr};
            return spawn_command("sleep", std::span<const char* const>(sleep_argv, 2),
                                 stdio_in, stdio_out, stdio_err);
        };

        auto spawn_ps = [&](int stdio_in, int stdio_out, int stdio_err) noexcept -> int {
            const char* ps_argv[] = {"ps", nullptr};
            return spawn_command("ps", std::span<const char* const>(ps_argv, 1),
                                 stdio_in, stdio_out, stdio_err);
        };

        auto spawn_kill = [&](std::string_view arg0,
                              std::string_view arg1,
                              bool has_signal,
                              int stdio_in,
                              int stdio_out,
                              int stdio_err) noexcept -> int {
            char sig_buf[32]{};
            char pid_buf[32]{};
            const auto sig_n = arg0.size() < (sizeof(sig_buf) - 1) ? arg0.size() : (sizeof(sig_buf) - 1);
            for (util::usize i = 0; i < sig_n; ++i) {
                sig_buf[i] = arg0[i];
            }
            const auto pid_src = has_signal ? arg1 : arg0;
            const auto pid_n = pid_src.size() < (sizeof(pid_buf) - 1) ? pid_src.size() : (sizeof(pid_buf) - 1);
            for (util::usize i = 0; i < pid_n; ++i) {
                pid_buf[i] = pid_src[i];
            }
            if (has_signal) {
                const char* kill_argv[] = {"kill", sig_buf, pid_buf, nullptr};
                return spawn_command("kill", std::span<const char* const>(kill_argv, 3),
                                     stdio_in, stdio_out, stdio_err);
            }
            const char* kill_argv[] = {"kill", pid_buf, nullptr};
            return spawn_command("kill", std::span<const char* const>(kill_argv, 2),
                                 stdio_in, stdio_out, stdio_err);
        };

        auto run_simple = [&](std::string_view raw_cmd) noexcept -> int {
            const std::string_view one_cmd = trim_ascii_spaces(raw_cmd);
            if (one_cmd.empty()) return 0;

            const auto pipe_pos = one_cmd.find(" | cat");
            if (pipe_pos != std::string_view::npos) {
                const std::string_view arg = one_cmd.substr(5, pipe_pos - 5);
                int fds[2]{-1, -1};
                if (posix::user::pipe(fds) != 0) return 5;
                const int echo_rc = spawn_echo(arg, -1, fds[1], -1);
                (void)posix::user::close(fds[1]);
                if (echo_rc != 0) {
                    (void)posix::user::close(fds[0]);
                    return echo_rc;
                }
                const int cat_rc = spawn_cat(fds[0], -1, -1);
                (void)posix::user::close(fds[0]);
                return cat_rc;
            }

            std::array<std::string_view, 8> words{};
            const int word_count = split_shell_words(one_cmd, words);
            if (word_count <= 0) return 127;

            const std::string_view program = words[0];
            std::array<std::string_view, 2> arg_words{};
            int arg_count = 0;
            std::string_view input_path{};
            std::string_view output_path{};
            std::string_view err_path{};
            bool output_append = false;
            bool merge_err_to_out = false;
            bool saw_redirect = false;

            for (int i = 1; i < word_count; ++i) {
                const std::string_view token = words[static_cast<util::usize>(i)];
                if (token == "2>&1") {
                    saw_redirect = true;
                    merge_err_to_out = true;
                    err_path = {};
                    continue;
                }
                if (token == "<" || (token.size() > 1 && token[0] == '<')) {
                    saw_redirect = true;
                    if (token == "<") {
                        ++i;
                        if (i >= word_count) return 127;
                        input_path = words[static_cast<util::usize>(i)];
                    } else {
                        input_path = token.substr(1);
                    }
                    continue;
                }
                if (token == ">>" || (token.size() > 2 && token[0] == '>' && token[1] == '>')) {
                    if (merge_err_to_out) {
                        return 127;
                    }
                    saw_redirect = true;
                    output_append = true;
                    if (token == ">>") {
                        ++i;
                        if (i >= word_count) return 127;
                        output_path = words[static_cast<util::usize>(i)];
                    } else {
                        output_path = token.substr(2);
                    }
                    continue;
                }
                if (token == ">" || (token.size() > 1 && token[0] == '>')) {
                    if (merge_err_to_out) {
                        return 127;
                    }
                    saw_redirect = true;
                    output_append = false;
                    if (token == ">") {
                        ++i;
                        if (i >= word_count) return 127;
                        output_path = words[static_cast<util::usize>(i)];
                    } else {
                        output_path = token.substr(1);
                    }
                    continue;
                }
                if (token == "2>" || (token.size() > 2 && token[0] == '2' && token[1] == '>')) {
                    saw_redirect = true;
                    merge_err_to_out = false;
                    if (token == "2>") {
                        ++i;
                        if (i >= word_count) return 127;
                        err_path = words[static_cast<util::usize>(i)];
                    } else {
                        err_path = token.substr(2);
                    }
                    continue;
                }
                if (saw_redirect) {
                    return 127;
                }
                if (arg_count >= static_cast<int>(arg_words.size())) {
                    return 127;
                }
                arg_words[static_cast<util::usize>(arg_count++)] = token;
            }

            std::array<char, 64> input_buf{};
            std::array<char, 64> output_buf{};
            std::array<char, 64> err_buf{};
            std::array<char, 64> path_buf{};
            std::array<char, 64> cwd_buf{};
            int input_fd = -1;
            int output_fd = -1;
            int err_fd = -1;

            const auto close_if_open = [&](int& fd) noexcept {
                if (fd >= 0) {
                    (void)posix::user::close(fd);
                    fd = -1;
                }
            };

            if (!input_path.empty()) {
                if (!copy_path_arg(input_path, input_buf)) {
                    return 127;
                }
                input_fd = posix::user::open(input_buf.data(), posix::O_RDONLY, 0);
                if (input_fd < 0) {
                    return 9;
                }
            }
            if (!output_path.empty()) {
                if (!copy_path_arg(output_path, output_buf)) {
                    close_if_open(input_fd);
                    return 127;
                }
                const int out_flags = output_append
                    ? (posix::O_WRONLY | posix::O_CREAT | posix::O_APPEND)
                    : (posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC);
                output_fd = posix::user::open(output_buf.data(), out_flags, 0);
                if (output_fd < 0) {
                    close_if_open(input_fd);
                    return 9;
                }
            }
            if (!err_path.empty()) {
                if (!copy_path_arg(err_path, err_buf)) {
                    close_if_open(input_fd);
                    close_if_open(output_fd);
                    return 127;
                }
                err_fd = posix::user::open(err_buf.data(), posix::O_WRONLY | posix::O_CREAT | posix::O_TRUNC, 0);
                if (err_fd < 0) {
                    close_if_open(input_fd);
                    close_if_open(output_fd);
                    return 9;
                }
            }
            if (merge_err_to_out) {
                err_fd = 1;
            }

            const int out_target = output_fd >= 0 ? output_fd : 1;
            int rc = 127;
            if (program == "echo") {
                if (arg_count > 1) {
                    rc = 127;
                } else {
                    const auto arg = arg_count == 1 ? arg_words[0] : std::string_view{};
                    rc = spawn_echo(arg, input_fd, output_fd, err_fd);
                }
            } else if (program == "cat") {
                if (arg_count != 0) {
                    rc = 127;
                } else {
                    rc = spawn_cat(input_fd, output_fd, err_fd);
                }
            } else if (program == "stderr_demo") {
                if (arg_count != 0) {
                    rc = 127;
                } else {
                    rc = spawn_stderr_demo(input_fd, output_fd, err_fd);
                }
            } else if (program == "sleep") {
                if (arg_count != 1) {
                    rc = 127;
                } else {
                    rc = spawn_sleep(arg_words[0], input_fd, output_fd, err_fd);
                }
            } else if (program == "ps") {
                if (arg_count != 0) {
                    rc = 127;
                } else {
                    rc = spawn_ps(input_fd, output_fd, err_fd);
                }
            } else if (program == "kill") {
                if (arg_count == 1) {
                    rc = spawn_kill(arg_words[0], {}, false, input_fd, output_fd, err_fd);
                } else if (arg_count == 2) {
                    rc = spawn_kill(arg_words[0], arg_words[1], true, input_fd, output_fd, err_fd);
                } else {
                    rc = 127;
                }
            } else if (program == "cd") {
                if (arg_count > 1) {
                    rc = 127;
                } else {
                    const auto target = arg_count == 1 ? arg_words[0] : std::string_view{"/"};
                    if (!copy_path_arg(target, path_buf)) {
                        rc = 127;
                    } else {
                        rc = posix::user::chdir(path_buf.data()) == 0 ? 0 : posix::get_errno();
                    }
                }
            } else if (program == "pwd") {
                if (arg_count != 0) {
                    rc = 127;
                } else if (!posix::user::getcwd(cwd_buf.data(), cwd_buf.size())) {
                    rc = posix::get_errno();
                } else if (write_text(out_target, std::string_view{cwd_buf.data()}) != 0) {
                    rc = 4;
                } else {
                    rc = write_text(out_target, "\n");
                }
            }

            close_if_open(input_fd);
            close_if_open(output_fd);
            if (!merge_err_to_out) {
                close_if_open(err_fd);
            }
            return rc;
        };

        int last_rc = 0;
        while (true) {
            const auto seq_pos = cmd.find(';');
            const auto segment = seq_pos == std::string_view::npos ? cmd : cmd.substr(0, seq_pos);
            const auto one_cmd = trim_ascii_spaces(segment);
            if (!one_cmd.empty()) {
                last_rc = run_simple(one_cmd);
            }
            if (seq_pos == std::string_view::npos) {
                break;
            }
            cmd.remove_prefix(seq_pos + 1);
        }
        return last_rc;
    }

    int mkdir_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        if (argc < 2 || !argv || !argv[1]) return 2;
        std::array<char, 64> path_buf{};
        if (!copy_path_arg(std::string_view{argv[1]}, path_buf)) return 3;
        return posix::user::mkdir(path_buf.data()) == 0 ? 0 : 4;
    }

    int rm_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        if (argc < 2 || !argv || !argv[1]) return 2;
        std::array<char, 64> path_buf{};
        if (!copy_path_arg(std::string_view{argv[1]}, path_buf)) return 3;
        return posix::user::unlink(path_buf.data()) == 0 ? 0 : 4;
    }

    int mv_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        if (argc < 3 || !argv || !argv[1] || !argv[2]) return 2;
        std::array<char, 64> from_buf{};
        std::array<char, 64> to_buf{};
        if (!copy_path_arg(std::string_view{argv[1]}, from_buf)) return 3;
        if (!copy_path_arg(std::string_view{argv[2]}, to_buf)) return 3;
        return posix::user::rename(from_buf.data(), to_buf.data()) == 0 ? 0 : 4;
    }

    int ls_main(int argc, char** argv, char**) {
        if (!posix::user::has_runtime()) return 1;
        std::array<char, 64> path_buf{};
        const char* path = ".";
        if (argc > 1 && argv && argv[1]) {
            if (!copy_path_arg(std::string_view{argv[1]}, path_buf)) return 2;
            path = path_buf.data();
        }
        auto* dir = posix::user::opendir(path);
        if (!dir) return 3;
        posix::set_errno(0);
        while (const auto* ent = posix::user::readdir(dir)) {
            const std::string_view name{ent->d_name.data()};
            if (write_text(1, name) != 0) {
                (void)posix::user::closedir(dir);
                return 4;
            }
            if (write_text(1, "\n") != 0) {
                (void)posix::user::closedir(dir);
                return 4;
            }
        }
        const int err = posix::get_errno();
        if (posix::user::closedir(dir) != 0) return 5;
        return err == 0 ? 0 : 6;
    }

    inline std::string_view command_basename(std::string_view name) noexcept {
        const auto pos = name.find_last_of('/');
        return pos == std::string_view::npos ? name : name.substr(pos + 1);
    }

    int busybox_main(int argc, char** argv, char** envp) {
        if (argc < 1 || !argv || !argv[0]) return 127;

        auto dispatch_applet = [&](std::string_view applet_name,
                                   int applet_argc,
                                   char** applet_argv,
                                   char** applet_envp) noexcept -> int {
            if (applet_name == "echo") {
                return echo_main(applet_argc, applet_argv, applet_envp);
            }
            if (applet_name == "cat") {
                return cat_main(applet_argc, applet_argv, applet_envp);
            }
            if (applet_name == "sh") {
                return sh_main(applet_argc, applet_argv, applet_envp);
            }
            if (applet_name == "mkdir") {
                return mkdir_main(applet_argc, applet_argv, applet_envp);
            }
            if (applet_name == "rm") {
                return rm_main(applet_argc, applet_argv, applet_envp);
            }
            if (applet_name == "mv") {
                return mv_main(applet_argc, applet_argv, applet_envp);
            }
            if (applet_name == "ls") {
                return ls_main(applet_argc, applet_argv, applet_envp);
            }
            if (applet_name == "pwd") {
                return pwd_main(applet_argc, applet_argv, applet_envp);
            }
            if (applet_name == "sleep") {
                return sleep_main(applet_argc, applet_argv, applet_envp);
            }
            if (applet_name == "ps") {
                return ps_main(applet_argc, applet_argv, applet_envp);
            }
            if (applet_name == "kill") {
                return kill_main(applet_argc, applet_argv, applet_envp);
            }
            return 127;
        };

        const auto argv0_name = command_basename(std::string_view{argv[0]});
        if (argv0_name != "busybox") {
            return dispatch_applet(argv0_name, argc, argv, envp);
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
        return dispatch_applet(applet_name, shifted_argc, shifted_argv.data(), envp);
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
        posix::user::ProcessBinding<ApiType> runtime_binding;

        Harness() : api(fds, files, pipes, procs), runtime_binding(api) {
            bind_test_clock();
            fds.init();
            files.init();
            pipes.init();
            procs.init();
            procs.bind_fd_table(fds);
            procs.bind_file_service(files);
            posix::user::bind_process_runtime(procs, runtime_binding);
        }
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
