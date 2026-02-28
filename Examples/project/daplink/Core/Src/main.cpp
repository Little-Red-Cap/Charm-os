#include "main.h"

#include "gpio.h"
#include "usart.h"

extern "C" void SystemClock_Config(void);
extern "C" void MPU_Config(void);

inline void set_led(bool state) { HAL_GPIO_WritePin(CONNECT_LED_GPIO_Port, CONNECT_LED_Pin, state ? GPIO_PIN_RESET : GPIO_PIN_SET); }
inline void toggle_led() { HAL_GPIO_TogglePin(CONNECT_LED_GPIO_Port, CONNECT_LED_Pin); }

int main()
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();

    {
        GPIO_InitTypeDef GPIO_InitStruct = {};
        // Put SWD-related pins into high-impedance analog mode to minimize interference.
        GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

        GPIO_InitStruct.Pin = T_CLK_Pin;
        HAL_GPIO_Init(T_CLK_GPIO_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = T_RST_Pin;
        HAL_GPIO_Init(T_RST_GPIO_Port, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = T_DIO_IN_Pin | T_CLKB4_Pin | T_DIO_OUT_Pin;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        // Keep the status LED as output for heartbeat.
        GPIO_InitStruct.Pin = CONNECT_LED_Pin;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }

    while (true) {
        toggle_led();
        HAL_Delay(1000);
    }
}
