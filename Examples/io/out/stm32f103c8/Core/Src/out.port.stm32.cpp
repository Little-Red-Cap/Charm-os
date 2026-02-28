module;
#include "main.h"
#include "usart.h"
#include <expected>

module out.port;
import out.core;

#ifndef OUT_UART_TIMEOUT_MS
#define OUT_UART_TIMEOUT_MS HAL_MAX_DELAY
#endif


namespace out::port {
    static console_sink* g_default_console = nullptr;

    namespace detail {
        inline void enter_critical() noexcept { __disable_irq(); }
        inline void exit_critical() noexcept { __enable_irq(); }
    }

    void set_default_console(console_sink* p) noexcept {
        detail::enter_critical();
        g_default_console = p;
        detail::exit_critical();
    }

    console_sink& default_console() noexcept {
        if (g_default_console) return *g_default_console;
        static console_sink inst{};
        return inst;
    }

    result<std::size_t> console_sink::write(const bytes b) noexcept {
        auto* p = reinterpret_cast<const uint8_t*>(b.data());
        std::size_t left = b.size();
        while (left != 0) {
            const uint16_t chunk = (left > 0xFFFFu) ? 0xFFFFu : static_cast<uint16_t>(left);
            if (HAL_OK != HAL_UART_Transmit(&huart1, const_cast<uint8_t*>(p), chunk, OUT_UART_TIMEOUT_MS))
                return std::unexpected(errc::io_error);
            p += chunk;
            left -= chunk;
        }
        return ok(b.size());
    }

    result<std::size_t> console_sink::flush() noexcept {
        return ok(0u);
    }

    result<std::size_t> uart_sink::write(const bytes b) const noexcept {
        auto* h = static_cast<UART_HandleTypeDef*>(handle);
        if (!h) return std::unexpected(errc::io_error);

        auto* p = reinterpret_cast<const uint8_t*>(b.data());
        std::size_t left = b.size();
        while (left != 0) {
            const uint16_t chunk = (left > 0xFFFFu) ? 0xFFFFu : static_cast<uint16_t>(left);
            if (HAL_OK != HAL_UART_Transmit(h, const_cast<uint8_t*>(p), chunk, OUT_UART_TIMEOUT_MS))
                return std::unexpected(errc::io_error);
            p += chunk;
            left -= chunk;
        }
        return ok(b.size());
    }

    tick_t now_ms() noexcept {
        auto ms = HAL_GetTick();
        return static_cast<tick_t>(ms);
    }

}
