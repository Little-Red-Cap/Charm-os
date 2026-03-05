//
// Created by Joho on 2026/03/05.
//

module;
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

export module canopen.od;

import util.core;
import util.error;
import canopen.types;

export namespace canopen {
    enum class Access : util::u8 {
        none = 0,
        read = 1,
        write = 2,
        read_write = 3,
    };

    enum class DataType : util::u8 {
        opaque = 0,
        u8,
        u16,
        u32,
        i8,
        i16,
        i32,
        bytes,
        custom,
    };

    enum class EntryFlags : util::u8 {
        none = 0,
        variable_size = 1u << 0,
    };

    constexpr bool has_flag(EntryFlags flags, EntryFlags bit) noexcept {
        return (static_cast<util::u8>(flags) & static_cast<util::u8>(bit)) != 0;
    }

    struct EntryOps {
        util::Result<util::usize> (*read)(void* ctx, std::span<std::byte> out) noexcept { nullptr };
        util::Result<util::usize> (*write)(void* ctx, std::span<const std::byte> in) noexcept { nullptr };
    };

    struct Entry {
        Index index{0};
        SubIndex sub{0};
        util::u32 size{0};
        Access access{Access::none};
        DataType type{DataType::opaque};
        EntryFlags flags{EntryFlags::none};
        void* ctx{nullptr};
        EntryOps ops{};
    };

    class ObjectDictionary {
    public:
        explicit ObjectDictionary(std::span<Entry> entries) noexcept : entries_(entries) {}

        const Entry* find(Index index, SubIndex sub) const noexcept {
            for (auto& e : entries_) {
                if (e.index == index && e.sub == sub) {
                    return &e;
                }
            }
            return nullptr;
        }

        util::Result<util::usize> read(Index index, SubIndex sub, std::span<std::byte> out) const noexcept {
            const Entry* e = find(index, sub);
            if (!e) {
                return util::unexpected(util::Errc::noent);
            }
            if (e->access == Access::none || e->access == Access::write) {
                return util::unexpected(util::Errc::perm);
            }
            if (!e->ops.read) {
                return util::unexpected(util::Errc::not_supported);
            }
            return e->ops.read(e->ctx, out);
        }

        util::Result<util::usize> write(Index index, SubIndex sub, std::span<const std::byte> in) const noexcept {
            const Entry* e = find(index, sub);
            if (!e) {
                return util::unexpected(util::Errc::noent);
            }
            if (e->access == Access::none || e->access == Access::read) {
                return util::unexpected(util::Errc::perm);
            }
            if (!e->ops.write) {
                return util::unexpected(util::Errc::not_supported);
            }
            return e->ops.write(e->ctx, in);
        }

    private:
        std::span<Entry> entries_{};
    };

    struct BytesRef {
        std::span<std::byte> span{};
    };

    struct ConstBytesRef {
        std::span<const std::byte> span{};
    };

    struct BytesVarRef {
        std::span<std::byte> span{};
        util::u32* size{nullptr};
    };

    struct ConstBytesVarRef {
        std::span<const std::byte> span{};
        const util::u32* size{nullptr};
    };

    template <class T>
    constexpr DataType data_type() noexcept {
        using U = std::remove_cv_t<T>;
        if constexpr (std::is_same_v<U, util::u8>) return DataType::u8;
        if constexpr (std::is_same_v<U, util::u16>) return DataType::u16;
        if constexpr (std::is_same_v<U, util::u32>) return DataType::u32;
        if constexpr (std::is_same_v<U, util::i8>) return DataType::i8;
        if constexpr (std::is_same_v<U, util::i16>) return DataType::i16;
        if constexpr (std::is_same_v<U, util::i32>) return DataType::i32;
        return DataType::custom;
    }

    template <class T>
    util::Result<util::usize> read_value(void* ctx, std::span<std::byte> out) noexcept {
        if (!ctx) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (out.size() < sizeof(T)) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const T* value = static_cast<const T*>(ctx);
        std::memcpy(out.data(), value, sizeof(T));
        return static_cast<util::usize>(sizeof(T));
    }

    template <class T>
    util::Result<util::usize> write_value(void* ctx, std::span<const std::byte> in) noexcept {
        if (!ctx) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (in.size() != sizeof(T)) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        T* value = static_cast<T*>(ctx);
        std::memcpy(value, in.data(), sizeof(T));
        return static_cast<util::usize>(sizeof(T));
    }

    inline util::Result<util::usize> read_bytes(void* ctx, std::span<std::byte> out) noexcept {
        if (!ctx) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto* ref = static_cast<const BytesRef*>(ctx);
        if (!ref || ref->span.empty()) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto n = (out.size() < ref->span.size()) ? out.size() : ref->span.size();
        std::memcpy(out.data(), ref->span.data(), n);
        return static_cast<util::usize>(n);
    }

    inline util::Result<util::usize> write_bytes(void* ctx, std::span<const std::byte> in) noexcept {
        if (!ctx) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        auto* ref = static_cast<BytesRef*>(ctx);
        if (!ref || ref->span.empty()) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (in.size() > ref->span.size()) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        const auto n = in.size();
        std::memcpy(ref->span.data(), in.data(), n);
        return static_cast<util::usize>(n);
    }

    inline util::Result<util::usize> read_bytes_ro(void* ctx, std::span<std::byte> out) noexcept {
        if (!ctx) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto* ref = static_cast<const ConstBytesRef*>(ctx);
        if (!ref || ref->span.empty()) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto n = (out.size() < ref->span.size()) ? out.size() : ref->span.size();
        std::memcpy(out.data(), ref->span.data(), n);
        return static_cast<util::usize>(n);
    }

