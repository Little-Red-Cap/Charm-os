module;

#include <cstddef>
#include <cstdint>
#include <span>

export module fs_block;

import util.core;
import fs_errno;
import fs_stream;

export namespace fs {
    struct BlockDevice {
        Status (*read)(util::u64 lba, std::span<util::u8>) noexcept { nullptr };
        Status (*write)(util::u64 lba, std::span<const util::u8>) noexcept { nullptr };
        Status (*erase)(util::u64 lba, util::u64 count) noexcept { nullptr };
        Status (*flush)() noexcept { nullptr };
        util::u64 block_size{0};
        util::u64 block_count{0};
    };
}
