module;

#include <array>
#include <cstddef>
#include <csetjmp>
#include <span>
#include <string_view>
#ifdef errno
#undef errno
#endif

export module posix.proc;

export import posix.proc_types;

import init.node;
import posix.env;
import posix.image_resolver;
import posix.exec_source;
import posix.exec_loader;
import posix.elf_hostcall;
import posix.exec_context;
import posix.errno;
import posix.program_catalog;
import posix.fd_table;
import posix.file;
import posix.spawn_fds;
import posix.program_image;
import posix.program_image_modulex;
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
            return elf_mem_registry_.register_image(name, data, size);
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
            exec_catalog_.reset();
            elf_mem_registry_.reset();
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
            return exec_catalog_.register_registered_image(name, entry);
        }

        util::Result<void> register_image(const ProgramImage& image) noexcept {
            return exec_catalog_.register_image(image);
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
            return posix::resolve_program_image(*this, cfg, exec_catalog_, elf_mem_registry_, resolved_path_);
        }
        util::Result<SpawnResult> spawn(const SpawnConfig& cfg) noexcept {
            return spawn(cfg, fd_table_);
        }

        util::Result<SpawnResult> spawn(const SpawnConfig& cfg, FdTableType* parent_fd_table) noexcept {
            if (cfg.path == nullptr && cfg.argv.empty()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (cfg.cwd != nullptr && cfg.cwd[0] != '\0') {
                return util::unexpected(util::Errc::not_supported);
            }
            auto child_table = posix::build_spawn_fd_table(parent_fd_table, file_service_, cfg);
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
                proc.fds.close_all();
                release_proc(proc);
                return util::unexpected(code.error());
            }
            proc.exit_code = code.value();
            proc.exited = true;
            proc.fds.close_all();
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

        FdTableType* fd_table_by_pid_value(int pid_value) noexcept {
            return fd_table(ProcessId{pid_value});
        }

        bool elf_exec_enabled() const noexcept { return elf_exec_enabled_; }
        bool elf_hostcalls_enabled() const noexcept { return elf_hostcalls_enabled_; }
        bool has_exec_file_service() const noexcept { return file_service_ != nullptr; }
        util::usize exec_path_capacity() const noexcept { return MaxPathLen; }
        util::u8* elf_image_buffer() noexcept { return elf_image_.data(); }
        util::usize elf_image_capacity() const noexcept { return elf_image_.size(); }
        void* elf_load_base_ptr() noexcept { return elf_load_base(); }
        util::usize elf_load_capacity() const noexcept { return elf_load_size(); }
        ImageEntry elf_exec_stub_entry() const noexcept { return elf_exec_stub_; }
        util::Result<void> apply_elf_hostcalls() noexcept { return install_elf_hostcalls(); }
        util::Result<FdEntry> open_exec_file(std::string_view path, int flags, int mode = 0) noexcept {
            if (!file_service_) {
                return util::unexpected(util::Errc::bad_state);
            }
            return file_service_->open(path, flags, mode);
        }

        void close_exec_entry(const FdEntry& entry) noexcept {
            if (entry.ops && entry.ops->close) {
                (void)entry.ops->close(entry.ctx);
            }
        }

    private:
        struct Process {
            ProcessId pid{};
            bool exited{false};
            int exit_code{0};
            FdTableType fds{};
        };

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
            ExecContext exec_ctx{};
            exec_ctx.owner = this;
            exec_ctx.pid_value = pid.value;
            push_exec_context(exec_ctx);
            if (on_enter_) {
                on_enter_(pid, hook_ctx_);
            }
            struct Guard {
                ProcHook hook;
                ProcessId pid;
                void* hook_ctx;
                ExecContext* exec_ctx;
                ~Guard() noexcept {
                    if (exec_ctx) pop_exec_context(*exec_ctx);
                    if (hook) hook(pid, hook_ctx);
                }
            } exit_guard{on_exit_, pid, hook_ctx_, &exec_ctx};
            ExecJumpBuffer exit_jmp{};
            exec_ctx.exit_jmp = &exit_jmp.storage;
            exec_ctx.jump_ready = true;
            const int jump_rc = setjmp(exit_jmp.storage);
            if (jump_rc != 0) {
                return finalize_exec_exit(exec_ctx, exec_ctx.exit_code);
            }
            if (image.entry) {
                const int rc = image.entry(argv_envp.value().argc, argv_envp.value().argv, argv_envp.value().envp);
                return finalize_exec_exit(exec_ctx, rc);
            }
            if (image.entry_v0) {
                const int rc = image.entry_v0(argv_envp.value().argc, argv_envp.value().argv);
                return finalize_exec_exit(exec_ctx, rc);
            }
            return util::unexpected(util::Errc::invalid_arg);
        }

        util::Result<void> install_elf_hostcalls() noexcept {
            return posix::install_elf_hostcalls<ProcService>(*this, elf_load_base(), elf_load_size());
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

        ProgramCatalog<MaxExecs> exec_catalog_{};
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
        ElfMemRegistry<8> elf_mem_registry_{};
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
