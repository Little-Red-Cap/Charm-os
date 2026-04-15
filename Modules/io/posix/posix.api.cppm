module;

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

#ifdef errno
#undef errno
#endif

export module posix.api;

import posix.errno;
import posix.fd_table;
import posix.file;
import posix.pipe;
import posix.proc;
import net.common;
import net.posix;
import charm.system.time;
import fs_core;
import fs_errno;
import fs_path;
import fs_stream;
import fs_vfs;
import util.core;
import util.error;

export namespace posix {
    using ssize_t = util::i64;
    inline constexpr util::usize kDirentNameMax = 64;
    inline constexpr int F_DUPFD = 0;
    inline constexpr int F_GETFD = 1;
    inline constexpr int F_SETFD = 2;
    inline constexpr int F_GETFL = 3;
    inline constexpr int F_SETFL = 4;
    inline constexpr int FD_CLOEXEC = 1;

    struct PosixDirent {
        std::array<char, kDirentNameMax> d_name{};
        util::u32 d_mode{0};
        util::u64 d_size{0};
    };

    struct PosixDir {
        util::usize slot{0};
    };

    inline int encode_wait_status(const WaitStatus& st) noexcept {
        switch (st.kind) {
            case WaitKind::exited:
                return (st.code & 0xff) << 8;
            case WaitKind::signaled:
                return (st.code & 0x7f);
            case WaitKind::stopped:
            case WaitKind::continued:
                return 0;
        }
        return 0;
    }

    template <util::usize MaxFds,
              util::usize MaxPipes,
              util::usize PipeCapacity,
              util::usize MaxProcs,
              util::usize MaxExecs,
              util::usize MaxFiles,
              util::usize MaxPathLen = 128,
              util::usize MaxArgc = 16,
              util::usize MaxEnvp = 16,
              util::usize MaxArgBytes = 256,
              util::usize MaxElfImage = 4096,
              util::usize MaxElfLoad = 4096,
              util::usize MaxDirs = 4,
              util::usize MaxDirEntries = 32,
              util::usize MaxSockets = 16>
    class Api {
    public:
        Api(FdTable<MaxFds>& fd_table,
            FileService<MaxFiles>& file_service,
            PipeService<MaxPipes, PipeCapacity>& pipe_service,
            ProcService<MaxProcs, MaxExecs, MaxFds, MaxFiles, MaxPathLen, MaxArgc, MaxEnvp, MaxArgBytes,
                        MaxElfImage, MaxElfLoad>& proc_service,
            SocketService<MaxSockets>* socket_service = nullptr) noexcept
            : fd_table_(&fd_table),
              file_service_(&file_service),
              pipe_service_(&pipe_service),
              proc_service_(&proc_service),
              socket_service_(socket_service) {
            reset_path_storage(host_cwd_);
        }

        void bind_process(ProcessId pid) noexcept {
            bound_pid_ = pid.value;
            bound_depth_ = 0;
        }
        void unbind_process() noexcept {
            bound_pid_ = -1;
            bound_depth_ = 0;
        }

        void push_process(ProcessId pid) noexcept {
            if (bound_depth_ < bound_pid_stack_.size()) {
                bound_pid_stack_[bound_depth_++] = bound_pid_;
            }
            bound_pid_ = pid.value;
        }

        void pop_process() noexcept {
            if (bound_depth_ > 0) {
                bound_pid_ = bound_pid_stack_[--bound_depth_];
                return;
            }
            bound_pid_ = -1;
        }

        int open(const char* path, int flags, int mode = 0) noexcept {
            auto* table = current_fd_table();
            if (!table || !file_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!path) {
                set_errno(EINVAL);
                return -1;
            }
            std::array<char, MaxPathLen> resolved{};
            auto resolved_path = resolve_path(path, resolved);
            if (!resolved_path) {
                set_errno(map_errno(resolved_path.error()));
                return -1;
            }
            const auto alias_kind = classify_fd_alias(resolved_path.value());
            if (alias_kind != FdAliasKind::none) {
                bool duplicated = false;
                auto alias_entry = clone_fd_alias_entry(alias_kind, *table, flags, duplicated);
                if (!alias_entry) {
                    set_errno(map_errno(alias_entry.error()));
                    return -1;
                }
                auto rfd = table->attach(alias_entry.value());
                if (!rfd) {
                    if (duplicated) {
                        close_entry(alias_entry.value());
                    }
                    set_errno(map_fd_attach_errno(rfd.error()));
                    return -1;
                }
                return rfd.value();
            }
            auto entry = file_service_->open(resolved_path.value(), flags, mode);
            if (!entry) {
                set_errno(map_errno(entry.error()));
                return -1;
            }
            auto rfd = table->attach(entry.value());
            if (!rfd) {
                close_entry(entry.value());
                set_errno(map_fd_attach_errno(rfd.error()));
                return -1;
            }
            return rfd.value();
        }

