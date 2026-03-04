module;

export module platform.board;

import hal_uart;
import util.core;

export namespace platform::board {
    struct ClockDesc {
        void* ctx{nullptr};
        util::u64 (*now_ms)(void* ctx) noexcept { nullptr };
        util::u64 (*now_us)(void* ctx) noexcept { nullptr };
    };

    struct UartDesc {
        hal::UartIoHandle handle{};
        hal::UartConfig config{};
        const char* io_cap{"io.uart1"};
        const char* hal_cap{"hal.uart1"};
    };

    struct BoardCaps {
        UartDesc uart1{};
        ClockDesc clock{};
    };
}
