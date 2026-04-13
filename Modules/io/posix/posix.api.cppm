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
              util::usize MaxDirEntries = 32>
    class Api {
    public:
        Api(FdTable<MaxFds>& fd_table,
            FileService<MaxFiles>& file_service,
            PipeService<MaxPipes, PipeCapacity>& pipe_service,
            ProcService<MaxProcs, MaxExecs, MaxFds, MaxFiles, MaxPathLen, MaxArgc, MaxEnvp, MaxArgBytes,
                        MaxElfImage, MaxElfLoad>& proc_service) noexcept
            : fd_table_(&fd_table),
              file_service_(&file_service),
              pipe_service_(&pipe_service),
              proc_service_(&proc_service) {}

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
            auto entry = file_service_->open(std::string_view{path}, flags, mode);
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
            if (!entry.value()->ops || !entry.value()->ops->stat) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = entry.value()->ops->stat(entry.value()->ctx, *out);
            if (!r) {
                set_errno(map_fd_errno(r.error()));
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
            const std::string_view path_view{path};
            auto entry = file_service_->open(path_view, O_RDONLY, 0);
            if (!entry) {
                if (entry.error() == util::Errc::isdir || is_root_path(path_view)) {
                    auto st = stat_directory_path(path_view, *out);
                    if (!st) {
                        set_errno(map_errno(st.err));
                        return -1;
                    }
                    return 0;
                }
                set_errno(map_errno(entry.error()));
                return -1;
            }
            if (!entry.value().ops || !entry.value().ops->stat) {
                close_entry(entry.value());
                set_errno(ENOSYS);
                return -1;
            }
            auto r = entry.value().ops->stat(entry.value().ctx, *out);
            close_entry(entry.value());
            if (!r) {
                set_errno(map_errno(r.error()));
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
            auto st = fs::vfs_mkdir(std::string_view{path});
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
            PosixStat st_out{};
            if (stat(path, &st_out) != 0) {
                return -1;
            }
            if ((st_out.mode & S_IFMT) == S_IFDIR) {
                set_errno(EISDIR);
                return -1;
            }
            auto st = fs::vfs_unlink(std::string_view{path});
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
            PosixStat st_out{};
            if (stat(path, &st_out) != 0) {
                return -1;
            }
            if ((st_out.mode & S_IFMT) != S_IFDIR) {
                set_errno(ENOTDIR);
                return -1;
            }
            auto st = fs::vfs_unlink(std::string_view{path});
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
            auto st = fs::vfs_rename(std::string_view{from}, std::string_view{to});
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
            auto* handle = alloc_dir_handle();
            if (!handle) {
                set_errno(EMFILE);
                return nullptr;
            }
            handle->count = 0;
            handle->pos = 0;
            auto st = fs::vfs_list(std::string_view{path}, handle, &Api::collect_dir_entry);
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
            auto r = proc_service_->spawn(cfg, current_fd_table());
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

    private:
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

        static int map_pipe_errno(util::Errc err) noexcept {
            return to_pipe_errno(err);
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
        int bound_pid_{-1};
        std::array<int, 8> bound_pid_stack_{};
        util::usize bound_depth_{0};
        std::array<DirHandle, MaxDirs> dir_handles_{};
    };
}