        int close(int fd) noexcept {
            auto* table = current_fd_table();
            if (!table) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = table->close(fd);
            if (!r) {
                set_errno(map_fd_errno(r.error()));
                return -1;
            }
            return 0;
        }

        int socket(int domain, int type, int protocol = 0) noexcept {
            auto* table = current_fd_table();
            if (!table || !socket_service_) {
                set_errno(ENOSYS);
                return -1;
            }

            auto entry = socket_service_->socket(domain, type, protocol);
            if (!entry) {
                set_errno(map_socket_errno(entry.error()));
                return -1;
            }

            auto rfd = table->attach(entry.value());
            if (!rfd) {
                close_entry(entry.value());
                set_errno(map_fd_attach_errno(rfd.error()));
                return -1;
            }
            return rfd.value();
        }

        int bind(int fd, const net::Endpoint& ep) noexcept {
            auto* table = current_fd_table();
            if (!table || !socket_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || entry.value()->kind != FdKind::socket) {
                set_errno(EBADF);
                return -1;
            }
            auto r = socket_service_->bind(*entry.value(), ep);
            if (!r) {
                set_errno(map_socket_errno(r.error()));
                return -1;
            }
            return 0;
        }

        int connect(int fd, const net::Endpoint& ep) noexcept {
            auto* table = current_fd_table();
            if (!table || !socket_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || entry.value()->kind != FdKind::socket) {
                set_errno(EBADF);
                return -1;
            }
            auto r = socket_service_->connect(*entry.value(), ep);
            if (!r) {
                set_errno(map_socket_errno(r.error()));
                return -1;
            }
            return 0;
        }

        int listen(int fd, int backlog) noexcept {
            auto* table = current_fd_table();
            if (!table || !socket_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            if (backlog < 0) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || entry.value()->kind != FdKind::socket) {
                set_errno(EBADF);
                return -1;
            }
            auto r = socket_service_->listen(*entry.value(), static_cast<util::u16>(backlog));
            if (!r) {
                set_errno(map_socket_errno(r.error()));
                return -1;
            }
            return 0;
        }

        int accept(int fd, net::Endpoint* peer = nullptr) noexcept {
            auto* table = current_fd_table();
            if (!table || !socket_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || entry.value()->kind != FdKind::socket) {
                set_errno(EBADF);
                return -1;
            }
            auto r = socket_service_->accept(*table, *entry.value(), peer);
            if (!r) {
                set_errno(map_socket_accept_errno(r.error()));
                return -1;
            }
            return r.value();
        }

        ssize_t send(int fd, const void* buf, util::usize count) noexcept {
            auto* table = current_fd_table();
            if (!table || !socket_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!buf && count > 0) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || entry.value()->kind != FdKind::socket) {
                set_errno(EBADF);
                return -1;
            }
            auto r = socket_service_->send(*entry.value(), ByteView{static_cast<const util::u8*>(buf), count});
            if (!r) {
                set_errno(map_socket_errno(r.error()));
                return -1;
            }
            return static_cast<ssize_t>(r.value());
        }

