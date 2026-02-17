module;

#include <cstddef>

export module boot_flash;

import util.core;
import util.alias;
import boot_storage;

export namespace boot {
    struct FlashConfig {
        util::u32 erase_size{4096};
        util::u32 write_size{256};
    };

    inline util::u32 align_down(util::u32 v, util::u32 align) noexcept {
        return align == 0 ? v : (v / align) * align;
    }

    inline util::u32 align_up(util::u32 v, util::u32 align) noexcept {
        if (align == 0) return v;
        const util::u32 rem = v % align;
        return rem == 0 ? v : (v + (align - rem));
    }

    inline bool flash_write(const Storage& s, util::u32 offset,
                            util::span<const util::u8> data, FlashConfig cfg) noexcept {
        const util::u32 start = align_down(offset, cfg.erase_size);
        const util::u32 end = align_up(offset + static_cast<util::u32>(data.size()), cfg.erase_size);
        if (!storage_erase(s, start, end - start)) return false;
        util::u32 written = 0;
        while (written < data.size()) {
            const auto chunk = cfg.write_size == 0 ? data.size() - written
                : (data.size() - written > cfg.write_size ? cfg.write_size : data.size() - written);
            if (!storage_write(s, offset + written,
                               util::span<const util::u8>(data.data() + written, chunk))) {
                return false;
            }
            written += static_cast<util::u32>(chunk);
        }
        return true;
    }
}
