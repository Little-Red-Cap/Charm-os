module;

#include <cstddef>
#include <cstdint>

export module boot_core;

import util.core;

export namespace boot {
    constexpr util::u32 k_magic = 0x424F4F54; // 'BOOT'
    constexpr util::u16 k_version = 1;

    enum class ImageFlags : util::u16 {
        none = 0,
        compressed = 1 << 0,
        signed_image = 1 << 1
    };

    struct ImageHeader {
        util::u32 magic{k_magic};
        util::u16 version{k_version};
        util::u16 flags{0};
        util::u32 image_size{0};
        util::u32 payload_size{0};
        util::u32 payload_crc32{0};
        util::u32 entry_offset{0};
        util::u32 image_version{0};
        util::u32 min_version{0};
        util::u32 signature{0};
    };

    enum class Slot : util::u8 { a = 0, b = 1 };
    enum class BootStatus : util::u8 { ok = 0, invalid, io_error };

    struct BootResult {
        BootStatus status{BootStatus::invalid};
        Slot slot{Slot::a};
    };

    struct Partition {
        util::u32 offset{0};
        util::u32 size{0};
    };

    struct BootInfo {
        Slot active{Slot::a};
        Slot pending{Slot::a};
        util::u32 flags{0};
        util::u32 counter{0};
        util::u32 last_good_version{0};
        util::u32 min_version{0};
    };

    inline util::u32 crc32_update(util::u32 crc, const util::u8* data, util::usize len) noexcept {
        crc = ~crc;
        for (util::usize i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j) {
                const util::u32 mask = -(crc & 1u);
                crc = (crc >> 1) ^ (0xEDB88320u & mask);
            }
        }
        return ~crc;
    }
}
