#include "drivers.h"

extern "C" void MX_DMA_Init(void);
extern "C" void MX_GPIO_Init(void);
extern "C" void MX_I2C1_Init(void);
extern "C" void MX_I2C4_Init(void);
extern "C" void MX_QUADSPI_Init(void);
extern "C" void MX_SPI4_Init(void);
extern "C" void MX_SPI5_Init(void);
extern "C" void MX_TIM5_Init(void);
extern "C" void MX_TIM8_Init(void);
extern "C" void MX_USART1_UART_Init(void);
extern "C" void MX_USART2_UART_Init(void);

namespace h747::board {

void init_default_peripherals() {
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
#if !defined(H747_LAB_FOUNDATION_PLATFORM)
    MX_USART2_UART_Init();
    MX_I2C1_Init();
    MX_I2C4_Init();
    MX_QUADSPI_Init();
    MX_SPI4_Init();
    MX_SPI5_Init();
    MX_TIM5_Init();
    MX_TIM8_Init();
#endif
}

} // namespace h747::board
