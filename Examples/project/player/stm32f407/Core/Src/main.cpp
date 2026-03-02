#include "main.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

#include "gpio.h"
#include "i2c.h"
#include "i2s.h"
#include "dma.h"
#include "usart.h"

import player.stm32.fs_demo;
import player.stm32.audio_mp3_demo;
import lcd_driver;
import out.api;

extern "C" void SystemClock_Config(void);

out::port::console_sink uart_sink;

namespace {
    constexpr bool kRunFsDemo = false;
    constexpr bool kBootLog = true;
    constexpr bool kI2sWaveLoop = false;
    enum class SelfTestMode : std::uint8_t { none, i2s_dma, sdio_dma, both };
    constexpr auto kSelfTest = SelfTestMode::none;
    std::array<std::uint16_t, 512> g_i2s_dma_buf{};

    void i2s_wave_dma_start() noexcept {
        constexpr std::size_t half_period = 24; // ~1kHz at 48kHz, ~918Hz at 44.1kHz
        std::size_t count = 0;
        std::uint16_t sample = 0x4000;
        for (std::size_t i = 0; i + 1 < g_i2s_dma_buf.size(); i += 2) {
            if ((count++ % half_period) == 0) {
                sample = (sample == 0x4000) ? 0xC000 : 0x4000;
            }
            // 16-bit stereo: L/R interleaved.
            g_i2s_dma_buf[i + 0] = sample;
            g_i2s_dma_buf[i + 1] = sample;
        }
        if constexpr (kBootLog) {
            out::println<"i2s dma test: begin">(uart_sink);
        }
        if (HAL_I2S_Transmit_DMA(&hi2s2, g_i2s_dma_buf.data(),
                static_cast<uint16_t>(g_i2s_dma_buf.size())) != HAL_OK) {
            if constexpr (kBootLog) {
                out::println<"i2s dma test: start failed">(uart_sink);
            }
        } else {
            if constexpr (kBootLog) {
                out::println<"i2s dma test: started">(uart_sink);
            }
        }
    }
}

extern "C" void charm_audio_i2s_debug_toggle() {
    HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
}

int main()
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_DMA_Init();
    MX_I2S2_Init();
    MX_USART1_UART_Init();
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
        GPIO_InitTypeDef gpio_init = {};
        gpio_init.Pin = GPIO_PIN_12;
        gpio_init.Mode = GPIO_MODE_INPUT;
        gpio_init.Pull = GPIO_PULLUP;
        gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOA, &gpio_init);
    }
    if constexpr (kBootLog) {
        out::println<"boot: init ok">(uart_sink);
    }
    if constexpr (kI2sWaveLoop) {
        i2s_wave_dma_start();
        while (true) {
            HAL_Delay(1000);
        }
    }
    // LCD_Init();
    if constexpr (kBootLog) {
        out::println<"boot: lcd init ok">(uart_sink);
    }
    if constexpr (kSelfTest != SelfTestMode::none) {
        if constexpr (kSelfTest == SelfTestMode::i2s_dma || kSelfTest == SelfTestMode::both) {
            i2s_dma_selftest();
        }
        if constexpr (kSelfTest == SelfTestMode::sdio_dma || kSelfTest == SelfTestMode::both) {
            sdio_dma_selftest();
        }
        while (true) {
            HAL_Delay(1000);
        }
    }
    if (!fs_boot_init()) {
        while (true) {
            HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
            HAL_Delay(200);
        }
    }
    if constexpr (kRunFsDemo) {
        fs_demo_run();
        if constexpr (kBootLog) {
            out::println<"boot: fs demo done">(uart_sink);
        }
    }
    if constexpr (kBootLog) {
        out::println<"boot: wait key">(uart_sink);
    }
    while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12) != GPIO_PIN_RESET) {
        HAL_Delay(10);
    }
    HAL_Delay(20);
    audio_mp3_demo_run();
    if constexpr (kBootLog) {
        out::println<"boot: mp3 demo done">(uart_sink);
    }

    while (true) {
        HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
        HAL_Delay(1000);
    }
}
