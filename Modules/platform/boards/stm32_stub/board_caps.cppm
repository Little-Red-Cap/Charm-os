module;

#include <array>
#include <span>

export module platform.board.stm32_stub;

import platform.board;
import hal_core;
import hal_i2c;
import hal_input;
import hal_uart;
import hal_stm32_stub;
import io.channel;
import util.core;

namespace platform::board::stm32_stub::detail {
    struct Stm32UartCtx {
        hal::UartHandle handle{};
    };

    struct Stm32I2cCtx {
        hal::I2cHandle handle{};
    };

    hal::Result stm32_uart_init(void* ctx, const hal::UartConfig& cfg) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        return hal::stm32_stub::Uart::init(self->handle, cfg);
    }

    hal::Result stm32_uart_enable(void* ctx) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        return hal::stm32_stub::Uart::enable(self->handle);
    }

    hal::Result stm32_uart_disable(void* ctx) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        return hal::stm32_stub::Uart::disable(self->handle);
    }

    hal::Result stm32_uart_try_write(void* ctx, util::u8 byte) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        return hal::stm32_stub::Uart::try_write(self->handle, byte);
    }

    hal::Result stm32_uart_try_read(void* ctx, util::u8& byte) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        return hal::stm32_stub::Uart::try_read(self->handle, byte);
    }

    void stm32_uart_enable_irq(void* ctx, util::u32 mask) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        hal::stm32_stub::Uart::enable_irq(self->handle, mask);
    }

    void stm32_uart_disable_irq(void* ctx, util::u32 mask) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        hal::stm32_stub::Uart::disable_irq(self->handle, mask);
    }

    void stm32_uart_clear_irq(void* ctx, util::u32 mask) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        hal::stm32_stub::Uart::clear_irq(self->handle, mask);
    }

    hal::Result stm32_i2c_init(void* ctx, const hal::I2cConfig& cfg) noexcept {
        auto* self = static_cast<Stm32I2cCtx*>(ctx);
        return hal::stm32_stub::I2c::init(self->handle, cfg);
    }

    hal::Result stm32_i2c_enable(void* ctx) noexcept {
        auto* self = static_cast<Stm32I2cCtx*>(ctx);
        return hal::stm32_stub::I2c::enable(self->handle);
    }

    hal::Result stm32_i2c_disable(void* ctx) noexcept {
        auto* self = static_cast<Stm32I2cCtx*>(ctx);
        return hal::stm32_stub::I2c::disable(self->handle);
    }

    hal::Result stm32_i2c_write(void* ctx,
                                util::u16 addr,
                                std::span<const util::u8> data) noexcept {
        auto* self = static_cast<Stm32I2cCtx*>(ctx);
        return hal::stm32_stub::I2c::write(self->handle, addr, data);
    }

    hal::Result stm32_i2c_read(void* ctx,
                               util::u16 addr,
                               std::span<util::u8> data) noexcept {
        auto* self = static_cast<Stm32I2cCtx*>(ctx);
        return hal::stm32_stub::I2c::read(self->handle, addr, data);
    }

    hal::Result stm32_i2c_write_read(void* ctx,
                                     util::u16 addr,
                                     std::span<const util::u8> tx,
                                     std::span<util::u8> rx) noexcept {
        auto* self = static_cast<Stm32I2cCtx*>(ctx);
        return hal::stm32_stub::I2c::write_read(self->handle, addr, tx, rx);
    }

    util::u64 stm32_now_ms(void*) noexcept {
        return 0;
    }

    util::u64 stm32_now_us(void*) noexcept {
        return 0;
    }

    struct ByteRing {
        std::array<util::u8, 512> buf{};
        util::usize head{0};
        util::usize tail{0};
        util::usize count{0};

        util::usize push(io::ByteView in) noexcept {
            util::usize n = 0;
            while (n < in.size() && count < buf.size()) {
                buf[tail] = in[n++];
                tail = (tail + 1) % buf.size();
                ++count;
            }
            return n;
        }

        util::usize pop(io::MutByteView out) noexcept {
            util::usize n = 0;
            while (n < out.size() && count > 0) {
                out[n++] = buf[head];
                head = (head + 1) % buf.size();
                --count;
            }
            return n;
        }
    };

    struct CanLoopbackCtx {
        ByteRing ring{};
    };

    io::result stm32_can_read(void* ctx, io::MutByteView buf) noexcept {
        if (!ctx || buf.empty()) return io::fail(io::errc::invalid_arg);
        auto* self = static_cast<CanLoopbackCtx*>(ctx);
        const auto n = self->ring.pop(buf);
        if (n == 0) return io::fail(io::errc::would_block);
        return io::ok(n);
    }

    io::result stm32_can_write(void* ctx, io::ByteView buf) noexcept {
        if (!ctx || buf.empty()) return io::fail(io::errc::invalid_arg);
        auto* self = static_cast<CanLoopbackCtx*>(ctx);
        const auto n = self->ring.push(buf);
        if (n == 0) return io::fail(io::errc::would_block);
        return io::ok(n);
    }

    io::result stm32_can_flush(void* ctx) noexcept {
        if (!ctx) return io::fail(io::errc::invalid_arg);
        return io::ok(0);
    }
} // namespace platform::board::stm32_stub::detail

