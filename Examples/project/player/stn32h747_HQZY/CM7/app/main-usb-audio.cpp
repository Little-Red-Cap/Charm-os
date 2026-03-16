#include <cstddef>
#include <cstring>

#include "main.h"
#include "dma.h"
#include "gpio.h"
#include "usart.h"
#include "usb_device.h"

extern "C" {
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_DMA_Init(void);
void MX_USART1_UART_Init(void);
void Error_Handler(void);
extern UART_HandleTypeDef huart1;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

extern "C" void app_usb_setup_sniff(const uint8_t[8]) {
}

extern "C" void charm_audio_dma_irq_notify(void) {
}

namespace {
void uart_write(const char* msg) {
    if (!msg) return;
    const std::size_t len = std::strlen(msg);
    if (len == 0) return;
    (void)HAL_UART_Transmit(&huart1,
        reinterpret_cast<uint8_t*>(const_cast<char*>(msg)),
        static_cast<uint16_t>(len),
        100);
}
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    uart_write("boot: uart ok\n");

    MX_USB_DEVICE_Init();
    uart_write("usb: device init ok\n");
    if (HAL_PCD_Start(&hpcd_USB_OTG_FS) != HAL_OK) {
        uart_write("usb: start failed\n");
        Error_Handler();
    }
    uart_write("usb: pcd start ok\n");

    while (1) {
        HAL_Delay(1000);
    }
}
