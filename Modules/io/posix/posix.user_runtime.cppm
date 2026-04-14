module;

#include <array>
#include <cstddef>
#include <span>

#ifdef errno
#undef errno
#endif

export module posix.user_runtime;

export import posix.api;
export import posix.errno;
export import posix.fd_table;
export import posix.proc_types;
export import posix.user_context;

import posix.exec_context;
import util.core;
import util.error;

namespace posix::user::detail {
    inline constexpr util::usize kRuntimeStackDepth = 32;

    template <class Api>
    ssize_t runtime_read(void* ctx, int fd, void* buf, util::usize count) noexcept {
        return static_cast<Api*>(ctx)->read(fd, buf, count);
    }

    template <class Api>
    ssize_t runtime_write(void* ctx, int fd, const void* buf, util::usize count) noexcept {
        return static_cast<Api*>(ctx)->write(fd, buf, count);
    }

    template <class Api>
    int runtime_fstat(void* ctx, int fd, PosixStat* out) noexcept {
        return static_cast<Api*>(ctx)->fstat(fd, out);
    }

    template <class Api>
    int runtime_stat(void* ctx, const char* path, PosixStat* out) noexcept {
        return static_cast<Api*>(ctx)->stat(path, out);
    }

    template <class Api>
    int runtime_isatty(void* ctx, int fd) noexcept {
        return static_cast<Api*>(ctx)->isatty(fd);
    }

    template <class Api>
    ssize_t runtime_lseek(void* ctx, int fd, util::i64 offset, int whence) noexcept {
        return static_cast<Api*>(ctx)->lseek(fd, offset, whence);
    }

    template <class Api>
    int runtime_open(void* ctx, const char* path, int flags, int mode) noexcept {
        return static_cast<Api*>(ctx)->open(path, flags, mode);
    }

    template <class Api>
    int runtime_close(void* ctx, int fd) noexcept {
        return static_cast<Api*>(ctx)->close(fd);
    }

    template <class Api>
    int runtime_dup(void* ctx, int fd) noexcept {
        return static_cast<Api*>(ctx)->dup(fd);
    }

    template <class Api>
    int runtime_dup2(void* ctx, int from, int to) noexcept {
        return static_cast<Api*>(ctx)->dup2(from, to);
    }

    template <class Api>
    int runtime_fcntl(void* ctx, int fd, int cmd, int arg) noexcept {
        return static_cast<Api*>(ctx)->fcntl(fd, cmd, arg);
    }

    template <class Api>
    int runtime_pipe(void* ctx, int fds[2]) noexcept {
        return static_cast<Api*>(ctx)->pipe(fds);
    }

    template <class Api>
    int runtime_spawn(void* ctx, SpawnConfig cfg) noexcept {
        return static_cast<Api*>(ctx)->spawn(cfg);
    }

    template <class Api>
    int runtime_spawnp(void* ctx, SpawnConfig cfg) noexcept {
        return static_cast<Api*>(ctx)->spawnp(cfg);
    }

    template <class Api>
    int runtime_waitpid(void* ctx, ProcessId pid, int* status, int options) noexcept {
        return static_cast<Api*>(ctx)->waitpid(pid, status, options);
    }

    template <class Api>
    int runtime_kill(void* ctx, int pid, int sig) noexcept {
        return static_cast<Api*>(ctx)->kill(pid, sig);
    }

    template <class Api>
    int runtime_list_processes(void* ctx, std::span<ProcessSnapshot> out) noexcept {
        return static_cast<Api*>(ctx)->list_processes(out);
    }

    template <class Api>
    int runtime_sleep(void* ctx, unsigned seconds) noexcept {
        return static_cast<Api*>(ctx)->sleep(seconds);
    }

    template <class Api>
    int runtime_mkdir(void* ctx, const char* path) noexcept {
        return static_cast<Api*>(ctx)->mkdir(path);
    }

    template <class Api>
    int runtime_unlink(void* ctx, const char* path) noexcept {
        return static_cast<Api*>(ctx)->unlink(path);
    }

    template <class Api>
    int runtime_rmdir(void* ctx, const char* path) noexcept {
        if constexpr (requires(Api& api, const char* p) { api.rmdir(p); }) {
            return static_cast<Api*>(ctx)->rmdir(path);
        } else {
            (void)ctx;
            (void)path;
            posix::set_errno(posix::to_errno(util::Errc::nosys));
            return -1;
        }
    }

    template <class Api>
    int runtime_rename(void* ctx, const char* from, const char* to) noexcept {
        return static_cast<Api*>(ctx)->rename(from, to);
    }

