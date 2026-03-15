#include "main.h"
#include "dma.h"
#include "gpio.h"
#include "usart.h"

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();

    while (1) {
        HAL_Delay(1000);
    }
}
