module;

#include <string_view>

#ifdef errno
#undef errno
#endif

export module posix.elf_hostcall;

import posix.errno;
import posix.exec_context;
import posix.fd_table;
import posix.proc_types;
import charm.system.time;
import util.core;
import util.error;

export namespace posix {
    struct ElfHostStat {
        util::u64 st_size{0};
        util::u32 st_mode{0};
    };

    struct ElfHostCalls {
        util::i32 (*write)(int fd, const void* buf, util::usize len) noexcept {nullptr};
        void (*exit)(int code) noexcept {nullptr};
        util::i32 (*open)(const char* path, int flags, int mode) noexcept {nullptr};
        util::i32 (*close)(int fd) noexcept {nullptr};
        util::i32 (*read)(int fd, void* buf, util::usize len) noexcept {nullptr};
        util::i32 (*fstat)(int fd, void* st) noexcept {nullptr};
        util::i32 (*isatty)(int fd) noexcept {nullptr};
        int* (*errno_location)() noexcept {nullptr};
        util::i32 (*getpid)() noexcept {nullptr};
        util::i32 (*sleep)(util::u32 seconds) noexcept {nullptr};
        util::i32 (*kill)(int pid, int sig) noexcept {nullptr};
    };

    template <class Service>
    struct ElfHostcallAdapter {
        static int errc_to_errno(util::Errc err) noexcept {
            return to_errno(err);
        }

        static int fd_errc_to_errno(util::Errc err) noexcept {
            return to_fd_errno(err);
        }

        static int fd_attach_errc_to_errno(util::Errc err) noexcept {
            return to_fd_attach_errno(err);
        }

        static ExecContext* current_exec_context() noexcept {
            return active_exec_context();
        }

        static Service* exec_service(const ExecContext* ctx) noexcept {
            return ctx ? static_cast<Service*>(ctx->owner) : nullptr;
        }

