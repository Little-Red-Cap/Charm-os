module;

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

export module posix.proc;

import init.node;
import posix.env;
import util.core;
import util.error;

export namespace posix {
    struct ProcessId { int value{-1}; };

    enum class PathMode : util::u8 {
        exact,
        search_path,
    };

    struct FileAction {
        enum class Kind { open, close, dup2 };
        Kind kind{};
        int fd{-1};
        int newfd{-1};
        const char* path{nullptr};
        int flags{0};
        int mode{0};
    };

    template <util::usize MaxActions>
    struct FileActions {
        std::array<FileAction, MaxActions> actions{};
        util::usize count{0};

        bool add_open(int fd, const char* path, int flags, int mode = 0) noexcept {
            if (count >= MaxActions) return false;
            actions[count++] = FileAction{FileAction::Kind::open, fd, -1, path, flags, mode};
            return true;
        }
        bool add_close(int fd) noexcept {
            if (count >= MaxActions) return false;
            actions[count++] = FileAction{FileAction::Kind::close, fd, -1, nullptr, 0, 0};
            return true;
        }
        bool add_dup2(int fd, int newfd) noexcept {
            if (count >= MaxActions) return false;
            actions[count++] = FileAction{FileAction::Kind::dup2, fd, newfd, nullptr, 0, 0};
            return true;
        }
    };

    struct SpawnConfig {
        const char* path{nullptr};
        std::span<const char* const> argv{};
        std::span<const char* const> envp{};
        const char* cwd{nullptr};
        const FileActions<16>* file_actions{nullptr};
        int stdio_in{-1};
        int stdio_out{-1};
        int stdio_err{-1};
        PathMode path_mode{PathMode::exact};
    };

    struct SpawnResult { ProcessId pid{}; };

    enum class WaitKind : util::u8 {
        exited,
        signaled,
        stopped,
        continued
    };

    struct WaitStatus {
        ProcessId pid{};
        WaitKind kind{WaitKind::exited};
        int code{0};
    };

    using MainEntry = int (*)(int, char**);

    struct ExecEntry {
        std::string_view name{};
        MainEntry entry{nullptr};
    };

    template <util::usize MaxProcs, util::usize MaxExecs, util::usize MaxPathLen = 128>
    class ProcService {
    public:
        void init() noexcept {
            for (auto& p : procs_) p = {};
            for (auto& used : used_) used = false;
            exec_count_ = 0;
            next_pid_ = 1;
        }

        util::Result<void> register_executable(std::string_view name, MainEntry entry) noexcept {
            if (name.empty() || entry == nullptr) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < exec_count_; ++i) {
                if (execs_[i].name == name) {
                    return util::unexpected(util::Errc::exist);
                }
            }
            if (exec_count_ >= MaxExecs) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            execs_[exec_count_++] = ExecEntry{name, entry};
            return {};
        }

