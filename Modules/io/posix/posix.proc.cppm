module;

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

export module posix.proc;

import init.node;
import posix.env;
import posix.fd_table;
import posix.file;
import posix.program_image;
import posix.program_image_modulex;
import posix.program_image_elf;
import module_core;
import util.core;
import util.error;

#if defined(POSIX_TEST_BUILD) && POSIX_TEST_BUILD
namespace posix::detail {
    inline constexpr util::usize kElfTestLoadSize = 8192;
    alignas(16) __attribute__((section(".elf_load")))
    std::array<util::u8, kElfTestLoadSize> g_elf_test_load{};
}
#endif

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

    struct ExecEntry {
        ProgramImage image{};
    };

    template <util::usize MaxProcs,
              util::usize MaxExecs,
              util::usize MaxFds,
              util::usize MaxFiles,
              util::usize MaxPathLen = 128,
              util::usize MaxArgc = 16,
              util::usize MaxEnvp = 16,
              util::usize MaxArgBytes = 256,
              util::usize MaxElfImage = 4096,
              util::usize MaxElfLoad = 4096>
    class ProcService {
    public:
        using FdTableType = FdTable<MaxFds>;
        using ProcHook = void (*)(ProcessId pid, void* ctx) noexcept;
        void enable_elf_exec(bool enabled) noexcept { elf_exec_enabled_ = enabled; }
        void enable_elf_hostcalls(bool enabled) noexcept { elf_hostcalls_enabled_ = enabled; }
        util::Result<void> register_elf_mem(std::string_view name,
                                            const void* data,
                                            util::usize size) noexcept {
            if (name.empty() || !data || size == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < elf_mem_count_; ++i) {
                if (elf_mem_[i].name.compare(name) == 0) {
                    return util::unexpected(util::Errc::exist);
                }
            }
            if (elf_mem_count_ >= elf_mem_.size()) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            elf_mem_[elf_mem_count_++] = ElfMemImage{
                name, static_cast<const util::u8*>(data), size
            };
            return {};
        }
        void set_elf_exec_stub(ImageEntry stub) noexcept {
            #if defined(POSIX_TEST_BUILD) && POSIX_TEST_BUILD
            elf_exec_stub_ = stub;
            #else
            (void)stub;
            #endif
        }

        void init() noexcept {
            for (auto& p : procs_) p = {};
            for (auto& used : used_) used = false;
            exec_count_ = 0;
            next_pid_ = 1;
        }

        void bind_fd_table(FdTable<MaxFds>& table) noexcept {
            fd_table_ = &table;
        }

        void bind_file_service(FileService<MaxFiles>& file_service) noexcept {
            file_service_ = &file_service;
        }

        void bind_process_hooks(ProcHook on_enter, ProcHook on_exit, void* ctx = nullptr) noexcept {
            on_enter_ = on_enter;
            on_exit_ = on_exit;
            hook_ctx_ = ctx;
        }

        util::Result<void> register_executable(std::string_view name, ImageEntryV0 entry) noexcept {
            if (name.empty() || entry == nullptr) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < exec_count_; ++i) {
                if (execs_[i].image.name.compare(name) == 0) {
                    return util::unexpected(util::Errc::exist);
                }
            }
            if (exec_count_ >= MaxExecs) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            execs_[exec_count_++] = ExecEntry{make_registered_image(name, entry)};
            return {};
        }

        util::Result<void> register_image(const ProgramImage& image) noexcept {
            if (image.name.empty()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < exec_count_; ++i) {
                if (execs_[i].image.name.compare(image.name) == 0) {
                    return util::unexpected(util::Errc::exist);
                }
            }
            if (exec_count_ >= MaxExecs) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            execs_[exec_count_++] = ExecEntry{image};
            return {};
        }

        util::Result<void> register_modulex_image(std::string_view name,
                                                  const modulex::ImageHeader* header,
                                                  const ModuleXLoadConfig& cfg) noexcept {
            auto image = load_modulex_image(name, header, cfg);
            if (!image) {
                return util::unexpected(image.error());
            }
            return register_image(image.value());
        }

        util::Result<ProgramImage> load_image(const SpawnConfig& cfg) noexcept {
            if (is_elf_mem_prefixed(cfg)) {
                auto mem_name = resolve_elf_mem_name(cfg);
                if (mem_name.empty()) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                const auto* mem = find_elf_mem(mem_name);
                if (!mem) {
                    return util::unexpected(util::Errc::noent);
                }
                return load_elf_from_buffer(mem->data, mem->size);
            }
            if (is_elf_prefixed(cfg)) {
                auto elf_path = resolve_elf_path(cfg);
                if (elf_path.empty()) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                return load_elf_from_file(elf_path);
            }
            std::string_view name = resolve_name(cfg);
            auto* entry = find_entry(name);
            if (!entry && cfg.path_mode == PathMode::search_path) {
                entry = find_entry_by_argv0(cfg.argv);
            }
            if (!entry) {
                auto elf_candidate = load_elf_candidate(cfg);
                if (elf_candidate) {
                    return elf_candidate;
                }
                if (elf_candidate.error() == util::Errc::noent ||
                    elf_candidate.error() == util::Errc::not_supported) {
                    return util::unexpected(util::Errc::noent);
                }
                return util::unexpected(elf_candidate.error());
            }
            return entry->image;
        }

        util::Result<SpawnResult> spawn(const SpawnConfig& cfg) noexcept {
            if (cfg.path == nullptr && cfg.argv.empty()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (cfg.cwd != nullptr && cfg.cwd[0] != '\0') {
                return util::unexpected(util::Errc::not_supported);
            }
            auto child_table = build_child_fd_table(cfg);
            if (!child_table) {
                return util::unexpected(child_table.error());
            }

            auto image = load_image(cfg);
            if (!image) {
                return util::unexpected(image.error());
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
            proc.fds = child_table.value();

            const auto code = start_image(proc.pid, image.value(), cfg);
            if (!code) {
                release_proc(proc);
                return util::unexpected(code.error());
            }
            proc.exit_code = code.value();
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

        FdTableType* fd_table(ProcessId pid) noexcept {
            auto* proc = find_proc(pid);
            if (!proc) return nullptr;
            return &proc->fds;
        }

    private:
        struct Process {
            ProcessId pid{};
            bool exited{false};
            int exit_code{0};
            FdTableType fds{};
        };

        util::Result<FdTableType> build_child_fd_table(const SpawnConfig& cfg) noexcept {
            if (!fd_table_) {
                return util::unexpected(util::Errc::not_supported);
            }
            FdTableType child{};
            child.init();
            auto rc = fd_table_->clone_to(child);
            if (!rc) {
                return util::unexpected(rc.error());
            }

            if (cfg.stdio_in >= 0) {
                auto r = child.dup2(cfg.stdio_in, 0);
                if (!r) return util::unexpected(r.error());
            }
            if (cfg.stdio_out >= 0) {
                auto r = child.dup2(cfg.stdio_out, 1);
                if (!r) return util::unexpected(r.error());
            }
            if (cfg.stdio_err >= 0) {
                auto r = child.dup2(cfg.stdio_err, 2);
                if (!r) return util::unexpected(r.error());
            }

            if (cfg.file_actions && cfg.file_actions->count > 0) {
                for (util::usize i = 0; i < cfg.file_actions->count; ++i) {
                    const auto& act = cfg.file_actions->actions[i];
                    switch (act.kind) {
                        case FileAction::Kind::open: {
                            if (!act.path || act.fd < 0) {
                                return util::unexpected(util::Errc::invalid_arg);
                            }
                            if (!file_service_) {
                                return util::unexpected(util::Errc::not_supported);
                            }
                            auto entry = file_service_->open(std::string_view{act.path}, act.flags, act.mode);
                            if (!entry) {
                                return util::unexpected(entry.error());
                            }
                            if (auto existing = child.get(act.fd)) {
                                auto r = child.close(act.fd);
                                if (!r) return util::unexpected(r.error());
                            }
                            auto r = child.attach(entry.value(), act.fd);
                            if (!r) return util::unexpected(r.error());
                            break;
                        }
                        case FileAction::Kind::close: {
                            auto r = child.close(act.fd);
                            if (!r) return util::unexpected(r.error());
                            break;
                        }
                        case FileAction::Kind::dup2: {
                            auto r = child.dup2(act.fd, act.newfd);
                            if (!r) return util::unexpected(r.error());
                            break;
                        }
                    }
                }
            }

            return child;
        }

        static bool match_exec_name(std::string_view exec_name, std::string_view query) noexcept {
            if (exec_name.compare(query) == 0) return true;
            if (query.empty()) return false;
            if (exec_name.size() <= query.size()) return false;
            const auto suffix_pos = exec_name.size() - query.size();
            if (suffix_pos == 0) return false;
            if (exec_name[suffix_pos - 1] != '/') return false;
            return exec_name.substr(suffix_pos).compare(query) == 0;
        }

        ExecEntry* find_entry(std::string_view name) noexcept {
            for (util::usize i = 0; i < exec_count_; ++i) {
                if (match_exec_name(execs_[i].image.name, name)) return &execs_[i];
            }
            return nullptr;
        }

        ExecEntry* find_entry_by_argv0(std::span<const char* const> argv) noexcept {
            if (argv.empty() || argv[0] == nullptr) return nullptr;
            const std::string_view name{argv[0]};
            for (util::usize i = 0; i < exec_count_; ++i) {
                if (match_exec_name(execs_[i].image.name, name)) return &execs_[i];
            }
            return nullptr;
        }

        std::string_view resolve_name(const SpawnConfig& cfg) noexcept {
            const std::string_view path = cfg.path ? std::string_view{cfg.path} : std::string_view{};
            const std::string_view argv0 =
                (!cfg.argv.empty() && cfg.argv[0] != nullptr) ? std::string_view{cfg.argv[0]} : std::string_view{};

            if (cfg.path_mode == PathMode::exact) {
                return strip_modulex_prefix(path);
            }

            const std::string_view target = !path.empty() ? strip_modulex_prefix(path)
                                                          : strip_modulex_prefix(argv0);
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

        static std::string_view strip_modulex_prefix(std::string_view name) noexcept {
            constexpr std::string_view kPrefix = "modulex:";
            if (name.size() >= kPrefix.size() && name.substr(0, kPrefix.size()) == kPrefix) {
                return name.substr(kPrefix.size());
            }
            return name;
        }

        static std::string_view strip_elf_prefix(std::string_view name) noexcept {
            constexpr std::string_view kPrefix = "elf:";
            if (name.size() >= kPrefix.size() && name.substr(0, kPrefix.size()) == kPrefix) {
                return name.substr(kPrefix.size());
            }
            return name;
        }

        static bool is_elf_prefix(std::string_view name) noexcept {
            constexpr std::string_view kPrefix = "elf:";
            return name.size() >= kPrefix.size() && name.substr(0, kPrefix.size()) == kPrefix;
        }

        static bool is_elf_prefixed(const SpawnConfig& cfg) noexcept {
            if (cfg.path && is_elf_prefix(std::string_view{cfg.path})) return true;
            if (!cfg.argv.empty() && cfg.argv[0] != nullptr &&
                is_elf_prefix(std::string_view{cfg.argv[0]})) {
                return true;
            }
            return false;
        }

        static std::string_view strip_elf_mem_prefix(std::string_view name) noexcept {
            constexpr std::string_view kPrefix = "elfmem:";
            if (name.size() >= kPrefix.size() && name.substr(0, kPrefix.size()) == kPrefix) {
                return name.substr(kPrefix.size());
            }
            return name;
        }

        static bool is_elf_mem_prefix(std::string_view name) noexcept {
            constexpr std::string_view kPrefix = "elfmem:";
            return name.size() >= kPrefix.size() && name.substr(0, kPrefix.size()) == kPrefix;
        }

        static bool is_elf_mem_prefixed(const SpawnConfig& cfg) noexcept {
            if (cfg.path && is_elf_mem_prefix(std::string_view{cfg.path})) return true;
            if (!cfg.argv.empty() && cfg.argv[0] != nullptr &&
                is_elf_mem_prefix(std::string_view{cfg.argv[0]})) {
                return true;
            }
            return false;
        }

        static std::string_view resolve_elf_mem_name(const SpawnConfig& cfg) noexcept {
            if (cfg.path && cfg.path[0] != '\0') {
                return strip_elf_mem_prefix(std::string_view{cfg.path});
            }
            if (!cfg.argv.empty() && cfg.argv[0] != nullptr) {
                return strip_elf_mem_prefix(std::string_view{cfg.argv[0]});
            }
            return {};
        }

        static std::string_view resolve_elf_path(const SpawnConfig& cfg) noexcept {
            if (cfg.path && cfg.path[0] != '\0') {
                return strip_elf_prefix(std::string_view{cfg.path});
            }
            if (!cfg.argv.empty() && cfg.argv[0] != nullptr) {
                return strip_elf_prefix(std::string_view{cfg.argv[0]});
            }
            return {};
        }

        static std::string_view resolve_exec_path(const SpawnConfig& cfg) noexcept {
            if (cfg.path && cfg.path[0] != '\0') {
                return std::string_view{cfg.path};
            }
            if (!cfg.argv.empty() && cfg.argv[0] != nullptr) {
                return std::string_view{cfg.argv[0]};
            }
            return {};
        }

        util::Result<ProgramImage> load_elf_from_buffer(const util::u8* base,
                                                        util::usize size) noexcept {
            if (!base || size == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            ElfLoadConfig elf_cfg{};
            elf_cfg.image_base = base;
            elf_cfg.image_size = size;
            elf_cfg.load_base = elf_load_base();
            elf_cfg.load_size = elf_load_size();
            elf_cfg.load_align = 16;
            auto image = load_elf_image(elf_cfg);
            if (image && elf_hostcalls_enabled_) {
                auto host_ok = install_elf_hostcalls();
                if (!host_ok) {
                    return util::unexpected(host_ok.error());
                }
            }
            #if defined(POSIX_TEST_BUILD) && POSIX_TEST_BUILD
            if (image && elf_exec_enabled_ && elf_exec_stub_) {
                image.value().entry = elf_exec_stub_;
            }
            #else
            (void)elf_exec_stub_;
            #endif
            return image;
        }

        util::Result<ProgramImage> load_elf_from_file(std::string_view path) noexcept {
            if (path.empty()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (!file_service_) {
                return util::unexpected(util::Errc::not_supported);
            }
            if (path.size() >= MaxPathLen) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            auto entry = file_service_->open(path, O_RDONLY, 0);
            if (!entry) {
                return util::unexpected(entry.error());
            }
            const auto close_guard = [&]() noexcept {
                if (entry.value().ops) {
                    (void)entry.value().ops->close(entry.value().ctx);
                }
            };
            bool size_known = false;
            util::usize file_size = 0;
            if (entry.value().ops && entry.value().ops->stat) {
                PosixStat st{};
                auto rst = entry.value().ops->stat(entry.value().ctx, st);
                if (!rst) {
                    close_guard();
                    return util::unexpected(rst.error());
                }
                size_known = true;
                file_size = static_cast<util::usize>(st.size);
                if (file_size > elf_image_.size()) {
                    close_guard();
                    return util::unexpected(util::Errc::buffer_overflow);
                }
            }
            util::usize total = 0;
            while (total < elf_image_.size()) {
                const auto view = MutByteView{elf_image_.data() + total, elf_image_.size() - total};
                auto r = entry.value().ops->read(entry.value().ctx, view);
                if (!r) {
                    close_guard();
                    return util::unexpected(r.error());
                }
                if (r.value() == 0) break;
                total += r.value();
            }
            close_guard();
            if (!size_known && total == elf_image_.size()) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            if (size_known && total < file_size) {
                return util::unexpected(util::Errc::io);
            }
            return load_elf_from_buffer(elf_image_.data(), total);
        }

        util::Result<ProgramImage> load_elf_candidate(const SpawnConfig& cfg) noexcept {
            if (!elf_exec_enabled_) {
                return util::unexpected(util::Errc::not_supported);
            }
            if (!file_service_) {
                return util::unexpected(util::Errc::not_supported);
            }
            const auto path = resolve_exec_path(cfg);
            if (path.empty()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (is_elf_prefix(path) || is_elf_mem_prefix(path) ||
                (path.size() >= 8 && path.substr(0, 8) == "modulex:")) {
                return util::unexpected(util::Errc::noent);
            }
            if (cfg.path_mode == PathMode::exact) {
                return load_elf_from_file(path);
            }
            const auto path_list = envp_path(cfg.envp);
            util::Result<ProgramImage> out = util::unexpected(util::Errc::noent);
            const bool found = for_each_path_candidate<MaxPathLen>(path_list, path,
                [&](std::string_view candidate) noexcept {
                    auto image = load_elf_from_file(candidate);
                    if (image) {
                        out = image;
                        return true;
                    }
                    if (image.error() != util::Errc::noent) {
                        out = util::unexpected(image.error());
                        return true;
                    }
                    return false;
                });
            if (found) {
                return out;
            }
            return util::unexpected(util::Errc::noent);
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

        struct ArgvEnvpView {
            int argc{0};
            char** argv{nullptr};
            char** envp{nullptr};
        };

        struct ArgvEnvpBuffer {
            std::array<char*, MaxArgc + 1> argv{};
            std::array<char*, MaxEnvp + 1> envp{};
            std::array<char, MaxArgBytes> blob{};
        };

        util::Result<ArgvEnvpView> build_argv_envp(const SpawnConfig& cfg, ArgvEnvpBuffer& buffer) noexcept {
            util::usize blob_offset = 0;
            util::usize argc = 0;
            for (const auto* arg : cfg.argv) {
                if (argc >= MaxArgc) return util::unexpected(util::Errc::buffer_overflow);
                if (!arg) break;
                const std::string_view s{arg};
                if (blob_offset + s.size() + 1 > buffer.blob.size()) {
                    return util::unexpected(util::Errc::buffer_overflow);
                }
                buffer.argv[argc++] = &buffer.blob[blob_offset];
                for (util::usize i = 0; i < s.size(); ++i) {
                    buffer.blob[blob_offset++] = s[i];
                }
                buffer.blob[blob_offset++] = '\0';
            }
            buffer.argv[argc] = nullptr;

            util::usize envc = 0;
            for (const auto* env : cfg.envp) {
                if (envc >= MaxEnvp) return util::unexpected(util::Errc::buffer_overflow);
                if (!env) break;
                const std::string_view s{env};
                if (blob_offset + s.size() + 1 > buffer.blob.size()) {
                    return util::unexpected(util::Errc::buffer_overflow);
                }
                buffer.envp[envc++] = &buffer.blob[blob_offset];
                for (util::usize i = 0; i < s.size(); ++i) {
                    buffer.blob[blob_offset++] = s[i];
                }
                buffer.blob[blob_offset++] = '\0';
            }
            buffer.envp[envc] = nullptr;

            ArgvEnvpView view{};
            view.argc = static_cast<int>(argc);
            view.argv = buffer.argv.data();
            view.envp = buffer.envp.data();
            return view;
        }

        util::Result<int> start_image(ProcessId pid, const ProgramImage& image, const SpawnConfig& cfg) noexcept {
            if (image.kind == ImageKind::elf && !elf_exec_enabled_) {
                return util::unexpected(util::Errc::not_supported);
            }
            ArgvEnvpBuffer args{};
            auto argv_envp = build_argv_envp(cfg, args);
            if (!argv_envp) {
                return util::unexpected(argv_envp.error());
            }
            const auto prev_service = elf_host_service_;
            const auto prev_pid = elf_host_pid_;
            elf_host_service_ = this;
            elf_host_pid_ = pid;
            elf_exit_requested_ = false;
            elf_exit_code_ = 0;
            if (on_enter_) {
                on_enter_(pid, hook_ctx_);
            }
            struct Guard {
                ProcHook hook;
                ProcessId pid;
                void* ctx;
                ~Guard() noexcept {
                    if (hook) hook(pid, ctx);
                }
            } exit_guard{on_exit_, pid, hook_ctx_};
            if (image.entry) {
                const int rc = image.entry(argv_envp.value().argc, argv_envp.value().argv, argv_envp.value().envp);
                const int out = elf_exit_requested_ ? elf_exit_code_ : rc;
                elf_host_service_ = prev_service;
                elf_host_pid_ = prev_pid;
                return out;
            }
            if (image.entry_v0) {
                const int rc = image.entry_v0(argv_envp.value().argc, argv_envp.value().argv);
                const int out = elf_exit_requested_ ? elf_exit_code_ : rc;
                elf_host_service_ = prev_service;
                elf_host_pid_ = prev_pid;
                return out;
            }
            elf_host_service_ = prev_service;
            elf_host_pid_ = prev_pid;
            return util::unexpected(util::Errc::invalid_arg);
        }

        struct ElfHostCalls {
            util::i32 (*write)(int fd, const void* buf, util::usize len) noexcept {nullptr};
            void (*exit)(int code) noexcept {nullptr};
        };

        struct ElfMemImage {
            std::string_view name{};
            const util::u8* data{nullptr};
            util::usize size{0};
        };

        const ElfMemImage* find_elf_mem(std::string_view name) const noexcept {
            for (util::usize i = 0; i < elf_mem_count_; ++i) {
                if (elf_mem_[i].name.compare(name) == 0) return &elf_mem_[i];
            }
            return nullptr;
        }

        static util::i32 elf_host_write(int fd, const void* buf, util::usize len) noexcept {
            if (!elf_host_service_ || !buf) return -1;
            auto* table = elf_host_service_->fd_table(elf_host_pid_);
            if (!table) return -1;
            auto entry = table->get(fd);
            if (!entry || !entry.value()->ops || !entry.value()->ops->write) return -1;
            ByteView view{static_cast<const util::u8*>(buf), len};
            auto r = entry.value()->ops->write(entry.value()->ctx, view);
            if (!r) return -1;
            return static_cast<util::i32>(r.value());
        }

        static void elf_host_exit(int code) noexcept {
            if (!elf_host_service_) return;
            elf_host_service_->elf_exit_requested_ = true;
            elf_host_service_->elf_exit_code_ = code;
        }

        util::Result<void> install_elf_hostcalls() noexcept {
            if (elf_load_size() < sizeof(ElfHostCalls)) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            auto* table = reinterpret_cast<ElfHostCalls*>(elf_load_base());
            table->write = &ProcService::elf_host_write;
            table->exit = &ProcService::elf_host_exit;
            return {};
        }

        util::u8* elf_load_base() noexcept {
#if defined(POSIX_TEST_BUILD) && POSIX_TEST_BUILD
            static_assert(MaxElfLoad <= detail::kElfTestLoadSize, "MaxElfLoad exceeds test buffer");
            return detail::g_elf_test_load.data();
#else
            return elf_load_.data();
#endif
        }

        util::usize elf_load_size() const noexcept {
#if defined(POSIX_TEST_BUILD) && POSIX_TEST_BUILD
            static_assert(MaxElfLoad <= detail::kElfTestLoadSize, "MaxElfLoad exceeds test buffer");
            return MaxElfLoad;
#else
            return elf_load_.size();
#endif
        }

        std::array<ExecEntry, MaxExecs> execs_{};
        util::usize exec_count_{0};
        std::array<Process, MaxProcs> procs_{};
        std::array<bool, MaxProcs> used_{};
        FdTable<MaxFds>* fd_table_{nullptr};
        FileService<MaxFiles>* file_service_{nullptr};
        int next_pid_{1};
        std::array<char, MaxPathLen> resolved_path_{};
        std::array<util::u8, MaxElfImage> elf_image_{};
#if !defined(POSIX_TEST_BUILD) || !POSIX_TEST_BUILD
        alignas(16) std::array<util::u8, MaxElfLoad> elf_load_{};
#endif
        ProcHook on_enter_{nullptr};
        ProcHook on_exit_{nullptr};
        void* hook_ctx_{nullptr};
        bool elf_exec_enabled_{false};
        ImageEntry elf_exec_stub_{nullptr};
        bool elf_hostcalls_enabled_{false};
        bool elf_exit_requested_{false};
        int elf_exit_code_{0};
        static inline ProcService* elf_host_service_{nullptr};
        static inline ProcessId elf_host_pid_{};
        std::array<ElfMemImage, 8> elf_mem_{};
        util::usize elf_mem_count_{0};
    };

    template <util::usize MaxProcs,
              util::usize MaxExecs,
              util::usize MaxFds,
              util::usize MaxFiles,
              util::usize MaxPathLen = 128,
              util::usize MaxArgc = 16,
              util::usize MaxEnvp = 16,
              util::usize MaxArgBytes = 256,
              util::usize MaxElfImage = 4096,
              util::usize MaxElfLoad = 4096>
    struct ProcServiceBinding {
        ProcService<MaxProcs, MaxExecs, MaxFds, MaxFiles, MaxPathLen, MaxArgc, MaxEnvp, MaxArgBytes,
                    MaxElfImage, MaxElfLoad>* service{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit ProcServiceBinding(ProcService<MaxProcs, MaxExecs, MaxFds, MaxFiles, MaxPathLen, MaxArgc, MaxEnvp,
                                    MaxArgBytes, MaxElfImage, MaxElfLoad>& proc_service,
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
