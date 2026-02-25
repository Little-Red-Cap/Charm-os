module;

#include <cstddef>

export module boot_uart;

import util.core;
import util.alias;
import boot_core;
import boot_storage;
import boot_flash;

export namespace boot {
    struct UartPolicy {
        Partition allowed{};
        bool enforce_range{false};
        bool enforce_seq{false};
        bool require_unlocked{false};
    };

    struct UartState {
        bool unlocked{false};
        util::u16 last_seq{0};
    };

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

    inline bool uart_apply_frame_policy(const Storage& s, FlashConfig cfg, const UartFrame& f,
                                        UartRx rx, const UartPolicy& policy, UartState& state) noexcept {
        if (f.magic != 0xB007) return false;
        if (policy.require_unlocked && !state.unlocked) return false;
        if (policy.enforce_range) {
            const util::u32 end = f.offset + f.size;
            const util::u32 allowed_end = policy.allowed.offset + policy.allowed.size;
            if (f.offset < policy.allowed.offset || end > allowed_end) return false;
        }
        if (policy.enforce_seq) {
            if (f.seq == state.last_seq) return false;
            if (state.last_seq != 0) {
                const util::u16 expected = static_cast<util::u16>(state.last_seq + 1);
                if (f.seq != expected) return false;
            }
        }
        if (rx.size < sizeof(UartFrame) + f.size) return false;
        const util::u8* payload = rx.buf + sizeof(UartFrame);
        const auto calc = crc16(payload, f.size);
        if (calc != f.crc) return false;
        if (!flash_write(s, f.offset, util::span<const util::u8>(payload, f.size), cfg)) return false;
        if (policy.enforce_seq) {
            state.last_seq = f.seq;
        }
        return true;
    }

    inline bool uart_apply_frame(const Storage& s, FlashConfig cfg, const UartFrame& f, UartRx rx) noexcept {
        UartPolicy policy{};
        UartState state{};
        return uart_apply_frame_policy(s, cfg, f, rx, policy, state);
    }
}
