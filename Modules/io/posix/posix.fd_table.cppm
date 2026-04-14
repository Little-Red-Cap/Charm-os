module;

#include <array>
#include <cstddef>
#include <span>

export module posix.fd_table;

import init.node;
import util.core;
import util.error;

export namespace posix {
    using ByteView = std::span<const util::u8>;
    using MutByteView = std::span<util::u8>;

    inline constexpr util::u32 S_IFMT = 0170000u;
    inline constexpr util::u32 S_IFIFO = 0010000u;
    inline constexpr util::u32 S_IFCHR = 0020000u;
    inline constexpr util::u32 S_IFDIR = 0040000u;
    inline constexpr util::u32 S_IFREG = 0100000u;

    inline constexpr int SEEK_SET = 0;
    inline constexpr int SEEK_CUR = 1;
    inline constexpr int SEEK_END = 2;

    inline constexpr util::u32 kModePermFile = 0644u;
    inline constexpr util::u32 kModePermDir = 0755u;
    inline constexpr util::u32 kModePermChar = 0666u;
    inline constexpr util::u32 kModePermPipe = 0600u;

    inline constexpr util::u32 make_stat_mode(util::u32 type_bits, util::u32 perm_bits) noexcept {
        return type_bits | perm_bits;
    }

    struct PosixStat {
        util::u32 mode{0};
        util::u64 size{0};
    };

    enum class FdKind : util::u8 {
        file,
        pipe,
        term,
        dev,
        proc,
    };

    enum class FdFlags : util::u16 {
        none = 0,
        non_block = 1u << 0,
        read_only = 1u << 1,
        write_only = 1u << 2,
        read_write = 1u << 3,
    };

    constexpr FdFlags operator|(FdFlags a, FdFlags b) noexcept {
        return static_cast<FdFlags>(
            static_cast<util::u16>(a) | static_cast<util::u16>(b));
    }

    struct FdOps {
        util::Result<util::usize> (*read)(void* ctx, MutByteView) noexcept {nullptr};
        util::Result<util::usize> (*write)(void* ctx, ByteView) noexcept {nullptr};
        util::Result<void> (*close)(void* ctx) noexcept {nullptr};
        util::Result<void> (*stat)(void* ctx, PosixStat& out) noexcept {nullptr};
        util::Result<void> (*dup)(void* ctx) noexcept {nullptr};
        util::Result<util::i64> (*seek)(void* ctx, util::i64 offset, int whence) noexcept {nullptr};
    };

    struct FdEntry {
        int id{-1};
        FdKind kind{FdKind::file};
        FdFlags flags{FdFlags::none};
        const FdOps* ops{nullptr};
        void* ctx{nullptr};
        bool inheritable{true};
    };

    template <util::usize MaxFds>
    struct FdTableSnapshot {
        std::array<FdEntry, MaxFds> slots{};
        std::array<bool, MaxFds> used{};
    };

    template <util::usize MaxFds>
    class FdTable {
    public:
        void init() noexcept { clear(); }

        util::Result<int> attach(FdEntry entry, int desired = -1) noexcept {
            if (entry.ops == nullptr) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (desired >= 0) {
                const util::usize idx = static_cast<util::usize>(desired);
                if (idx >= MaxFds) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                if (used_[idx]) {
                    return util::unexpected(util::Errc::exist);
                }
                entry.id = desired;
                slots_[idx] = entry;
                used_[idx] = true;
                return desired;
            }

            for (util::usize i = 0; i < MaxFds; ++i) {
                if (used_[i]) continue;
                entry.id = static_cast<int>(i);
                slots_[i] = entry;
                used_[i] = true;
                return static_cast<int>(i);
            }
            return util::unexpected(util::Errc::buffer_overflow);
        }

        util::Result<void> clone_to(FdTable<MaxFds>& out) const noexcept {
            out.clear();
            for (util::usize i = 0; i < MaxFds; ++i) {
                if (!used_[i]) continue;
                const auto& entry = slots_[i];
                if (!entry.inheritable) continue;
                if (entry.ops && entry.ops->dup) {
                    auto rdup = entry.ops->dup(entry.ctx);
                    if (!rdup) return util::unexpected(rdup.error());
                }
                auto r = out.attach(entry, static_cast<int>(i));
                if (!r) return util::unexpected(r.error());
            }
            return {};
        }

