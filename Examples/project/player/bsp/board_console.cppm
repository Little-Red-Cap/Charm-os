module;

#include <cstddef>
#include <cstdint>
#include <cstring>

export module player.stm32h7.board_console;

import charm.port;
import out.core;

export namespace player::stm32h7::board {
    struct UartIrqHandler {
        void* ctx{};
        void (*on_irq)(void*) noexcept{};
    };

    void set_console_sink(charm::port::ConsoleSink sink) noexcept;
    charm::port::ConsoleSink& console_sink() noexcept;
    void early_uart_print(const char* msg) noexcept;

    void set_uart_irq_handler(UartIrqHandler handler) noexcept;
}

namespace {
    charm::port::ConsoleSink g_console_sink{};
    player::stm32h7::board::UartIrqHandler g_uart_irq{};
}

namespace player::stm32h7::board {
    void set_console_sink(charm::port::ConsoleSink sink) noexcept {
        g_console_sink = sink;
    }

    charm::port::ConsoleSink& console_sink() noexcept {
        return g_console_sink;
    }

    void early_uart_print(const char* msg) noexcept {
        if (!msg) return;
        const std::size_t len = std::strlen(msg);
        if (len == 0) return;
        if (!g_console_sink.ctx) return;
        const out::bytes view{
            reinterpret_cast<const std::byte*>(msg),
            static_cast<std::size_t>(len)
        };
        (void)g_console_sink.write(view);
    }

    void set_uart_irq_handler(UartIrqHandler handler) noexcept {
        g_uart_irq = handler;
    }
}

extern "C" void USART1_IRQHandler(void) {
    if (g_uart_irq.on_irq) {
        g_uart_irq.on_irq(g_uart_irq.ctx);
    }
}
