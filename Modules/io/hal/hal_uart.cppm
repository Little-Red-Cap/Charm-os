module;

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <span>

export module hal_uart;

import hal_core;
import util.core;

export namespace hal {
    enum class Parity : util::u8 { none, even, odd };
    enum class StopBits : util::u8 { one, two };

    struct UartConfig {
        util::u32 baud{115200};
        util::u8 data_bits{8};
        Parity parity{Parity::none};
        StopBits stop_bits{StopBits::one};
    };

    struct UartHandle {
        util::usize id{0};
        void* impl{nullptr};
    };

    struct UartOps {
        Result (*init)(void* ctx, const UartConfig& cfg) noexcept { nullptr };
        Result (*write)(void* ctx, std::span<const util::u8> tx) noexcept { nullptr };
        Result (*read)(void* ctx, std::span<util::u8> rx) noexcept { nullptr };
    };

    struct UartIoHandle {
        void* ctx{nullptr};
        const UartOps* ops{nullptr};
    };

    inline Result uart_init(UartIoHandle h, const UartConfig& cfg) noexcept {
        if (!h.ops || !h.ops->init) return err(Status::unsupported);
        return h.ops->init(h.ctx, cfg);
    }

    inline Result uart_write(UartIoHandle h, std::span<const util::u8> tx) noexcept {
        if (!h.ops || !h.ops->write) return err(Status::unsupported);
        return h.ops->write(h.ctx, tx);
    }

    inline Result uart_read(UartIoHandle h, std::span<util::u8> rx) noexcept {
        if (!h.ops || !h.ops->read) return err(Status::unsupported);
        return h.ops->read(h.ctx, rx);
    }

    template <typename T>
    concept UartDriver = requires(UartHandle h, UartConfig cfg, std::span<const util::u8> tx, std::span<util::u8> rx) {
        { T::init(h, cfg) } -> std::same_as<Result>;
        { T::write(h, tx) } -> std::same_as<Result>;
        { T::read(h, rx) } -> std::same_as<Result>;
    };
}
