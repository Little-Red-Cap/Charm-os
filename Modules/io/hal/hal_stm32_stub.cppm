module;

#include <cstdint>
#include <optional>
#include <span>

export module hal_stm32_stub;

import hal_core;
import hal_gpio;
import hal_input;
import hal_i2c;
import hal_spi;
import hal_uart;
import hal_timer;
import input.raw;
import util.core;

// Stub implementations to keep the build/link pipeline working.
export namespace hal::stm32_stub {
    struct Delay {
        static void delay_ms(tick_t) noexcept {}
    };

    struct Gpio {
        static Result init(GpioPin, GpioConfig) noexcept { return ok(); }
        static Result write(GpioPin, GpioLevel) noexcept { return ok(); }
        static GpioLevel read(GpioPin) noexcept { return GpioLevel::low; }
    };

    struct Uart {
        static Result init(UartHandle, UartConfig) noexcept { return ok(); }
        static Result enable(UartHandle) noexcept { return ok(); }
        static Result disable(UartHandle) noexcept { return ok(); }
        static Result try_write(UartHandle, util::u8) noexcept { return err(Status::busy); }
        static Result try_read(UartHandle, util::u8&) noexcept { return err(Status::busy); }
        static void enable_irq(UartHandle, util::u32) noexcept {}
        static void disable_irq(UartHandle, util::u32) noexcept {}
        static void clear_irq(UartHandle, util::u32) noexcept {}
    };

    struct Timer {
        static Result init(TimerHandle, TimerConfig) noexcept { return ok(); }
        static Result start(TimerHandle) noexcept { return ok(); }
        static Result stop(TimerHandle) noexcept { return ok(); }
        static Result set_callback(TimerHandle, TimerCallback) noexcept { return ok(); }
    };

    struct Spi {
        static Result init(SpiHandle, SpiConfig) noexcept { return err(Status::busy); }
        static Result enable(SpiHandle) noexcept { return err(Status::busy); }
        static Result disable(SpiHandle) noexcept { return err(Status::busy); }
        static Result transfer(SpiHandle,
                               std::span<const util::u8>,
                               std::span<util::u8>) noexcept { return err(Status::busy); }
    };

    struct I2c {
        static Result init(I2cHandle, I2cConfig) noexcept { return ok(); }
        static Result enable(I2cHandle) noexcept { return ok(); }
        static Result disable(I2cHandle) noexcept { return ok(); }
        static Result write(I2cHandle, util::u16,
                            std::span<const util::u8>) noexcept { return err(Status::busy); }
        static Result read(I2cHandle, util::u16,
                           std::span<util::u8>) noexcept { return err(Status::busy); }
        static Result write_read(I2cHandle, util::u16,
                                 std::span<const util::u8>,
                                 std::span<util::u8>) noexcept { return err(Status::busy); }
    };

    struct RawInput {
        static bool is_down(void*, input::Button) noexcept { return false; }
        static input::PointerRaw read_pointer(void*) noexcept { return input::PointerRaw{}; }
        static input::AxisRaw read_axis(void*) noexcept { return input::AxisRaw{}; }
        static std::optional<std::uint8_t> pop_encoder_ab(void*) noexcept { return std::nullopt; }

        static hal::RawInputDriver driver() noexcept {
            return hal::RawInputDriver{
                .ctx = nullptr,
                .is_down = &RawInput::is_down,
                .read_pointer = &RawInput::read_pointer,
                .read_axis = &RawInput::read_axis,
                .pop_encoder_ab = &RawInput::pop_encoder_ab
            };
        }
    };
}