export namespace platform::board::stm32_stub {
    inline BootBoardCaps make_boot_caps() noexcept {
        return {};
    }

    inline ConsoleCaps make_console_caps() noexcept {
        static detail::Stm32UartCtx uart1_ctx{hal::UartHandle{1, nullptr}};
        static const hal::UartOps kStm32UartOps{
            &detail::stm32_uart_init,
            &detail::stm32_uart_enable,
            &detail::stm32_uart_disable,
            &detail::stm32_uart_try_write,
            &detail::stm32_uart_try_read,
            &detail::stm32_uart_enable_irq,
            &detail::stm32_uart_disable_irq,
            &detail::stm32_uart_clear_irq
        };
        ConsoleCaps caps{};
        caps.uart.handle = hal::UartIoHandle{&uart1_ctx, &kStm32UartOps};
        caps.uart.config = hal::UartConfig{};
        caps.uart.io_cap = "io.uart1";
        caps.uart.hal_cap = "hal.uart1";
        caps.clock = ClockDesc{nullptr, &detail::stm32_now_ms, &detail::stm32_now_us};
        caps.console_cap = "io.console0";
        return caps;
    }

    inline InputCaps make_input_caps() noexcept {
        static const hal::RawInputDriver kStm32RawInput = hal::stm32_stub::RawInput::driver();
        InputCaps caps{};
        caps.input.driver = &kStm32RawInput;
        caps.clock = ClockDesc{nullptr, &detail::stm32_now_ms, &detail::stm32_now_us};
        return caps;
    }

    inline BlockCaps make_block_caps() noexcept {
        BlockCaps caps{};
        caps.clock = ClockDesc{nullptr, &detail::stm32_now_ms, &detail::stm32_now_us};
        return caps;
    }

    inline BoardCaps make_board_caps() noexcept {
        static detail::Stm32UartCtx uart1_ctx{hal::UartHandle{1, nullptr}};
        static const hal::UartOps kStm32UartOps{
            &detail::stm32_uart_init,
            &detail::stm32_uart_enable,
            &detail::stm32_uart_disable,
            &detail::stm32_uart_try_write,
            &detail::stm32_uart_try_read,
            &detail::stm32_uart_enable_irq,
            &detail::stm32_uart_disable_irq,
            &detail::stm32_uart_clear_irq
        };
        static const hal::RawInputDriver kStm32RawInput = hal::stm32_stub::RawInput::driver();
        static detail::Stm32I2cCtx i2c1_ctx{hal::I2cHandle{1, nullptr}};
        static const hal::I2cOps kStm32I2cOps{
            &detail::stm32_i2c_init,
            &detail::stm32_i2c_enable,
            &detail::stm32_i2c_disable,
            &detail::stm32_i2c_write,
            &detail::stm32_i2c_read,
            &detail::stm32_i2c_write_read
        };
        static detail::CanLoopbackCtx can0_ctx{};
        static io::Channel can0_channel{
            &can0_ctx,
            io::ChannelOps{&detail::stm32_can_read, &detail::stm32_can_write, &detail::stm32_can_flush}
        };
        BoardCaps caps{};
        caps.uart1.handle = hal::UartIoHandle{&uart1_ctx, &kStm32UartOps};
        caps.uart1.config = hal::UartConfig{};
        caps.uart1.io_cap = "io.uart1";
        caps.uart1.hal_cap = "hal.uart1";
        caps.clock = ClockDesc{nullptr, &detail::stm32_now_ms, &detail::stm32_now_us};
        caps.console_cap = "io.console0";
        caps.input.driver = &kStm32RawInput;
        caps.i2c1.handle = hal::I2cIoHandle{&i2c1_ctx, &kStm32I2cOps};
        caps.i2c1.config = hal::I2cConfig{};
        caps.i2c1.hal_cap = "hal.i2c1";
        caps.can0.channel = &can0_channel;
        caps.can0.io_cap = "io.can0";
        return with_boot_caps(caps, make_boot_caps());
    }
}