    inline util::Result<util::usize> read_bytes_var(void* ctx, std::span<std::byte> out) noexcept {
        if (!ctx) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto* ref = static_cast<const BytesVarRef*>(ctx);
        if (!ref || ref->span.empty() || !ref->size) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto size = static_cast<util::usize>(*ref->size);
        if (size > ref->span.size()) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        const auto n = (out.size() < size) ? out.size() : size;
        std::memcpy(out.data(), ref->span.data(), n);
        return static_cast<util::usize>(n);
    }

    inline util::Result<util::usize> write_bytes_var(void* ctx, std::span<const std::byte> in) noexcept {
        if (!ctx) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        auto* ref = static_cast<BytesVarRef*>(ctx);
        if (!ref || ref->span.empty() || !ref->size) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (in.size() > ref->span.size()) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        std::memcpy(ref->span.data(), in.data(), in.size());
        *ref->size = static_cast<util::u32>(in.size());
        return static_cast<util::usize>(in.size());
    }

    inline util::Result<util::usize> read_bytes_var_ro(void* ctx, std::span<std::byte> out) noexcept {
        if (!ctx) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto* ref = static_cast<const ConstBytesVarRef*>(ctx);
        if (!ref || ref->span.empty() || !ref->size) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto size = static_cast<util::usize>(*ref->size);
        if (size > ref->span.size()) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        const auto n = (out.size() < size) ? out.size() : size;
        std::memcpy(out.data(), ref->span.data(), n);
        return static_cast<util::usize>(n);
    }

    template <class T>
    Entry make_entry(Index index, SubIndex sub, T& value, Access access) noexcept {
        Entry e{};
        e.index = index;
        e.sub = sub;
        e.size = static_cast<util::u32>(sizeof(T));
        e.access = access;
        e.type = data_type<T>();
        e.ctx = &value;
        e.ops = EntryOps{
            .read = (access == Access::write) ? nullptr : &read_value<T>,
            .write = (access == Access::read) ? nullptr : &write_value<T>,
        };
        return e;
    }

    template <class T>
    Entry make_entry_ro(Index index, SubIndex sub, const T& value) noexcept {
        Entry e{};
        e.index = index;
        e.sub = sub;
        e.size = static_cast<util::u32>(sizeof(T));
        e.access = Access::read;
        e.type = data_type<T>();
        e.ctx = const_cast<T*>(&value);
        e.ops = EntryOps{
            .read = &read_value<T>,
            .write = nullptr,
        };
        return e;
    }

    inline Entry make_bytes_entry(Index index,
                                  SubIndex sub,
                                  BytesRef& ref,
                                  Access access,
                                  bool variable_size = false) noexcept {
        Entry e{};
        e.index = index;
        e.sub = sub;
        e.size = static_cast<util::u32>(ref.span.size());
        e.access = access;
        e.type = DataType::bytes;
        e.flags = variable_size ? EntryFlags::variable_size : EntryFlags::none;
        e.ctx = &ref;
        e.ops = EntryOps{
            .read = (access == Access::write) ? nullptr : &read_bytes,
            .write = (access == Access::read) ? nullptr : &write_bytes,
        };
        return e;
    }

    inline Entry make_bytes_entry_ro(Index index,
                                     SubIndex sub,
                                     ConstBytesRef& ref,
                                     bool variable_size = false) noexcept {
        Entry e{};
        e.index = index;
        e.sub = sub;
        e.size = static_cast<util::u32>(ref.span.size());
        e.access = Access::read;
        e.type = DataType::bytes;
        e.flags = variable_size ? EntryFlags::variable_size : EntryFlags::none;
        e.ctx = &ref;
        e.ops = EntryOps{
            .read = &read_bytes_ro,
            .write = nullptr,
        };
        return e;
    }

    inline Entry make_bytes_var_entry(Index index, SubIndex sub, BytesVarRef& ref, Access access) noexcept {
        Entry e{};
        e.index = index;
        e.sub = sub;
        e.size = static_cast<util::u32>(ref.span.size());
        e.access = access;
        e.type = DataType::bytes;
        e.flags = EntryFlags::variable_size;
        e.ctx = &ref;
        e.ops = EntryOps{
            .read = (access == Access::write) ? nullptr : &read_bytes_var,
            .write = (access == Access::read) ? nullptr : &write_bytes_var,
        };
        return e;
    }

    inline Entry make_bytes_var_entry_ro(Index index, SubIndex sub, ConstBytesVarRef& ref) noexcept {
        Entry e{};
        e.index = index;
        e.sub = sub;
        e.size = static_cast<util::u32>(ref.span.size());
        e.access = Access::read;
        e.type = DataType::bytes;
        e.flags = EntryFlags::variable_size;
        e.ctx = &ref;
        e.ops = EntryOps{
            .read = &read_bytes_var_ro,
            .write = nullptr,
        };
        return e;
    }

    inline Entry make_custom_entry(Index index,
                                   SubIndex sub,
                                   util::u32 size,
                                   Access access,
                                   void* ctx,
                                   EntryOps ops,
                                   DataType type = DataType::custom,
                                   EntryFlags flags = EntryFlags::none) noexcept {
        Entry e{};
        e.index = index;
        e.sub = sub;
        e.size = size;
        e.access = access;
        e.type = type;
        e.flags = flags;
        e.ctx = ctx;
        e.ops = ops;
        return e;
    }
} // namespace canopen
