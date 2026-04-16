module;

#include <array>
#include <cstddef>
#include <span>
#include <string_view>

export module posix.file;

import fs_core;
import fs_vfs;
import init.binding;
import posix.fd_table;
import util.core;
import util.error;

export namespace posix {
    inline constexpr int O_RDONLY = 0x0;
    inline constexpr int O_WRONLY = 0x1;
    inline constexpr int O_RDWR = 0x2;
    inline constexpr int O_ACCMODE = 0x3;
    inline constexpr int O_CREAT = 0x40;
    inline constexpr int O_EXCL = 0x80;
    inline constexpr int O_TRUNC = 0x200;
    inline constexpr int O_APPEND = 0x400;
    inline constexpr int O_NONBLOCK = 0x800;

    inline constexpr util::u32 stat_type_from_node(fs::NodeType type) noexcept {
        switch (type) {
            case fs::NodeType::dir:
                return S_IFDIR;
            case fs::NodeType::device:
                return S_IFCHR;
            case fs::NodeType::file:
            default:
                return S_IFREG;
        }
    }

    inline constexpr util::u32 stat_perm_from_node(fs::NodeType type) noexcept {
        switch (type) {
            case fs::NodeType::dir:
                return kModePermDir;
            case fs::NodeType::device:
                return kModePermChar;
            case fs::NodeType::file:
            default:
                return kModePermFile;
        }
    }

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
            handle->non_block = (flags & O_NONBLOCK) != 0;

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
            if ((flags & O_EXCL) != 0) {
                fs_flags = combine_flags(fs_flags, fs::OpenFlags::excl);
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
            entry.kind = handle->file.node.type == fs::NodeType::device ? FdKind::dev : FdKind::file;
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
                out.mode = make_stat_mode(S_IFCHR, kModePermChar);
                out.size = 0;
                return {};
            }
            static const FdOps& ops() noexcept {
                static const FdOps kOps{
                    &NullDevice::read,
                    &NullDevice::write,
                    &NullDevice::close,
                    &NullDevice::stat,
                    nullptr,
                    nullptr,
                    nullptr,
                    nullptr
                };
                return kOps;
            }
        };

        struct Handle {
            fs::File file{};
            bool append{false};
            bool non_block{false};
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


        static util::Result<int> get_status_flags(void* ctx) noexcept {
            auto* handle = static_cast<Handle*>(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            int flags = 0;
            if (handle->append) flags |= O_APPEND;
            if (handle->non_block) flags |= O_NONBLOCK;
            return flags;
        }

        static util::Result<void> set_status_flags(void* ctx, int flags) noexcept {
            auto* handle = static_cast<Handle*>(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            handle->append = (flags & O_APPEND) != 0;
            handle->non_block = (flags & O_NONBLOCK) != 0;
            return {};
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
            out.mode = make_stat_mode(
                stat_type_from_node(handle->file.node.type),
                stat_perm_from_node(handle->file.node.type));
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

        static util::Result<util::i64> seek(void* ctx, util::i64 offset, int whence) noexcept {
            auto* handle = static_cast<Handle*>(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            util::i64 target = 0;
            switch (whence) {
                case SEEK_SET:
                    target = offset;
                    break;
                case SEEK_CUR:
                    target = handle->file.node.offset + offset;
                    break;
                case SEEK_END:
                    target = handle->file.node.size + offset;
                    break;
                default:
                    return util::unexpected(util::Errc::inval);
            }
            if (target < 0) {
                return util::unexpected(util::Errc::inval);
            }
            auto st = fs::vfs_seek(handle->file, target);
            if (!st) {
                return util::unexpected(st.err);
            }
            return handle->file.node.offset;
        }

        static const FdOps& ops() noexcept {
            static const FdOps kOps{
                &FileService::read,
                &FileService::write,
                &FileService::close,
                &FileService::stat,
                &FileService::dup,
                &FileService::seek,
                &FileService::get_status_flags,
                &FileService::set_status_flags
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
            provides = init::capability_ids(cap_name);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           &FileServiceBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name));
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