    template <class Api>
    PosixDir* runtime_opendir(void* ctx, const char* path) noexcept {
        return static_cast<Api*>(ctx)->opendir(path);
    }

    template <class Api>
    const PosixDirent* runtime_readdir(void* ctx, PosixDir* dir) noexcept {
        return static_cast<Api*>(ctx)->readdir(dir);
    }

    template <class Api>
    int runtime_closedir(void* ctx, PosixDir* dir) noexcept {
        return static_cast<Api*>(ctx)->closedir(dir);
    }

    template <class Api>
    int runtime_getpid(void* ctx) noexcept {
        return static_cast<Api*>(ctx)->getpid();
    }

    template <class Api>
    int runtime_chdir(void* ctx, const char* path) noexcept {
        if constexpr (requires(Api& api, const char* p) { api.chdir(p); }) {
            return static_cast<Api*>(ctx)->chdir(path);
        } else {
            (void)ctx;
            (void)path;
            posix::set_errno(posix::to_errno(util::Errc::nosys));
            return -1;
        }
    }

    template <class Api>
    char* runtime_getcwd(void* ctx, char* buf, util::usize size) noexcept {
        if constexpr (requires(Api& api, char* p, util::usize n) { api.getcwd(p, n); }) {
            return static_cast<Api*>(ctx)->getcwd(buf, size);
        } else {
            (void)ctx;
            (void)buf;
            (void)size;
            posix::set_errno(posix::to_errno(util::Errc::nosys));
            return nullptr;
        }
    }

    template <class Ret>
    Ret missing_runtime(Ret value) noexcept {
        posix::set_errno(posix::to_errno(util::Errc::nosys));
        if (auto* context = posix::active_exec_context()) {
            context->errno_value = posix::get_errno();
        }
        return value;
    }

    template <class Fn>
    auto invoke_with_errno_sync(Fn&& fn) noexcept(noexcept(fn())) -> decltype(fn()) {
        posix::set_errno(0);
        auto result = fn();
        if (auto* context = posix::active_exec_context()) {
            context->errno_value = posix::get_errno();
        }
        return result;
    }

    struct RuntimeStorage;
}

export namespace posix::user {
    struct Runtime {
        void* ctx{nullptr};
        ssize_t (*read)(void* ctx, int fd, void* buf, util::usize count) noexcept {nullptr};
        ssize_t (*write)(void* ctx, int fd, const void* buf, util::usize count) noexcept {nullptr};
        int (*fstat)(void* ctx, int fd, PosixStat* out) noexcept {nullptr};
        int (*stat)(void* ctx, const char* path, PosixStat* out) noexcept {nullptr};
        int (*isatty)(void* ctx, int fd) noexcept {nullptr};
        ssize_t (*lseek)(void* ctx, int fd, util::i64 offset, int whence) noexcept {nullptr};
        int (*open)(void* ctx, const char* path, int flags, int mode) noexcept {nullptr};
        int (*close)(void* ctx, int fd) noexcept {nullptr};
        int (*dup)(void* ctx, int fd) noexcept {nullptr};
        int (*dup2)(void* ctx, int from, int to) noexcept {nullptr};
        int (*fcntl)(void* ctx, int fd, int cmd, int arg) noexcept {nullptr};
        int (*pipe)(void* ctx, int fds[2]) noexcept {nullptr};
        int (*spawn)(void* ctx, SpawnConfig cfg) noexcept {nullptr};
        int (*spawnp)(void* ctx, SpawnConfig cfg) noexcept {nullptr};
        int (*waitpid)(void* ctx, ProcessId pid, int* status, int options) noexcept {nullptr};
        int (*kill)(void* ctx, int pid, int sig) noexcept {nullptr};
        int (*list_processes)(void* ctx, std::span<ProcessSnapshot> out) noexcept {nullptr};
        int (*sleep)(void* ctx, unsigned seconds) noexcept {nullptr};
        int (*mkdir)(void* ctx, const char* path) noexcept {nullptr};
        int (*unlink)(void* ctx, const char* path) noexcept {nullptr};
        int (*rmdir)(void* ctx, const char* path) noexcept {nullptr};
        int (*rename)(void* ctx, const char* from, const char* to) noexcept {nullptr};
        PosixDir* (*opendir)(void* ctx, const char* path) noexcept {nullptr};
        const PosixDirent* (*readdir)(void* ctx, PosixDir* dir) noexcept {nullptr};
        int (*closedir)(void* ctx, PosixDir* dir) noexcept {nullptr};
        int (*getpid)(void* ctx) noexcept {nullptr};
        int (*chdir)(void* ctx, const char* path) noexcept {nullptr};
        char* (*getcwd)(void* ctx, char* buf, util::usize size) noexcept {nullptr};
    };

