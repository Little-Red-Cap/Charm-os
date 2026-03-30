module;

#include <cstddef>
#include <span>
#include <string_view>

export module posix.api;

import posix.errno;
import posix.fd_table;
import posix.file;
import posix.pipe;
import posix.proc;
import util.core;
import util.error;

export namespace posix {
    using ssize_t = util::i64;

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
              util::usize MaxPathLen = 128>
    class Api {
    public:
        Api(FdTable<MaxFds>& fd_table,
            FileService<MaxFiles>& file_service,
            PipeService<MaxPipes, PipeCapacity>& pipe_service,
            ProcService<MaxProcs, MaxExecs, MaxFds, MaxFiles, MaxPathLen>& proc_service) noexcept
            : fd_table_(&fd_table),
              file_service_(&file_service),
              pipe_service_(&pipe_service),
              proc_service_(&proc_service) {}

        int open(const char* path, int flags, int mode = 0) noexcept {
            if (!fd_table_ || !file_service_) {
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
            auto rfd = fd_table_->attach(entry.value());
            if (!rfd) {
                close_entry(entry.value());
                set_errno(map_fd_attach_errno(rfd.error()));
                return -1;
            }
            return rfd.value();
        }

        int close(int fd) noexcept {
            if (!fd_table_) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = fd_table_->close(fd);
            if (!r) {
                set_errno(map_fd_errno(r.error()));
                return -1;
            }
            return 0;
        }

        ssize_t read(int fd, void* buf, util::usize count) noexcept {
            if (!fd_table_) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!buf && count > 0) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = fd_table_->get(fd);
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
            if (!fd_table_) {
                set_errno(ENOSYS);
                return -1;
            }
            if (!buf && count > 0) {
                set_errno(EINVAL);
                return -1;
            }
            auto entry = fd_table_->get(fd);
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

        int dup2(int from, int to) noexcept {
            if (!fd_table_) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = fd_table_->dup2(from, to);
            if (!r) {
                set_errno(map_fd_errno(r.error()));
                return -1;
            }
            return to;
        }

        int pipe(int fds[2]) noexcept {
            if (!fds) {
                set_errno(EINVAL);
                return -1;
            }
            if (!fd_table_ || !pipe_service_) {
                set_errno(ENOSYS);
                return -1;
            }
            auto r = pipe_service_->create(*fd_table_);
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
            auto r = proc_service_->spawn(cfg);
            if (!r) {
                set_errno(map_errno(r.error()));
                return -1;
            }
            return r.value().pid.value;
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

    private:
        static void close_entry(const FdEntry& entry) noexcept {
            if (entry.ops && entry.ops->close) {
                (void)entry.ops->close(entry.ctx);
            }
        }

        static int map_errno(util::Errc err) noexcept {
            return to_errno(err);
        }

        static int map_fd_errno(util::Errc err) noexcept {
            if (err == util::Errc::noent) return EBADF;
            return to_errno(err);
        }

        static int map_fd_attach_errno(util::Errc err) noexcept {
            if (err == util::Errc::buffer_overflow) return EMFILE;
            return to_errno(err);
        }

        static int map_pipe_errno(util::Errc err) noexcept {
            if (err == util::Errc::buffer_overflow) return ENOSPC;
            return to_errno(err);
        }

        FdTable<MaxFds>* fd_table_{nullptr};
        FileService<MaxFiles>* file_service_{nullptr};
        PipeService<MaxPipes, PipeCapacity>* pipe_service_{nullptr};
        ProcService<MaxProcs, MaxExecs, MaxFds, MaxFiles, MaxPathLen>* proc_service_{nullptr};
    };
}
