module;

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <optional>
#include <span>
#include <thread>

export module hal_win;

import hal_core;
import hal_gpio;
import hal_uart;
import hal_timer;
import hal_input;
import input.raw;
import util.core;

export namespace hal::win {
    struct Delay {
        static void delay_ms(tick_t ms) noexcept {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
    };

    struct Sleep {
        static void sleep_ms(tick_t ms) noexcept {
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        }
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
        static Result try_write(UartHandle, util::u8 byte) noexcept {
            std::fwrite(&byte, 1, 1, stdout);
            return ok();
        }
        static Result try_read(UartHandle, util::u8&) noexcept {
            return err(Status::busy);
        }
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