    template <class Api>
    Runtime make_runtime(Api& api) noexcept {
        Runtime runtime{};
        runtime.ctx = &api;
        runtime.read = &detail::runtime_read<Api>;
        runtime.write = &detail::runtime_write<Api>;
        runtime.fstat = &detail::runtime_fstat<Api>;
        runtime.stat = &detail::runtime_stat<Api>;
        runtime.isatty = &detail::runtime_isatty<Api>;
        runtime.lseek = &detail::runtime_lseek<Api>;
        runtime.open = &detail::runtime_open<Api>;
        runtime.close = &detail::runtime_close<Api>;
        runtime.dup = &detail::runtime_dup<Api>;
        runtime.dup2 = &detail::runtime_dup2<Api>;
        runtime.fcntl = &detail::runtime_fcntl<Api>;
        runtime.pipe = &detail::runtime_pipe<Api>;
        runtime.spawn = &detail::runtime_spawn<Api>;
        runtime.spawnp = &detail::runtime_spawnp<Api>;
        runtime.waitpid = &detail::runtime_waitpid<Api>;
        runtime.kill = &detail::runtime_kill<Api>;
        runtime.list_processes = &detail::runtime_list_processes<Api>;
        runtime.sleep = &detail::runtime_sleep<Api>;
        runtime.mkdir = &detail::runtime_mkdir<Api>;
        runtime.unlink = &detail::runtime_unlink<Api>;
        runtime.rmdir = &detail::runtime_rmdir<Api>;
        runtime.rename = &detail::runtime_rename<Api>;
        runtime.opendir = &detail::runtime_opendir<Api>;
        runtime.readdir = &detail::runtime_readdir<Api>;
        runtime.closedir = &detail::runtime_closedir<Api>;
        runtime.getpid = &detail::runtime_getpid<Api>;
        runtime.chdir = &detail::runtime_chdir<Api>;
        runtime.getcwd = &detail::runtime_getcwd<Api>;
        return runtime;
    }

    template <class Api>
    struct ProcessBinding {
        Api* api{nullptr};
        Runtime runtime{};

        explicit ProcessBinding(Api& bound_api) noexcept
            : api(&bound_api), runtime(make_runtime(bound_api)) {}
    };

    void bind_runtime(const Runtime& runtime) noexcept;
    void unbind_runtime() noexcept;
    const Runtime* active_runtime() noexcept;
    bool has_runtime() noexcept;

    template <class Api>
    void process_enter(ProcessId pid, void* ctx) noexcept {
        auto* binding = static_cast<ProcessBinding<Api>*>(ctx);
        if (!binding) {
            return;
        }
        if (binding->api) {
            binding->api->push_process(pid);
        }
        bind_runtime(binding->runtime);
    }

    template <class Api>
    void process_exit(ProcessId, void* ctx) noexcept {
        auto* binding = static_cast<ProcessBinding<Api>*>(ctx);
        if (!binding) {
            return;
        }
        unbind_runtime();
        if (binding->api) {
            binding->api->pop_process();
        }
    }

    template <class ProcService, class Api>
    void bind_process_runtime(ProcService& proc_service, ProcessBinding<Api>& binding) noexcept {
        proc_service.bind_process_runtime_hooks(&process_enter<Api>, &process_exit<Api>, &binding);
    }

