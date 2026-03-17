#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "main.h"
#include "dma.h"
#include "gpio.h"
#include "usart.h"

void SystemClock_Config(void);

void charm_boot_init_core(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
}

void* charm_boot_init_uart(void) {
    MX_USART1_UART_Init();
    return &huart1;
}

int charm_boot_uart_write_bytes(void* uart, const uint8_t* data, uint16_t len) {
    if (!uart || !data || len == 0) {
        return 0;
    }
    if (HAL_UART_Transmit((UART_HandleTypeDef*)uart, (uint8_t*)data, len, 100) != HAL_OK) {
        return 0;
    }
    return 1;
}

uint32_t charm_boot_get_tick(void) {
    return (uint32_t)HAL_GetTick();
}
