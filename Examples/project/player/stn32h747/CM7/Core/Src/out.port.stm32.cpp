module;

#include <expected>
#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_uart.h"


module out.port;
import out.core;

extern "C" UART_HandleTypeDef huart1;

namespace out::port {
    namespace {
        console_sink* g_default_console = nullptr;
        console_sink g_console_instance{};
    }

    result<std::size_t> console_sink::write(const bytes b) noexcept {
        if (HAL_OK != HAL_UART_Transmit(&huart1, reinterpret_cast<const uint8_t*>(b.data()), b.size(), HAL_MAX_DELAY))
            return std::unexpected(errc::io_error);
        return ok(b.size());
    }

    result<std::size_t> console_sink::flush() noexcept {
        return ok<std::size_t>(0u);
    }

    console_sink& default_console() noexcept {
        return g_default_console ? *g_default_console : g_console_instance;
    }

    void set_default_console(console_sink* p) noexcept {
        g_default_console = p;
    }

    result<std::size_t> uart_sink::write(const bytes b) const noexcept {
        (void)handle;
        return console_sink{}.write(b);
    }

    tick_t now_ms() noexcept {
        const auto ms = HAL_GetTick();
        return static_cast<tick_t>(ms);
    }

}
