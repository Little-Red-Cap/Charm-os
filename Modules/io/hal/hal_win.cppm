module;

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <thread>

export module hal_win;

import hal_core;
import hal_time;
import hal_gpio;
import hal_uart;
import hal_timer;
import util.core;

export namespace hal::win {
    struct Time {
        using Tick = tick_t;
        static Tick now() noexcept {
            using namespace std::chrono;
            auto ms = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
            return static_cast<Tick>(ms);
        }
    };

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
        static Result write(UartHandle, std::span<const util::u8> tx) noexcept {
            if (!tx.empty()) std::fwrite(tx.data(), 1, tx.size(), stdout);
            return ok();
        }
        static Result read(UartHandle, std::span<util::u8>) noexcept {
            return err(Status::unsupported);
        }
    };

    struct Timer {
        static Result init(TimerHandle, TimerConfig) noexcept { return ok(); }
        static Result start(TimerHandle) noexcept { return ok(); }
        static Result stop(TimerHandle) noexcept { return ok(); }
        static Result set_callback(TimerHandle, TimerCallback) noexcept { return ok(); }
    };
}