        util::Result<void> close(int fd) noexcept {
            auto* entry = get_ptr(fd);
            if (!entry) {
                return util::unexpected(util::Errc::noent);
            }
            if (entry->ops && entry->ops->close) {
                auto r = entry->ops->close(entry->ctx);
                if (!r) return r;
            }
            const util::usize idx = static_cast<util::usize>(fd);
            slots_[idx] = {};
            used_[idx] = false;
            return {};
        }

        util::Result<void> dup2(int from, int to) noexcept {
            auto* src = get_ptr(from);
            if (!src) {
                return util::unexpected(util::Errc::noent);
            }
            if (to < 0 || static_cast<util::usize>(to) >= MaxFds) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (from == to) {
                return {};
            }
            if (used_[static_cast<util::usize>(to)]) {
                auto r = close(to);
                if (!r) return r;
            }
            if (src->ops && src->ops->dup) {
                auto rdup = src->ops->dup(src->ctx);
                if (!rdup) return rdup;
            }
            FdEntry copy = *src;
            copy.id = to;
            copy.inheritable = true;
            slots_[static_cast<util::usize>(to)] = copy;
            used_[static_cast<util::usize>(to)] = true;
            return {};
        }

        util::Result<int> dup(int from) noexcept {
            return dup(from, 0);
        }

        util::Result<int> dup(int from, int min_fd) noexcept {
            auto* src = get_ptr(from);
            if (!src) {
                return util::unexpected(util::Errc::noent);
            }
            if (min_fd < 0 || static_cast<util::usize>(min_fd) >= MaxFds) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = static_cast<util::usize>(min_fd); i < MaxFds; ++i) {
                if (used_[i]) continue;
                if (src->ops && src->ops->dup) {
                    auto rdup = src->ops->dup(src->ctx);
                    if (!rdup) return util::unexpected(rdup.error());
                }
                FdEntry copy = *src;
                copy.id = static_cast<int>(i);
                copy.inheritable = true;
                slots_[i] = copy;
                used_[i] = true;
                return copy.id;
            }
            return util::unexpected(util::Errc::buffer_overflow);
        }

        util::Result<FdEntry*> get(int fd) noexcept {
            auto* entry = get_ptr(fd);
            if (!entry) {
                return util::unexpected(util::Errc::noent);
            }
            return entry;
        }

        util::Result<FdEntry> clone_entry(int fd) noexcept {
            auto* entry = get_ptr(fd);
            if (!entry) {
                return util::unexpected(util::Errc::noent);
            }
            FdEntry copy = *entry;
            copy.id = -1;
            return copy;
        }

        void clear() noexcept {
            for (util::usize i = 0; i < MaxFds; ++i) {
                slots_[i] = {};
                used_[i] = false;
            }
        }

        void close_all() noexcept {
            for (util::usize i = 0; i < MaxFds; ++i) {
                if (!used_[i]) continue;
                const auto& entry = slots_[i];
                if (entry.ops && entry.ops->close) {
                    (void)entry.ops->close(entry.ctx);
                }
                slots_[i] = {};
                used_[i] = false;
            }
        }

        void snapshot(FdTableSnapshot<MaxFds>& out) const noexcept {
            out.slots = slots_;
            out.used = used_;
        }

    private:
        FdEntry* get_ptr(int fd) noexcept {
            if (fd < 0) return nullptr;
            const util::usize idx = static_cast<util::usize>(fd);
            if (idx >= MaxFds) return nullptr;
            if (!used_[idx]) return nullptr;
            return &slots_[idx];
        }

        std::array<FdEntry, MaxFds> slots_{};
        std::array<bool, MaxFds> used_{};
    };

    template <util::usize MaxFds>
    struct FdTableBinding {
        FdTable<MaxFds>* table{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit FdTableBinding(FdTable<MaxFds>& fd_table,
                                const char* cap_name = "posix.fd_table",
                                init::Phase phase = init::Phase::core,
                                util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : table(&fd_table) {
            provides[0] = init::cap_id(cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                {},
                &FdTableBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<FdTableBinding*>(ctx);
            if (!self || !self->table) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->table->init();
            return {};
        }
    };
}
