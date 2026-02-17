module;

#include <cstddef>

export module boot_storage;

import util.core;
import util.alias;

export namespace boot {
    struct Storage {
        void* ctx{nullptr};
        bool (*read)(void* ctx, util::u32 offset, util::span<util::u8> out) noexcept { nullptr };
        bool (*write)(void* ctx, util::u32 offset, util::span<const util::u8> in) noexcept { nullptr };
        bool (*erase)(void* ctx, util::u32 offset, util::u32 size) noexcept { nullptr };
    };

    inline bool storage_read(const Storage& s, util::u32 offset, util::span<util::u8> out) noexcept {
        return s.read ? s.read(s.ctx, offset, out) : false;
    }

    inline bool storage_write(const Storage& s, util::u32 offset, util::span<const util::u8> in) noexcept {
        return s.write ? s.write(s.ctx, offset, in) : false;
    }

    inline bool storage_erase(const Storage& s, util::u32 offset, util::u32 size) noexcept {
        return s.erase ? s.erase(s.ctx, offset, size) : false;
    }
}
