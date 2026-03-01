#include "main.h"

#include "gpio.h"
#include "i2s.h"
#include "dma.h"
#include "usart.h"

/*
SD/TF Card
    SDIO_D0 -> PB14 -> MISO
    SDIO_D1 -> PB15 -> NC
    SDIO_D2 -> PG11 -> NC
    SDIO_D3 -> PB4  -> CS
    SDIO_CMD-> PA0  -> MOSI
    SDIO_CLK-> PD6  -> CLK
Audio
    I2S1_SDO-> PB5
    I2S1_WS -> PG10
    I2S1_CK -> PA5
    I2S1_MCK-> PC4
LED
    StateLED-> PI15

F4 -> H7 PinMap
    D0: PC8 → H7 PB14
    D1: PC9 → H7 PB15
    D2: PC10 → H7 PG11
    D3: PC11 → H7 PB4
    CK: PC12 → H7 PD6
    CMD: PD2 → H7 PA0
 */

extern "C" void SystemClock_Config(void);
extern "C" void MPU_Config(void);
extern bool player_app_boot_and_run() noexcept;

namespace {
    constexpr bool H7BootLog = false;
    constexpr bool H7RunPlayer = true;
}

inline void set_led(bool state) { HAL_GPIO_WritePin(GPIOI, GPIO_PIN_15, state ? GPIO_PIN_RESET : GPIO_PIN_SET); }
inline void toggle_led() { HAL_GPIO_TogglePin(GPIOI, GPIO_PIN_15); }

extern "C" void charm_audio_i2s_debug_toggle() {
    toggle_led();
}


void GPIO_Init()
{
    GPIO_InitTypeDef GPIO_InitStruct = {};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
}

int main()
{
    MPU_Config();
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2S1_Init();
    MX_USART1_UART_Init();

    GPIO_Init();

    if constexpr (H7BootLog) {
        /* reserved for early boot diagnostics */
    }

    if constexpr (H7RunPlayer) {
        if (!player_app_boot_and_run()) {
            while (true) {
                HAL_Delay(200);
            }
        }
    }

    while (true) {
        toggle_led();
        HAL_Delay(1000);
    }
}
