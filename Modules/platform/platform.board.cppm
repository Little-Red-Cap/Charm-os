module;

export module platform.board;

import hal_uart;

export namespace platform::board {
    struct UartDesc {
        hal::UartIoHandle handle{};
        hal::UartConfig config{};
        const char* io_cap{"io.uart1"};
        const char* hal_cap{"hal.uart1"};
    };

    struct BoardCaps {
        UartDesc uart1{};
    };
}
