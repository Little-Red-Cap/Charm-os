#include "main.h"

#include <cstring>

#include "gpio.h"
#include "i2c.h"
#include "usart.h"

import player.stm32.fs_demo;

extern "C" void SystemClock_Config(void);

int main()
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();

    fs_demo_run();

    while (1) {
        HAL_UART_Transmit(&huart1, (uint8_t *)"Hello World!\r\n", strlen("Hello World!\r\n"), 100);
        HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
        HAL_GPIO_TogglePin(LCD_BL_GPIO_Port, LCD_BL_Pin);
        HAL_Delay(1000);
    }
}