    inline ssize_t read(int fd, void* buf, util::usize count) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->read) {
            return detail::missing_runtime<ssize_t>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->read(runtime->ctx, fd, buf, count);
        });
    }

    inline ssize_t write(int fd, const void* buf, util::usize count) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->write) {
            return detail::missing_runtime<ssize_t>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->write(runtime->ctx, fd, buf, count);
        });
    }

    inline int fstat(int fd, PosixStat* out) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->fstat) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->fstat(runtime->ctx, fd, out);
        });
    }

    inline int stat(const char* path, PosixStat* out) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->stat) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->stat(runtime->ctx, path, out);
        });
    }

    inline int isatty(int fd) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->isatty) {
            return detail::missing_runtime<int>(0);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->isatty(runtime->ctx, fd);
        });
    }

    inline ssize_t lseek(int fd, util::i64 offset, int whence) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->lseek) {
            return detail::missing_runtime<ssize_t>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->lseek(runtime->ctx, fd, offset, whence);
        });
    }

    inline int open(const char* path, int flags, int mode = 0) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->open) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->open(runtime->ctx, path, flags, mode);
        });
    }

    inline int close(int fd) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->close) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->close(runtime->ctx, fd);
        });
    }

    inline int dup(int fd) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->dup) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->dup(runtime->ctx, fd);
        });
    }

    inline int dup2(int from, int to) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->dup2) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->dup2(runtime->ctx, from, to);
        });
    }

    inline int fcntl(int fd, int cmd, int arg = 0) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->fcntl) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->fcntl(runtime->ctx, fd, cmd, arg);
        });
    }

    inline int pipe(int fds[2]) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->pipe) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->pipe(runtime->ctx, fds);
        });
    }

    inline int spawn(SpawnConfig cfg) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->spawn) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->spawn(runtime->ctx, cfg);
        });
    }

    inline int spawnp(SpawnConfig cfg) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->spawnp) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->spawnp(runtime->ctx, cfg);
        });
    }

    inline int waitpid(ProcessId pid, int* status, int options = 0) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->waitpid) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->waitpid(runtime->ctx, pid, status, options);
        });
    }

    inline int waitpid(int pid, int* status, int options = 0) noexcept {
        return waitpid(ProcessId{pid}, status, options);
    }

    inline int kill(ProcessId pid, int sig) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->kill) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->kill(runtime->ctx, pid.value, sig);
        });
    }

    inline int kill(int pid, int sig) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->kill) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->kill(runtime->ctx, pid, sig);
        });
    }

    inline int list_processes(std::span<ProcessSnapshot> out) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->list_processes) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->list_processes(runtime->ctx, out);
        });
    }

    inline int sleep(unsigned seconds) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->sleep) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->sleep(runtime->ctx, seconds);
        });
    }

    inline int mkdir(const char* path) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->mkdir) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->mkdir(runtime->ctx, path);
        });
    }

    inline int unlink(const char* path) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->unlink) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->unlink(runtime->ctx, path);
        });
    }

    inline int rmdir(const char* path) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->rmdir) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->rmdir(runtime->ctx, path);
        });
    }

    inline int rename(const char* from, const char* to) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->rename) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->rename(runtime->ctx, from, to);
        });
    }

    inline PosixDir* opendir(const char* path) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->opendir) {
            return detail::missing_runtime<PosixDir*>(nullptr);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->opendir(runtime->ctx, path);
        });
    }

    inline const PosixDirent* readdir(PosixDir* dir) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->readdir) {
            return detail::missing_runtime<const PosixDirent*>(nullptr);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->readdir(runtime->ctx, dir);
        });
    }

    inline int closedir(PosixDir* dir) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->closedir) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->closedir(runtime->ctx, dir);
        });
    }

    inline int getpid() noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->getpid) {
            return 0;
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->getpid(runtime->ctx);
        });
    }

    inline int chdir(const char* path) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->chdir) {
            return detail::missing_runtime<int>(-1);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->chdir(runtime->ctx, path);
        });
    }

    inline char* getcwd(char* buf, util::usize size) noexcept {
        const auto* runtime = active_runtime();
        if (!runtime || !runtime->getcwd) {
            return detail::missing_runtime<char*>(nullptr);
        }
        return detail::invoke_with_errno_sync([&]() noexcept {
            return runtime->getcwd(runtime->ctx, buf, size);
        });
    }
}

namespace posix::user::detail {
#if defined(__arm__) || defined(__thumb__)
    inline Runtime g_active_runtime{};
    inline bool g_has_active_runtime{false};
    inline std::array<Runtime, kRuntimeStackDepth> g_runtime_stack{};
    inline util::usize g_runtime_depth{0};
#else
    inline thread_local Runtime g_active_runtime{};
    inline thread_local bool g_has_active_runtime{false};
    inline thread_local std::array<Runtime, kRuntimeStackDepth> g_runtime_stack{};
    inline thread_local util::usize g_runtime_depth{0};
#endif
}

export namespace posix::user {
    inline void bind_runtime(const Runtime& runtime) noexcept {
        if (detail::g_has_active_runtime && detail::g_runtime_depth < detail::g_runtime_stack.size()) {
            detail::g_runtime_stack[detail::g_runtime_depth++] = detail::g_active_runtime;
        }
        detail::g_active_runtime = runtime;
        detail::g_has_active_runtime = true;
    }

    inline void unbind_runtime() noexcept {
        if (detail::g_runtime_depth > 0) {
            detail::g_active_runtime = detail::g_runtime_stack[--detail::g_runtime_depth];
            detail::g_has_active_runtime = true;
            return;
        }
        detail::g_active_runtime = {};
        detail::g_has_active_runtime = false;
    }

    inline const Runtime* active_runtime() noexcept {
        return detail::g_has_active_runtime ? &detail::g_active_runtime : nullptr;
    }

    inline bool has_runtime() noexcept {
        return active_runtime() != nullptr;
    }
}