        util::Result<SpawnResult> spawn(const SpawnConfig& cfg) noexcept {
            if (cfg.path == nullptr && cfg.argv.empty()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (cfg.cwd != nullptr && cfg.cwd[0] != '\0') {
                return util::unexpected(util::Errc::not_supported);
            }
            if (cfg.file_actions && cfg.file_actions->count > 0) {
                return util::unexpected(util::Errc::not_supported);
            }
            if (cfg.stdio_in >= 0 || cfg.stdio_out >= 0 || cfg.stdio_err >= 0) {
                return util::unexpected(util::Errc::not_supported);
            }

            std::string_view name = resolve_name(cfg);
            auto* entry = find_entry(name);
            if (!entry) {
                return util::unexpected(util::Errc::noent);
            }

            const auto slot = alloc_slot();
            if (slot < 0) {
                return util::unexpected(util::Errc::buffer_overflow);
            }

            const int pid = next_pid_++;
            Process& proc = procs_[static_cast<util::usize>(slot)];
            proc.pid.value = pid;
            proc.exited = false;
            proc.exit_code = 0;

            int argc = static_cast<int>(cfg.argv.size());
            char** argv = const_cast<char**>(cfg.argv.data());
            const int code = entry->entry(argc, argv);

            proc.exit_code = code;
            proc.exited = true;
            return SpawnResult{ProcessId{pid}};
        }

        util::Result<WaitStatus> waitpid(ProcessId pid, int) noexcept {
            auto* proc = find_proc(pid);
            if (!proc) {
                return util::unexpected(util::Errc::noent);
            }
            if (!proc->exited) {
                return util::unexpected(util::Errc::would_block);
            }
            WaitStatus st{};
            st.pid = pid;
            st.kind = WaitKind::exited;
            st.code = proc->exit_code;
            release_proc(*proc);
            return st;
        }

    private:
        struct Process {
            ProcessId pid{};
            bool exited{false};
            int exit_code{0};
        };

        ExecEntry* find_entry(std::string_view name) noexcept {
            for (util::usize i = 0; i < exec_count_; ++i) {
                if (execs_[i].name == name) return &execs_[i];
            }
            return nullptr;
        }

        std::string_view resolve_name(const SpawnConfig& cfg) noexcept {
            const std::string_view path = cfg.path ? std::string_view{cfg.path} : std::string_view{};
            const std::string_view argv0 =
                (!cfg.argv.empty() && cfg.argv[0] != nullptr) ? std::string_view{cfg.argv[0]} : std::string_view{};

            if (cfg.path_mode == PathMode::exact) {
                return path;
            }

            const std::string_view target = !path.empty() ? path : argv0;
            if (target.empty()) return {};

            const auto path_list = envp_path(cfg.envp);
            if (path_list.empty()) {
                return target;
            }

            resolved_path_[0] = '\0';
            bool found = for_each_path_candidate<MaxPathLen>(path_list, target,
                [&](std::string_view candidate) noexcept {
                    if (find_entry(candidate) != nullptr) {
                        const util::usize n = candidate.size() < (MaxPathLen - 1)
                            ? candidate.size()
                            : (MaxPathLen - 1);
                        for (util::usize i = 0; i < n; ++i) {
                            resolved_path_[i] = candidate[i];
                        }
                        resolved_path_[n] = '\0';
                        return true;
                    }
                    return false;
                });
            if (found) {
                return std::string_view{resolved_path_};
            }
            return target;
        }

        static std::string_view envp_path(std::span<const char* const> envp) noexcept {
            return envp_get(envp, kPathKey);
        }

        int alloc_slot() noexcept {
            for (util::usize i = 0; i < MaxProcs; ++i) {
                if (!used_[i]) {
                    used_[i] = true;
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        Process* find_proc(ProcessId pid) noexcept {
            for (util::usize i = 0; i < MaxProcs; ++i) {
                if (!used_[i]) continue;
                if (procs_[i].pid.value == pid.value) return &procs_[i];
            }
            return nullptr;
        }

        void release_proc(Process& proc) noexcept {
            for (util::usize i = 0; i < MaxProcs; ++i) {
                if (!used_[i]) continue;
                if (&procs_[i] != &proc) continue;
                procs_[i] = {};
                used_[i] = false;
                return;
            }
        }

        std::array<ExecEntry, MaxExecs> execs_{};
        util::usize exec_count_{0};
        std::array<Process, MaxProcs> procs_{};
        std::array<bool, MaxProcs> used_{};
        int next_pid_{1};
        std::array<char, MaxPathLen> resolved_path_{};
    };

    template <util::usize MaxProcs, util::usize MaxExecs, util::usize MaxPathLen = 128>
    struct ProcServiceBinding {
        ProcService<MaxProcs, MaxExecs, MaxPathLen>* service{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit ProcServiceBinding(ProcService<MaxProcs, MaxExecs, MaxPathLen>& proc_service,
                                    const char* cap_name = "posix.proc",
                                    init::Phase phase = init::Phase::core,
                                    util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : service(&proc_service) {
            provides[0] = init::cap_id(cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                {},
                &ProcServiceBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ProcServiceBinding*>(ctx);
            if (!self || !self->service) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->service->init();
            return {};
        }
    };
}
