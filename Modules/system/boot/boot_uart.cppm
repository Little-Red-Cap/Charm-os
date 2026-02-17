module;

#include <cstddef>

export module boot_uart;

import util.core;
import util.alias;
import boot_core;
import boot_storage;
import boot_flash;

export namespace boot {
    struct UartFrame {
        util::u16 magic{0xB007};
        util::u16 seq{0};
        util::u32 offset{0};
        util::u16 size{0};
        util::u16 crc{0};
    };

    inline util::u16 crc16(const util::u8* data, util::usize len) noexcept {
        util::u16 crc = 0xFFFF;
        for (util::usize i = 0; i < len; ++i) {
            crc ^= static_cast<util::u16>(data[i]);
            for (int j = 0; j < 8; ++j) {
                if (crc & 1) crc = (crc >> 1) ^ 0xA001;
                else crc >>= 1;
            }
        }
        return crc;
    }

    struct UartRx {
        const util::u8* buf{nullptr};
        util::usize size{0};
    };

    inline bool uart_apply_frame(const Storage& s, FlashConfig cfg, const UartFrame& f, UartRx rx) noexcept {
        if (f.magic != 0xB007) return false;
        if (rx.size < sizeof(UartFrame) + f.size) return false;
        const util::u8* payload = rx.buf + sizeof(UartFrame);
        const auto calc = crc16(payload, f.size);
        if (calc != f.crc) return false;
        return flash_write(s, f.offset, util::span<const util::u8>(payload, f.size), cfg);
    }
}
