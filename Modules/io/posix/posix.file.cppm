module;

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

export module posix.file;

import fs_core;
import fs_vfs;
import init.node;
import posix.fd_table;
import util.core;
import util.error;

export namespace posix {
    inline constexpr int O_RDONLY = 0x0;
    inline constexpr int O_WRONLY = 0x1;
    inline constexpr int O_RDWR = 0x2;
    inline constexpr int O_CREAT = 0x40;
    inline constexpr int O_TRUNC = 0x200;
    inline constexpr int O_APPEND = 0x400;

    template <typename T, util::usize N>
    class FilePool {
    public:
        T* create() noexcept {
            for (util::usize i = 0; i < N; ++i) {
                if (!used_[i]) {
                    used_[i] = true;
                    return new (&storage_[i]) T();
                }
            }
            return nullptr;
        }

        void destroy(T* obj) noexcept {
            if (!obj) return;
            for (util::usize i = 0; i < N; ++i) {
                auto* slot = reinterpret_cast<void*>(&storage_[i]);
                if (slot == reinterpret_cast<void*>(obj)) {
                    obj->~T();
                    used_[i] = false;
                    return;
                }
            }
        }

        void reset() noexcept {
            for (util::usize i = 0; i < N; ++i) {
                used_[i] = false;
            }
        }

    private:
        using Slot = std::aligned_storage_t<sizeof(T), alignof(T)>;
        std::array<Slot, N> storage_{};
        std::array<bool, N> used_{};
    };

    template <util::usize MaxFiles>
    class FileService {
    public:
        void init() noexcept { pool_.reset(); }

        util::Result<FdEntry> open(std::string_view path, int flags, int mode = 0) noexcept {
            (void)mode;
            if (path.empty()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (path == "/dev/null") {
                FdEntry entry{};
                entry.kind = FdKind::dev;
                entry.flags = flags_to_fd_flags(flags);
                entry.ops = &NullDevice::ops();
                entry.ctx = nullptr;
                entry.inheritable = true;
                return entry;
            }
            auto* handle = pool_.create();
            if (!handle) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            handle->owner = this;
            handle->append = (flags & O_APPEND) != 0;

            auto fs_flags = fs::OpenFlags::read;
            if ((flags & O_WRONLY) != 0) {
                fs_flags = fs::OpenFlags::write;
            }
            if ((flags & O_RDWR) != 0) {
                fs_flags = combine_flags(fs::OpenFlags::read, fs::OpenFlags::write);
            }
            if ((flags & O_CREAT) != 0) {
                fs_flags = combine_flags(fs_flags, fs::OpenFlags::create);
            }
            if ((flags & O_TRUNC) != 0) {
                fs_flags = combine_flags(fs_flags, fs::OpenFlags::trunc);
            }

            auto st = fs::vfs_open(path, handle->file, fs_flags);
            if (!st) {
                pool_.destroy(handle);
                return util::unexpected(st.err);
            }

            FdEntry entry{};
            entry.kind = FdKind::file;
            entry.flags = flags_to_fd_flags(flags);
            entry.ops = &FileService::ops();
            entry.ctx = handle;
            entry.inheritable = true;
            return entry;
        }

    private:
        struct NullDevice {
            static util::Result<util::usize> read(void*, MutByteView) noexcept {
                return util::usize{0};
            }
            static util::Result<util::usize> write(void*, ByteView buf) noexcept {
                return buf.size();
            }
            static util::Result<void> close(void*) noexcept { return {}; }
            static util::Result<void> stat(void*, PosixStat& out) noexcept {
                out.mode = 0;
                out.size = 0;
                return {};
            }
            static const FdOps& ops() noexcept {
                static const FdOps kOps{
                    &NullDevice::read,
                    &NullDevice::write,
                    &NullDevice::close,
                    &NullDevice::stat,
                    nullptr
                };
                return kOps;
            }
        };

        struct Handle {
            fs::File file{};
            bool append{false};
            FileService* owner{nullptr};
            util::u32 refs{1};
        };

        static FdFlags flags_to_fd_flags(int flags) noexcept {
            if ((flags & O_RDWR) != 0) return FdFlags::read_write;
            if ((flags & O_WRONLY) != 0) return FdFlags::write_only;
            return FdFlags::read_only;
        }

        static fs::OpenFlags combine_flags(fs::OpenFlags a, fs::OpenFlags b) noexcept {
            const auto av = static_cast<util::u32>(a);
            const auto bv = static_cast<util::u32>(b);
            return static_cast<fs::OpenFlags>(av | bv);
        }

        static util::Result<util::usize> read(void* ctx, MutByteView buf) noexcept {
            auto* handle = static_cast<Handle*>(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            const auto before = handle->file.node.offset;
            auto st = fs::vfs_read(handle->file, buf);
            if (!st) {
                return util::unexpected(st.err);
            }
            const auto after = handle->file.node.offset;
            if (after < before) return util::unexpected(util::Errc::io);
            return static_cast<util::usize>(after - before);
        }

        static util::Result<util::usize> write(void* ctx, ByteView buf) noexcept {
            auto* handle = static_cast<Handle*>(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (handle->append) {
                auto st = fs::vfs_seek(handle->file, handle->file.node.size);
                if (!st) {
                    return util::unexpected(st.err);
                }
            }
            const auto before = handle->file.node.offset;
            auto st = fs::vfs_write(handle->file, buf);
            if (!st) {
                return util::unexpected(st.err);
            }
            const auto after = handle->file.node.offset;
            if (after < before) return util::unexpected(util::Errc::io);
            return static_cast<util::usize>(after - before);
        }

        static util::Result<void> close(void* ctx) noexcept {
            auto* handle = static_cast<Handle*>(ctx);
            if (!handle || !handle->owner) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (handle->refs > 1) {
                --handle->refs;
                return {};
            }
            auto st = fs::vfs_close(handle->file);
            if (!st) {
                return util::unexpected(st.err);
            }
            handle->owner->pool_.destroy(handle);
            return {};
        }

        static util::Result<void> stat(void* ctx, PosixStat& out) noexcept {
            auto* handle = static_cast<Handle*>(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            out.mode = 0;
            out.size = static_cast<util::u64>(handle->file.node.size);
            return {};
        }

        static util::Result<void> dup(void* ctx) noexcept {
            auto* handle = static_cast<Handle*>(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            ++handle->refs;
            return {};
        }

        static const FdOps& ops() noexcept {
            static const FdOps kOps{
                &FileService::read,
                &FileService::write,
                &FileService::close,
                &FileService::stat,
                &FileService::dup
            };
            return kOps;
        }

        FilePool<Handle, MaxFiles> pool_{};
    };

    template <util::usize MaxFiles>
    struct FileServiceBinding {
        FileService<MaxFiles>* service{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit FileServiceBinding(FileService<MaxFiles>& file_service,
                                    const char* cap_name = "posix.file",
                                    init::Phase phase = init::Phase::core,
                                    util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : service(&file_service) {
            provides[0] = init::cap_id(cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                {},
                &FileServiceBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<FileServiceBinding*>(ctx);
            if (!self || !self->service) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->service->init();
            return {};
        }
    };
}
