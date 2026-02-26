module;
#include "main.h"
#include "usart.h"
#include <expected>

module out.port;
import out.core;


namespace out::port {

    result<std::size_t> console_sink::write(const bytes b) noexcept {
        if (HAL_OK != HAL_UART_Transmit(&huart1, reinterpret_cast<const uint8_t*>(b.data()), b.size(), HAL_MAX_DELAY))
            return std::unexpected(errc::io_error);
        return ok(b.size());
    }

    result<std::size_t> uart_sink::write(const bytes b) const noexcept {
        (void)handle;
        return console_sink{}.write(b);
        // return ok(HAL_UART_Transmit(static_cast<UART_HandleTypeDef*>(handle), reinterpret_cast<const uint8_t*>(b.data()), b.size(), HAL_MAX_DELAY));
    }

    tick_t now_ms() noexcept {
        auto ms = HAL_GetTick();
        return static_cast<tick_t>(ms);
    }

}
