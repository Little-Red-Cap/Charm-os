#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "main.h"
#include "dma.h"
#include "gpio.h"
#include "usart.h"

void SystemClock_Config(void);

void charm_port_init_core(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
}

void* charm_port_init_console(void) {
    MX_USART1_UART_Init();
    return &huart1;
}

int charm_port_console_write(void* uart, const uint8_t* data, uint16_t len) {
    if (!uart || !data || len == 0) {
        return 0;
    }
    if (HAL_UART_Transmit((UART_HandleTypeDef*)uart, (uint8_t*)data, len, 100) != HAL_OK) {
        return 0;
    }
    return 1;
}

uint32_t charm_port_now_ms(void) {
    return (uint32_t)HAL_GetTick();
}

__attribute__((weak)) void app_usb_setup_sniff(const uint8_t setup[8]) {
    (void)setup;
}

__attribute__((weak)) void charm_audio_dma_irq_notify(void) {
}

__attribute__((weak)) void usbd_msc_debug_cbw(uint32_t sig, uint32_t data_len,
                                              uint8_t flags, uint8_t cb_len,
                                              uint8_t opcode) {
    (void)sig;
    (void)data_len;
    (void)flags;
    (void)cb_len;
    (void)opcode;
}

__attribute__((weak)) void usbd_msc_debug_cdb(const uint8_t *cb, uint8_t cb_len) {
    (void)cb;
    (void)cb_len;
}

__attribute__((weak)) void usbd_msc_debug_send(uint32_t kind, uint32_t len) {
    (void)kind;
    (void)len;
}

__attribute__((weak)) void usbd_msc_debug_bot_state(uint32_t kind, uint32_t state, uint8_t epnum) {
    (void)kind;
    (void)state;
    (void)epnum;
}
