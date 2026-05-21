#include "main.h"
#include "usart.h"

#include <cstdint>

extern "C" void Error_Handler(void) {
    static constexpr char kMsg[] = "error: Error_Handler\r\n";

    while (true) {
        if (huart1.Instance != nullptr) {
            (void)HAL_UART_Transmit(&huart1,
                                    reinterpret_cast<std::uint8_t*>(const_cast<char*>(kMsg)),
                                    sizeof(kMsg) - 1U,
                                    100U);
        }
        HAL_Delay(100U);
    }
}
