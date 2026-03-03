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

    enum class UartIrq : util::u32 {
        rx = 1u << 0,
        tx = 1u << 1,
        err = 1u << 2,
    };

    constexpr util::u32 operator|(UartIrq a, UartIrq b) noexcept {
        return static_cast<util::u32>(a) | static_cast<util::u32>(b);
    }

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
        Result (*enable)(void* ctx) noexcept { nullptr };
        Result (*disable)(void* ctx) noexcept { nullptr };
        Result (*try_write)(void* ctx, util::u8 byte) noexcept { nullptr };
        Result (*try_read)(void* ctx, util::u8& byte) noexcept { nullptr };
        void (*enable_irq)(void* ctx, util::u32 mask) noexcept { nullptr };
        void (*disable_irq)(void* ctx, util::u32 mask) noexcept { nullptr };
        void (*clear_irq)(void* ctx, util::u32 mask) noexcept { nullptr };
    };

    struct UartIoHandle {
        void* ctx{nullptr};
        const UartOps* ops{nullptr};
    };

    inline Result uart_init(UartIoHandle h, const UartConfig& cfg) noexcept {
        if (!h.ops || !h.ops->init) return err(Status::unsupported);
        return h.ops->init(h.ctx, cfg);
    }

    inline Result uart_enable(UartIoHandle h) noexcept {
        if (!h.ops || !h.ops->enable) return err(Status::unsupported);
        return h.ops->enable(h.ctx);
    }

    inline Result uart_disable(UartIoHandle h) noexcept {
        if (!h.ops || !h.ops->disable) return err(Status::unsupported);
        return h.ops->disable(h.ctx);
    }

    inline Result uart_try_write(UartIoHandle h, util::u8 byte) noexcept {
        if (!h.ops || !h.ops->try_write) return err(Status::unsupported);
        return h.ops->try_write(h.ctx, byte);
    }

    inline Result uart_try_read(UartIoHandle h, util::u8& byte) noexcept {
        if (!h.ops || !h.ops->try_read) return err(Status::unsupported);
        return h.ops->try_read(h.ctx, byte);
    }

    inline void uart_enable_irq(UartIoHandle h, util::u32 mask) noexcept {
        if (!h.ops || !h.ops->enable_irq) return;
        h.ops->enable_irq(h.ctx, mask);
    }

    inline void uart_disable_irq(UartIoHandle h, util::u32 mask) noexcept {
        if (!h.ops || !h.ops->disable_irq) return;
        h.ops->disable_irq(h.ctx, mask);
    }

    inline void uart_clear_irq(UartIoHandle h, util::u32 mask) noexcept {
        if (!h.ops || !h.ops->clear_irq) return;
        h.ops->clear_irq(h.ctx, mask);
    }

    template <typename T>
    concept UartDriver = requires(UartHandle h, UartConfig cfg, util::u8 b) {
        { T::init(h, cfg) } -> std::same_as<Result>;
        { T::enable(h) } -> std::same_as<Result>;
        { T::disable(h) } -> std::same_as<Result>;
        { T::try_write(h, b) } -> std::same_as<Result>;
        { T::try_read(h, b) } -> std::same_as<Result>;
    };
}
