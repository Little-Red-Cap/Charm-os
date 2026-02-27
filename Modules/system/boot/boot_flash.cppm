module;

#include <span>
#include <cstddef>
#include <cstring>

export module boot_flash;

import util.core;
import boot_storage;

export namespace boot {
    struct FlashConfig {
        util::u32 erase_size{4096};
        util::u32 write_size{256};
        util::u8* scratch{nullptr};
        util::u32 scratch_size{0};
    };

    inline util::u32 align_down(util::u32 v, util::u32 align) noexcept {
        return align == 0 ? v : (v / align) * align;
    }

    inline util::u32 align_up(util::u32 v, util::u32 align) noexcept {
        if (align == 0) return v;
        const util::u32 rem = v % align;
        return rem == 0 ? v : (v + (align - rem));
    }

    inline bool flash_write_chunked(const Storage& s, util::u32 dst,
                                    std::span<const util::u8> src, FlashConfig cfg) noexcept {
        util::u32 written = 0;
        const util::u32 total = static_cast<util::u32>(src.size());
        while (written < total) {
            const util::u32 remain = total - written;
            const util::u32 chunk = (cfg.write_size == 0 || remain <= cfg.write_size)
                ? remain
                : cfg.write_size;
            if (!storage_write(s, dst + written,
                std::span<const util::u8>(src.data() + written, chunk))) {
                return false;
            }
            written += chunk;
        }
        return true;
    }

    inline bool flash_write(const Storage& s, util::u32 offset,
                            std::span<const util::u8> data, FlashConfig cfg) noexcept {
        if (data.empty()) return true;
        if (cfg.erase_size == 0) return false;
        const util::u32 start = align_down(offset, cfg.erase_size);
        const util::u32 end = align_up(offset + static_cast<util::u32>(data.size()), cfg.erase_size);
        const bool full_block_write = (offset % cfg.erase_size == 0) &&
                                      (data.size() % cfg.erase_size == 0);

        for (util::u32 blk = start; blk < end; blk += cfg.erase_size) {
            const util::u32 blk_end = blk + cfg.erase_size;
            const util::u32 data_start = offset;
            const util::u32 data_end = offset + static_cast<util::u32>(data.size());
            const util::u32 seg_start = (data_start > blk) ? data_start : blk;
            const util::u32 seg_end = (data_end < blk_end) ? data_end : blk_end;
            if (seg_start >= seg_end) continue;

            if (!full_block_write) {
                if (!cfg.scratch || cfg.scratch_size < cfg.erase_size) return false;
                auto scratch = std::span<util::u8>(cfg.scratch, cfg.erase_size);
                if (!storage_read(s, blk, scratch)) return false;
                const util::u32 seg_len = seg_end - seg_start;
                const util::u32 data_off = seg_start - offset;
                std::memcpy(scratch.data() + (seg_start - blk), data.data() + data_off, seg_len);
                if (!storage_erase(s, blk, cfg.erase_size)) return false;
                if (!flash_write_chunked(s, blk, scratch, cfg)) return false;
            } else {
                if (!storage_erase(s, blk, cfg.erase_size)) return false;
                const util::u32 data_off = blk - offset;
                auto block_span = std::span<const util::u8>(data.data() + data_off, cfg.erase_size);
                if (!flash_write_chunked(s, blk, block_span, cfg)) return false;
            }
        }
        return true;
    }
}