        static void set_exec_errno(util::Errc err) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx) return;
            ctx->errno_value = errc_to_errno(err);
        }

        static void set_exec_fd_errno(util::Errc err) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx) return;
            ctx->errno_value = fd_errc_to_errno(err);
        }

        static void set_exec_fd_attach_errno(util::Errc err) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx) return;
            ctx->errno_value = fd_attach_errc_to_errno(err);
        }

        static void clear_exec_errno() noexcept {
            auto* ctx = current_exec_context();
            if (!ctx) return;
            ctx->errno_value = 0;
        }

        static util::i32 write(int fd, const void* buf, util::usize len) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx || !buf) {
                set_exec_errno(util::Errc::invalid_arg);
                return -1;
            }
            auto* service = exec_service(ctx);
            auto* table = service ? service->fd_table_by_pid_value(ctx->pid_value) : nullptr;
            if (!table) {
                set_exec_errno(util::Errc::bad_state);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || !entry.value()->ops || !entry.value()->ops->write) {
                set_exec_fd_errno(util::Errc::noent);
                return -1;
            }
            ByteView view{static_cast<const util::u8*>(buf), len};
            auto r = entry.value()->ops->write(entry.value()->ctx, view);
            if (!r) {
                set_exec_errno(r.error());
                return -1;
            }
            clear_exec_errno();
            return static_cast<util::i32>(r.value());
        }

        static void exit(int code) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx) return;
            request_exec_exit(*ctx, code);
        }

        static util::i32 open(const char* path, int flags, int mode) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx || !path) {
                set_exec_errno(util::Errc::invalid_arg);
                return -1;
            }
            auto* service = exec_service(ctx);
            auto* table = service ? service->fd_table_by_pid_value(ctx->pid_value) : nullptr;
            if (!table || !service) {
                set_exec_errno(util::Errc::bad_state);
                return -1;
            }
            auto entry = service->open_exec_file(std::string_view{path}, flags, mode);
            if (!entry) {
                set_exec_errno(entry.error());
                return -1;
            }
            auto rfd = table->attach(entry.value());
            if (!rfd) {
                service->close_exec_entry(entry.value());
                set_exec_fd_attach_errno(rfd.error());
                return -1;
            }
            clear_exec_errno();
            return static_cast<util::i32>(rfd.value());
        }

        static util::i32 close(int fd) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx) {
                set_exec_errno(util::Errc::bad_state);
                return -1;
            }
            auto* service = exec_service(ctx);
            auto* table = service ? service->fd_table_by_pid_value(ctx->pid_value) : nullptr;
            if (!table) {
                set_exec_errno(util::Errc::bad_state);
                return -1;
            }
            auto r = table->close(fd);
            if (!r) {
                set_exec_fd_errno(r.error());
                return -1;
            }
            clear_exec_errno();
            return 0;
        }

        static util::i32 read(int fd, void* buf, util::usize len) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx || (!buf && len > 0)) {
                set_exec_errno(util::Errc::invalid_arg);
                return -1;
            }
            auto* service = exec_service(ctx);
            auto* table = service ? service->fd_table_by_pid_value(ctx->pid_value) : nullptr;
            if (!table) {
                set_exec_errno(util::Errc::bad_state);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry || !entry.value()->ops || !entry.value()->ops->read) {
                set_exec_fd_errno(util::Errc::noent);
                return -1;
            }
            MutByteView view{static_cast<util::u8*>(buf), len};
            auto r = entry.value()->ops->read(entry.value()->ctx, view);
            if (!r) {
                if (r.error() == util::Errc::end_of_stream) {
                    clear_exec_errno();
                    return 0;
                }
                set_exec_errno(r.error());
                return -1;
            }
            clear_exec_errno();
            return static_cast<util::i32>(r.value());
        }

        static util::i32 fstat(int fd, void* st) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx || !st) {
                set_exec_errno(util::Errc::invalid_arg);
                return -1;
            }
            auto* service = exec_service(ctx);
            auto* table = service ? service->fd_table_by_pid_value(ctx->pid_value) : nullptr;
            if (!table) {
                set_exec_errno(util::Errc::bad_state);
                return -1;
            }
            auto entry = table->get(fd);
            if (!entry) {
                set_exec_fd_errno(util::Errc::noent);
                return -1;
            }
            if (!entry.value()->ops || !entry.value()->ops->stat) {
                set_exec_fd_errno(util::Errc::noent);
                return -1;
            }
            PosixStat info{};
            auto r = entry.value()->ops->stat(entry.value()->ctx, info);
            if (!r) {
                set_exec_errno(r.error());
                return -1;
            }
            auto* host = static_cast<ElfHostStat*>(st);
            host->st_size = info.size;
            host->st_mode = info.mode;
            clear_exec_errno();
            return 0;
        }

        static util::i32 isatty(int fd) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx) return 0;
            auto* service = exec_service(ctx);
            auto* table = service ? service->fd_table_by_pid_value(ctx->pid_value) : nullptr;
            if (!table) {
                clear_exec_errno();
                return 0;
            }
            auto entry = table->get(fd);
            if (!entry) {
                clear_exec_errno();
                return 0;
            }
            clear_exec_errno();
            return entry.value()->kind == FdKind::term ? 1 : 0;
        }

        static int* errno_location() noexcept {
            auto* ctx = current_exec_context();
            return ctx ? &ctx->errno_value : nullptr;
        }

        static util::i32 getpid() noexcept {
            auto* ctx = current_exec_context();
            clear_exec_errno();
            return ctx ? static_cast<util::i32>(ctx->pid_value) : 0;
        }

        static util::i32 sleep(util::u32 seconds) noexcept {
            auto st = charm::system::time::try_sleep_ms(static_cast<util::u64>(seconds) * 1000u);
            if (!st) {
                set_exec_errno(st.error());
                return -1;
            }
            clear_exec_errno();
            return 0;
        }

        static util::i32 kill(int pid, int sig) noexcept {
            auto* ctx = current_exec_context();
            if (!ctx || pid < 0) {
                set_exec_errno(util::Errc::invalid_arg);
                return -1;
            }
            auto* service = exec_service(ctx);
            if (!service) {
                set_exec_errno(util::Errc::bad_state);
                return -1;
            }
            auto st = service->kill(ProcessId{pid}, sig);
            if (!st) {
                set_exec_errno(st.error());
                return -1;
            }
            clear_exec_errno();
            return 0;
        }
    };

    template <class Service>
    inline util::Result<void> install_elf_hostcalls(Service& service, void* load_base, util::usize load_size) noexcept {
        (void)service;
        if (load_size < sizeof(ElfHostCalls)) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        auto* table = reinterpret_cast<ElfHostCalls*>(load_base);
        table->write = &ElfHostcallAdapter<Service>::write;
        table->exit = &ElfHostcallAdapter<Service>::exit;
        table->open = &ElfHostcallAdapter<Service>::open;
        table->close = &ElfHostcallAdapter<Service>::close;
        table->read = &ElfHostcallAdapter<Service>::read;
        table->fstat = &ElfHostcallAdapter<Service>::fstat;
        table->isatty = &ElfHostcallAdapter<Service>::isatty;
        table->errno_location = &ElfHostcallAdapter<Service>::errno_location;
        table->getpid = &ElfHostcallAdapter<Service>::getpid;
        table->sleep = &ElfHostcallAdapter<Service>::sleep;
        table->kill = &ElfHostcallAdapter<Service>::kill;
        return {};
    }
}