        ssize_t recv(int fd, void* buf, util::usize count) noexcept {
            auto* table = current_fd_table();
            if (!table || !socket_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!buf && count > 0) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || entry.value()->kind != FdKind::socket) {
                set_errno(EBADF);
                return -1;
            }
            auto r = socket_service_->recv(*entry.value(), MutByteView{static_cast<util::u8*>(buf), count});
            if (!r) {
                if (r.error() == util::Errc::end_of_stream) return 0;
                set_errno(map_socket_errno(r.error()));
                return -1;
            }
            return static_cast<ssize_t>(r.value());
        }

        ssize_t sendto(int fd, const void* buf, util::usize count, const net::Endpoint& peer) noexcept {
            auto* table = current_fd_table();
            if (!table || !socket_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!buf && count > 0) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || entry.value()->kind != FdKind::socket) {
                set_errno(EBADF);
                return -1;
            }
            auto r = socket_service_->sendto(*entry.value(), peer, ByteView{static_cast<const util::u8*>(buf), count});
            if (!r) {
                set_errno(map_socket_errno(r.error()));
                return -1;
            }
            return static_cast<ssize_t>(r.value());
        }

        ssize_t recvfrom(int fd, void* buf, util::usize count, net::Endpoint* peer = nullptr) noexcept {
            auto* table = current_fd_table();
            if (!table || !socket_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!buf && count > 0) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || entry.value()->kind != FdKind::socket) {
                set_errno(EBADF);
                return -1;
            }
            auto r = socket_service_->recvfrom(*entry.value(), peer, MutByteView{static_cast<util::u8*>(buf), count});
            if (!r) {
                set_errno(map_socket_errno(r.error()));
                return -1;
            }
            return static_cast<ssize_t>(r.value());
        }

        int shutdown(int fd, net::ShutdownMode mode = net::ShutdownMode::both) noexcept {
            auto* table = current_fd_table();
            if (!table || !socket_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || entry.value()->kind != FdKind::socket) {
                set_errno(EBADF);
                return -1;
            }
            auto r = socket_service_->shutdown(*entry.value(), mode);
            if (!r) {
                set_errno(map_socket_errno(r.error()));
                return -1;
            }
            return 0;
        }

        ssize_t read(int fd, void* buf, util::usize count) noexcept {
            auto* table = current_fd_table();
            if (!table) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!buf && count > 0) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || !entry.value()->ops || !entry.value()->ops->read) {
                set_errno(EBADF);
                return -1;
            }
            MutByteView view{static_cast<util::u8*>(buf), count};
            auto r = entry.value()->ops->read(entry.value()->ctx, view);
            if (!r) {
                if (r.error() == util::Errc::end_of_stream) return 0;
                if (r.error() == util::Errc::would_block) {
                    set_errno(EAGAIN);
                    return -1;
                }
                set_errno(map_errno(r.error()));
                return -1;
            }
            return static_cast<ssize_t>(r.value());
        }

        ssize_t write(int fd, const void* buf, util::usize count) noexcept {
            auto* table = current_fd_table();
            if (!table) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!buf && count > 0) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || !entry.value()->ops || !entry.value()->ops->write) {
                set_errno(EBADF);
                return -1;
            }
            ByteView view{static_cast<const util::u8*>(buf), count};
            auto r = entry.value()->ops->write(entry.value()->ctx, view);
            if (!r) {
                if (r.error() == util::Errc::closed) {
                    set_errno(EPIPE);
                    return -1;
                }
                if (r.error() == util::Errc::would_block) {
                    set_errno(EAGAIN);
                    return -1;
                }
                set_errno(map_errno(r.error()));
                return -1;
            }
            return static_cast<ssize_t>(r.value());
        }

        int fstat(int fd, PosixStat* out) noexcept {
            auto* table = current_fd_table();
            if (!table) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!out) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry) {
                set_errno(EBADF);
                return -1;
            }
            auto r = stat_fd_entry(*entry.value(), *out);
            if (!r) {
                set_errno(map_errno(r.error()));
                return -1;
            }
            return 0;
        }

        int stat(const char* path, PosixStat* out) noexcept {
            if (!file_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!path || !out) {
                set_errno(EINVAL);
                return -1;
            }
            std::array<char, MaxPathLen> resolved{};
            auto resolved_path = resolve_path(path, resolved);
            if (!resolved_path) {
                set_errno(map_errno(resolved_path.error()));
                return -1;
            }
            const auto alias_kind = classify_fd_alias(resolved_path.value());
            if (alias_kind != FdAliasKind::none) {
                auto st = stat_fd_alias(alias_kind, *out);
                if (!st) {
                    set_errno(map_errno(st.error()));
                    return -1;
                }
                return 0;
            }
            auto st = stat_path(resolved_path.value(), *out);
            if (!st) {
                set_errno(map_errno(st.error()));
                return -1;
            }
            return 0;
        }

        ssize_t lseek(int fd, util::i64 offset, int whence) noexcept {
            auto* table = current_fd_table();
            if (!table) {
                set_errno(ENOSYS);
                return -1;
            }
            if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry) {
                set_errno(EBADF);
                return -1;
            }
            if (entry.value()->kind != FdKind::file) {
                set_errno(ESPIPE);
                return -1;
            }
            if (!entry.value()->ops || !entry.value()->ops->seek) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = entry.value()->ops->seek(entry.value()->ctx, offset, whence);
            if (!r) {
                set_errno(map_errno(r.error()));
                return -1;
            }
            return static_cast<ssize_t>(r.value());
        }

        int mkdir(const char* path) noexcept {
            if (!path) {
                set_errno(EINVAL);
                return -1;
            }
            std::array<char, MaxPathLen> resolved{};
            auto resolved_path = resolve_path(path, resolved);
            if (!resolved_path) {
                set_errno(map_errno(resolved_path.error()));
                return -1;
            }
            auto st = fs::vfs_mkdir(resolved_path.value());
            if (!st) {
                set_errno(map_errno(st.err));
                return -1;
            }
            return 0;
        }

        int unlink(const char* path) noexcept {
            if (!path) {
                set_errno(EINVAL);
                return -1;
            }
            std::array<char, MaxPathLen> resolved{};
            auto resolved_path = resolve_path(path, resolved);
            if (!resolved_path) {
                set_errno(map_errno(resolved_path.error()));
                return -1;
            }
            PosixStat st_out{};
            if (stat(resolved.data(), &st_out) != 0) {
                return -1;
            }
            if ((st_out.mode & S_IFMT) == S_IFDIR) {
                set_errno(EISDIR);
                return -1;
            }
            auto st = fs::vfs_unlink(resolved_path.value());
            if (!st) {
                set_errno(map_errno(st.err));
                return -1;
            }
            return 0;
        }

        int rmdir(const char* path) noexcept {
            if (!path) {
                set_errno(EINVAL);
                return -1;
            }
            std::array<char, MaxPathLen> resolved{};
            auto resolved_path = resolve_path(path, resolved);
            if (!resolved_path) {
                set_errno(map_errno(resolved_path.error()));
                return -1;
            }
            PosixStat st_out{};
            if (stat(resolved.data(), &st_out) != 0) {
                return -1;
            }
            if ((st_out.mode & S_IFMT) != S_IFDIR) {
                set_errno(ENOTDIR);
                return -1;
            }
            auto st = fs::vfs_unlink(resolved_path.value());
            if (!st) {
                if (st.err == fs::Errc::busy) {
                    set_errno(ENOTEMPTY);
                    return -1;
                }
                set_errno(map_errno(st.err));
                return -1;
            }
            return 0;
        }

        int rename(const char* from, const char* to) noexcept {
            if (!from || !to) {
                set_errno(EINVAL);
                return -1;
            }
            std::array<char, MaxPathLen> resolved_from{};
            auto from_path = resolve_path(from, resolved_from);
            if (!from_path) {
                set_errno(map_errno(from_path.error()));
                return -1;
            }
            std::array<char, MaxPathLen> resolved_to{};
            auto to_path = resolve_path(to, resolved_to);
            if (!to_path) {
                set_errno(map_errno(to_path.error()));
                return -1;
            }
            auto st = fs::vfs_rename(from_path.value(), to_path.value());
            if (!st) {
                set_errno(map_errno(st.err));
                return -1;
            }
            return 0;
        }

        PosixDir* opendir(const char* path) noexcept {
            if (!path) {
                set_errno(EINVAL);
                return nullptr;
            }
            std::array<char, MaxPathLen> resolved{};
            auto resolved_path = resolve_path(path, resolved);
            if (!resolved_path) {
                set_errno(map_errno(resolved_path.error()));
                return nullptr;
            }
            auto* handle = alloc_dir_handle();
            if (!handle) {
                set_errno(EMFILE);
                return nullptr;
            }
            handle->count = 0;
            handle->pos = 0;
            auto st = fs::vfs_list(resolved_path.value(), handle, &Api::collect_dir_entry);
            if (!st) {
                handle->used = false;
                set_errno(map_errno(st.err));
                return nullptr;
            }
            return &handle->dir;
        }

        const PosixDirent* readdir(PosixDir* dir) noexcept {
            auto* handle = dir_handle(dir);
            if (!handle) {
                set_errno(EINVAL);
                return nullptr;
            }
            if (handle->pos >= handle->count) {
                return nullptr;
            }
            return &handle->entries[handle->pos++];
        }

        int closedir(PosixDir* dir) noexcept {
            auto* handle = dir_handle(dir);
            if (!handle) {
                set_errno(EINVAL);
                return -1;
            }
            handle->used = false;
            handle->count = 0;
            handle->pos = 0;
            return 0;
        }

        int dup2(int from, int to) noexcept {
            auto* table = current_fd_table();
            if (!table) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = table->dup2(from, to);
            if (!r) {
                set_errno(map_fd_errno(r.error()));
                return -1;
            }
            return to;
        }

        int dup(int from) noexcept {
            auto* table = current_fd_table();
            if (!table) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = table->dup(from);
            if (!r) {
                set_errno(map_dup_errno(r.error()));
                return -1;
            }
            return r.value();
        }

        int fcntl(int fd, int cmd, int arg = 0) noexcept {
            auto* table = current_fd_table();
            if (!table) {
                set_errno(ENOSYS);
                return -1;
            }
            switch (cmd) {
                case F_DUPFD: {
                    auto r = table->dup(fd, arg);
                    if (!r) {
                        set_errno(map_fcntl_dup_errno(r.error()));
                        return -1;
                    }
                    return r.value();
                }
                case F_GETFD: {
                    auto entry = table->get(fd);
                    if (!entry) {
                        set_errno(map_fd_errno(entry.error()));
                        return -1;
                    }
                    return entry.value()->inheritable ? 0 : FD_CLOEXEC;
                }
                case F_SETFD: {
                    auto entry = table->get(fd);
                    if (!entry) {
                        set_errno(map_fd_errno(entry.error()));
                        return -1;
                    }
                    if (arg != 0 && arg != FD_CLOEXEC) {
                        set_errno(EINVAL);
                        return -1;
                    }
                    entry.value()->inheritable = (arg & FD_CLOEXEC) == 0;
                    return 0;
                }
                case F_GETFL: {
                    auto entry = table->get(fd);
                    if (!entry) {
                        set_errno(map_fd_errno(entry.error()));
                        return -1;
                    }
                    int status_flags = 0;
                    if (entry.value()->ops && entry.value()->ops->get_status_flags) {
                        auto status = entry.value()->ops->get_status_flags(entry.value()->ctx);
                        if (!status) {
                            set_errno(map_fcntl_status_errno(status.error()));
                            return -1;
                        }
                        status_flags = status.value();
                    }
                    return fd_flags_to_open_mode(entry.value()->flags) | status_flags;
                }
                case F_SETFL: {
                    auto entry = table->get(fd);
                    if (!entry) {
                        set_errno(map_fd_errno(entry.error()));
                        return -1;
                    }
                    constexpr int kSettableFlags = O_APPEND | O_NONBLOCK;
                    if ((arg & ~kSettableFlags) != 0) {
                        set_errno(EINVAL);
                        return -1;
                    }
                    if (!entry.value()->ops || !entry.value()->ops->set_status_flags) {
                        if (arg == 0) {
                            return 0;
                        }
                        set_errno(EINVAL);
                        return -1;
                    }
                    auto st = entry.value()->ops->set_status_flags(entry.value()->ctx, arg & kSettableFlags);
                    if (!st) {
                        set_errno(map_fcntl_status_errno(st.error()));
                        return -1;
                    }
                    return 0;
                }
                default:
                    set_errno(EINVAL);
                    return -1;
            }
        }

        int isatty(int fd) noexcept {
            auto* table = current_fd_table();
            if (!table) {
                set_errno(ENOSYS);
                return 0;
            }
            auto entry = table->get(fd);
            if (!entry) {
                set_errno(EBADF);
                return 0;
            }
            return entry.value()->kind == FdKind::term ? 1 : 0;
        }

        int pipe(int fds[2]) noexcept {
            if (!fds) {
                set_errno(EINVAL);
                return -1;
            }
            auto* table = current_fd_table();
            if (!table || !pipe_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = pipe_service_->create(*table);
            if (!r) {
                set_errno(map_pipe_errno(r.error()));
                return -1;
            }
            fds[0] = r.value().read_fd;
            fds[1] = r.value().write_fd;
            return 0;
        }

        int spawn(const SpawnConfig& cfg) noexcept {
            if (!proc_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            SpawnConfig runtime_cfg = cfg;
            std::array<char, MaxPathLen> cwd_storage{};
            if (cfg.cwd != nullptr && cfg.cwd[0] != '\0') {
                auto resolved_cwd = resolve_path(cfg.cwd, cwd_storage);
                if (!resolved_cwd) {
                    set_errno(map_errno(resolved_cwd.error()));
                    return -1;
                }
                runtime_cfg.cwd = cwd_storage.data();
            } else {
                auto st = copy_path_storage(current_cwd(), cwd_storage);
                if (!st) {
                    set_errno(map_errno(st.error()));
                    return -1;
                }
                runtime_cfg.cwd = cwd_storage.data();
            }
            auto r = proc_service_->spawn(runtime_cfg, current_fd_table());
            if (!r) {
                set_errno(map_errno(r.error()));
                return -1;
            }
            return r.value().pid.value;
        }

        int spawnp(SpawnConfig cfg) noexcept {
            cfg.path_mode = PathMode::search_path;
            return spawn(cfg);
        }

        int waitpid(ProcessId pid, int* status, int options = 0) noexcept {
            if (!proc_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = proc_service_->waitpid(pid, options);
            if (!r) {
                set_errno(map_errno(r.error()));
                return -1;
            }
            if (status) {
                *status = encode_wait_status(r.value());
            }
            return r.value().pid.value;
        }

        int kill(ProcessId pid, int sig) noexcept {
            if (!proc_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = proc_service_->kill(pid, sig);
            if (!r) {
                set_errno(map_errno(r.error()));
                return -1;
            }
            return 0;
        }

        int kill(int pid, int sig) noexcept {
            return kill(ProcessId{pid}, sig);
        }

        int list_processes(std::span<ProcessSnapshot> out) noexcept {
            if (!proc_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            return static_cast<int>(proc_service_->snapshot_processes(out));
        }

        int getpid() const noexcept {
            return bound_pid_ >= 0 ? bound_pid_ : 0;
        }

        int sleep(unsigned seconds) noexcept {
            const auto ms = static_cast<util::u64>(seconds) * 1000u;
            auto st = charm::system::time::try_sleep_ms(ms);
            if (!st) {
                set_errno(map_errno(st.error()));
                return -1;
            }
            return 0;
        }

        int chdir(const char* path) noexcept {
            if (!path) {
                set_errno(EINVAL);
                return -1;
            }
            std::array<char, MaxPathLen> resolved{};
            auto resolved_path = resolve_path(path, resolved);
            if (!resolved_path) {
                set_errno(map_errno(resolved_path.error()));
                return -1;
            }
            PosixStat st{};
            auto stat_result = stat_path(resolved_path.value(), st);
            if (!stat_result) {
                set_errno(map_errno(stat_result.error()));
                return -1;
            }
            if ((st.mode & S_IFMT) != S_IFDIR) {
                set_errno(ENOTDIR);
                return -1;
            }
            auto cwd_result = set_current_cwd(resolved_path.value());
            if (!cwd_result) {
                set_errno(map_errno(cwd_result.error()));
                return -1;
            }
            return 0;
        }

        char* getcwd(char* buf, util::usize size) noexcept {
            if (!buf || size == 0) {
                set_errno(EINVAL);
                return nullptr;
            }
            const auto cwd_view = current_cwd();
            if (cwd_view.size() + 1 > size) {
                set_errno(ERANGE);
                return nullptr;
            }
            for (util::usize i = 0; i < cwd_view.size(); ++i) {
                buf[i] = cwd_view[i];
            }
            buf[cwd_view.size()] = '\0';
            return buf;
        }

    private:
        static constexpr std::string_view root_cwd() noexcept {
            return std::string_view{"/"};
        }

        static void reset_path_storage(std::array<char, MaxPathLen>& storage) noexcept {
            storage = {};
            storage[0] = '/';
            storage[1] = '\0';
        }

        static void pop_component(std::array<char, MaxPathLen>& storage, util::usize& size) noexcept {
            while (size > 1 && storage[size - 1] != '/') {
                --size;
            }
            if (size > 1) {
                --size;
            }
        }

        static util::Result<void> append_component(std::array<char, MaxPathLen>& storage,
                                                   util::usize& size,
                                                   std::string_view component) noexcept {
            if (component.empty() || component == ".") {
                return {};
            }
            if (component == "..") {
                pop_component(storage, size);
                return {};
            }
            if (size > 1) {
                if (size + 1 >= MaxPathLen) {
                    return util::unexpected(util::Errc::nametoolong);
                }
                storage[size++] = '/';
            }
            if (size + component.size() >= MaxPathLen) {
                return util::unexpected(util::Errc::nametoolong);
            }
            for (char ch : component) {
                storage[size++] = ch;
            }
            return {};
        }

        static util::Result<void> append_path_components(std::array<char, MaxPathLen>& storage,
                                                         util::usize& size,
                                                         std::string_view path) noexcept {
            util::usize start = 0;
            while (start < path.size()) {
                while (start < path.size() && fs::is_sep(path[start])) {
                    ++start;
                }
                util::usize end = start;
                while (end < path.size() && !fs::is_sep(path[end])) {
                    ++end;
                }
                if (end > start) {
                    auto step = append_component(storage, size, path.substr(start, end - start));
                    if (!step) {
                        return step;
                    }
                }
                start = end + 1;
            }
            return {};
        }

        static util::Result<void> copy_path_storage(std::string_view path,
                                                    std::array<char, MaxPathLen>& storage) noexcept {
            if (path.empty()) {
                path = root_cwd();
            }
            util::usize size = 0;
            reset_path_storage(storage);
            size = 1;
            auto status = append_path_components(storage, size, path);
            if (!status) {
                return status;
            }
            storage[size] = '\0';
            return {};
        }

        util::Result<std::string_view> resolve_path(const char* path,
                                                    std::array<char, MaxPathLen>& storage) const noexcept {
            if (!path) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            const std::string_view input{path};
            if (input.empty()) {
                return util::unexpected(util::Errc::noent);
            }

            util::usize size = 1;
            reset_path_storage(storage);
            if (!fs::is_sep(input.front())) {
                auto status = append_path_components(storage, size, current_cwd());
                if (!status) {
                    return util::unexpected(status.error());
                }
            }
            auto status = append_path_components(storage, size, input);
            if (!status) {
                return util::unexpected(status.error());
            }
            storage[size] = '\0';
            return std::string_view{storage.data(), size};
        }

        std::string_view current_cwd() const noexcept {
            if (bound_pid_ >= 0 && proc_service_) {
                return proc_service_->cwd(ProcessId{bound_pid_});
            }
            return std::string_view{host_cwd_.data()};
        }

        util::Result<void> set_current_cwd(std::string_view path) noexcept {
            if (bound_pid_ >= 0 && proc_service_) {
                return proc_service_->set_cwd(ProcessId{bound_pid_}, path);
            }
            return copy_path_storage(path, host_cwd_);
        }

        util::Result<void> stat_path(std::string_view path, PosixStat& out) noexcept {
            auto entry = file_service_->open(path, O_RDONLY, 0);
            if (!entry) {
                if (entry.error() == util::Errc::isdir || is_root_path(path)) {
                    auto st = stat_directory_path(path, out);
                    if (!st) {
                        return util::unexpected(st.err);
                    }
                    return {};
                }
                return util::unexpected(entry.error());
            }
            auto st = stat_fd_entry(entry.value(), out);
            close_entry(entry.value());
            if (!st) {
                return util::unexpected(st.error());
            }
            return {};
        }

        struct DirHandle {
            PosixDir dir{};
            std::array<PosixDirent, MaxDirEntries> entries{};
            util::usize count{0};
            util::usize pos{0};
            bool used{false};
        };

        static fs::Status collect_dir_entry(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
            auto* handle = static_cast<DirHandle*>(ctx);
            if (!handle) {
                return fs::Status{fs::Errc::inval};
            }
            if (handle->count >= MaxDirEntries) {
                return fs::Status{fs::Errc::nomem};
            }
            if (entry.name.size() >= kDirentNameMax) {
                return fs::Status{fs::Errc::nametoolong};
            }
            auto& out = handle->entries[handle->count++];
            for (util::usize i = 0; i < entry.name.size(); ++i) {
                out.d_name[i] = entry.name[i];
            }
            out.d_name[entry.name.size()] = '\0';
            out.d_mode = make_stat_mode(stat_type_from_node(entry.type), stat_perm_from_node(entry.type));
            out.d_size = entry.size;
            return fs::Status{fs::Errc::ok};
        }

        static fs::Status probe_dir_entry(void*, const fs::MountOps::ListEntry&) noexcept {
            return fs::Status{fs::Errc::ok};
        }

        static bool is_root_path(std::string_view path) noexcept {
            const auto norm = fs::normalize(path);
            const auto trimmed = fs::rstrip_seps(norm);
            return trimmed.size == 0;
        }

        enum class FdAliasKind : util::u8 {
            none,
            stdin_alias,
            stdout_alias,
            stderr_alias,
            console_alias,
        };

        static FdAliasKind classify_fd_alias(std::string_view path) noexcept {
            if (path == "/dev/stdin") return FdAliasKind::stdin_alias;
            if (path == "/dev/stdout") return FdAliasKind::stdout_alias;
            if (path == "/dev/stderr") return FdAliasKind::stderr_alias;
            if (path == "/dev/console" || path == "/dev/tty") return FdAliasKind::console_alias;
            return FdAliasKind::none;
        }

        static FdFlags flags_to_fd_flags(int flags) noexcept {
            if ((flags & O_RDWR) != 0) return FdFlags::read_write;
            if ((flags & O_WRONLY) != 0) return FdFlags::write_only;
            return FdFlags::read_only;
        }


        static int fd_flags_to_open_mode(FdFlags flags) noexcept {
            if (flags == FdFlags::read_write) return O_RDWR;
            if (flags == FdFlags::write_only) return O_WRONLY;
            return O_RDONLY;
        }

        static util::Result<void> fill_fallback_stat(const FdEntry& entry, PosixStat& out) noexcept {
            switch (entry.kind) {
                case FdKind::term:
                case FdKind::dev:
                    out.mode = make_stat_mode(S_IFCHR, kModePermChar);
                    out.size = 0;
                    return {};
                case FdKind::pipe:
                    out.mode = make_stat_mode(S_IFIFO, kModePermPipe);
                    out.size = 0;
                    return {};
                case FdKind::file:
                case FdKind::proc:
                default:
                    return util::unexpected(util::Errc::nosys);
            }
        }

        static util::Result<void> stat_fd_entry(const FdEntry& entry, PosixStat& out) noexcept {
            if (entry.ops && entry.ops->stat) {
                auto st = entry.ops->stat(entry.ctx, out);
                if (!st) {
                    return util::unexpected(st.error());
                }
                return {};
            }
            return fill_fallback_stat(entry, out);
        }

        bool has_live_fd(const FdTable<MaxFds>& table, int fd) noexcept {
            auto entry = const_cast<FdTable<MaxFds>&>(table).get(fd);
            return static_cast<bool>(entry);
        }

        int select_console_fd(const FdTable<MaxFds>& table, int flags) noexcept {
            const auto is_term_fd = [&](int fd) noexcept -> bool {
                auto entry = const_cast<FdTable<MaxFds>&>(table).get(fd);
                return entry && entry.value()->kind == FdKind::term;
            };
            const auto first_any_term = [&]() noexcept -> int {
                for (util::usize i = 0; i < MaxFds; ++i) {
                    if (is_term_fd(static_cast<int>(i))) {
                        return static_cast<int>(i);
                    }
                }
                return -1;
            };

            const int access_mode = flags & O_ACCMODE;
            if (access_mode == O_WRONLY) {
                for (int fd : {1, 2, 0}) {
                    if (is_term_fd(fd)) return fd;
                }
                return first_any_term();
            }
            if (access_mode == O_RDONLY) {
                for (int fd : {0, 1, 2}) {
                    if (is_term_fd(fd)) return fd;
                }
                return first_any_term();
            }
            for (int fd : {0, 1, 2}) {
                if (is_term_fd(fd)) return fd;
            }
            return first_any_term();
        }

        int select_fd_alias_source(FdAliasKind kind,
                                   const FdTable<MaxFds>& table,
                                   int flags) noexcept {
            switch (kind) {
                case FdAliasKind::stdin_alias:
                    return has_live_fd(table, 0) ? 0 : -1;
                case FdAliasKind::stdout_alias:
                    return has_live_fd(table, 1) ? 1 : -1;
                case FdAliasKind::stderr_alias:
                    return has_live_fd(table, 2) ? 2 : -1;
                case FdAliasKind::console_alias:
                    return select_console_fd(table, flags);
                case FdAliasKind::none:
                default:
                    return -1;
            }
        }

        util::Result<FdEntry> clone_fd_alias_entry(FdAliasKind kind,
                                                   FdTable<MaxFds>& table,
                                                   int flags,
                                                   bool& duplicated) noexcept {
            duplicated = false;
            const int source_fd = select_fd_alias_source(kind, table, flags);
            if (source_fd < 0) {
                return util::unexpected(util::Errc::noent);
            }
            auto entry = table.clone_entry(source_fd);
            if (!entry) {
                return util::unexpected(entry.error());
            }
            auto copy = entry.value();
            copy.flags = flags_to_fd_flags(flags);
            if (copy.ops && copy.ops->dup) {
                auto dup_st = copy.ops->dup(copy.ctx);
                if (!dup_st) {
                    return util::unexpected(dup_st.error());
                }
                duplicated = true;
            }
            return copy;
        }

        util::Result<void> stat_fd_alias(FdAliasKind kind, PosixStat& out) noexcept {
            auto* table = current_fd_table();
            if (!table) {
                return util::unexpected(util::Errc::nosys);
            }
            const int source_fd = select_fd_alias_source(kind, *table, O_RDWR);
            if (source_fd < 0) {
                return util::unexpected(util::Errc::noent);
            }
            auto entry = table->get(source_fd);
            if (!entry) {
                return util::unexpected(entry.error());
            }
            return stat_fd_entry(*entry.value(), out);
        }

        static fs::Status stat_directory_path(std::string_view path, PosixStat& out) noexcept {
            auto st = fs::vfs_list(path, nullptr, &Api::probe_dir_entry);
            if (!st) {
                return st;
            }
            out.mode = make_stat_mode(stat_type_from_node(fs::NodeType::dir),
                                      stat_perm_from_node(fs::NodeType::dir));
            out.size = 0;
            return fs::Status{fs::Errc::ok};
        }

        DirHandle* alloc_dir_handle() noexcept {
            for (util::usize i = 0; i < dir_handles_.size(); ++i) {
                if (dir_handles_[i].used) continue;
                auto& handle = dir_handles_[i];
                handle.used = true;
                handle.dir.slot = i;
                handle.count = 0;
                handle.pos = 0;
                return &handle;
            }
            return nullptr;
        }

        DirHandle* dir_handle(PosixDir* dir) noexcept {
            if (!dir || dir->slot >= dir_handles_.size()) {
                return nullptr;
            }
            auto& handle = dir_handles_[dir->slot];
            return handle.used ? &handle : nullptr;
        }

        static void close_entry(const FdEntry& entry) noexcept {
            if (entry.ops && entry.ops->close) {
                (void)entry.ops->close(entry.ctx);
            }
        }

        static int map_errno(util::Errc err) noexcept {
            return to_errno(err);
        }

        static int map_fd_errno(util::Errc err) noexcept {
            return to_fd_errno(err);
        }

        static int map_fd_attach_errno(util::Errc err) noexcept {
            return to_fd_attach_errno(err);
        }

        static int map_dup_errno(util::Errc err) noexcept {
            if (err == util::Errc::buffer_overflow) {
                return EMFILE;
            }
            return map_fd_errno(err);
        }

        static int map_fcntl_dup_errno(util::Errc err) noexcept {
            switch (err) {
                case util::Errc::buffer_overflow:
                    return EMFILE;
                case util::Errc::invalid_arg:
                    return EINVAL;
                default:
                    return map_fd_errno(err);
            }
        }


        static int map_fcntl_status_errno(util::Errc err) noexcept {
            switch (err) {
                case util::Errc::invalid_arg:
                    return EINVAL;
                default:
                    return map_fd_errno(err);
            }
        }

        static int map_pipe_errno(util::Errc err) noexcept {
            return to_pipe_errno(err);
        }

        static int map_socket_errno(util::Errc err) noexcept {
            return to_errno(err);
        }

        static int map_socket_accept_errno(util::Errc err) noexcept {
            if (err == util::Errc::buffer_overflow) {
                return EMFILE;
            }
            return to_errno(err);
        }

        FdTable<MaxFds>* current_fd_table() noexcept {
            if (!fd_table_) return nullptr;
            if (bound_pid_ < 0 || !proc_service_) return fd_table_;
            auto* child = proc_service_->fd_table(ProcessId{bound_pid_});
            return child ? child : fd_table_;
        }

        FdTable<MaxFds>* fd_table_{nullptr};
        FileService<MaxFiles>* file_service_{nullptr};
        PipeService<MaxPipes, PipeCapacity>* pipe_service_{nullptr};
        ProcService<MaxProcs, MaxExecs, MaxFds, MaxFiles, MaxPathLen, MaxArgc, MaxEnvp, MaxArgBytes, MaxElfImage, MaxElfLoad>* proc_service_{nullptr};
        SocketService<MaxSockets>* socket_service_{nullptr};
        int bound_pid_{-1};
        std::array<int, 8> bound_pid_stack_{};
        util::usize bound_depth_{0};
        std::array<char, MaxPathLen> host_cwd_{};
        std::array<DirHandle, MaxDirs> dir_handles_{};
    };
}
