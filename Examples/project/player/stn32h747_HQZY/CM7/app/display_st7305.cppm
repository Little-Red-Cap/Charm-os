module;

#include <array>
#include <cstdint>
#include <span>

#include "stm32h7xx_hal.h"
#include "spi.h"

export module player.stm32h7.display_st7305;

import bsp.st7305;
import out.api;
import out.channel;
import player.stm32h7.audio_mp3_demo;
import util.core;

extern "C" SPI_HandleTypeDef hspi5;

namespace {
    static out::channel_sink* g_sink = nullptr;
    static void (*g_yield)() noexcept = nullptr;
    constexpr util::u32 kLogRetryMs = 20;
    static volatile bool g_spi5_dma_done = false;
    static volatile bool g_spi5_dma_error = false;

    template <out::fixed_string Fmt, typename... Args>
    inline void log(Args&&... args) noexcept {
        if (!g_sink) return;
        const util::u32 start = HAL_GetTick();
        while (true) {
            auto r = out::try_println<Fmt>(*g_sink, std::forward<Args>(args)...);
            if (r) break;
            if (r.error() != out::errc::would_block) break;
            if ((HAL_GetTick() - start) > kLogRetryMs) break;
            HAL_Delay(1);
        }
        const util::u32 flush_start = HAL_GetTick();
        while (true) {
            auto r = g_sink->flush();
            if (r) break;
            if (r.error() != out::errc::would_block) break;
            if ((HAL_GetTick() - flush_start) > kLogRetryMs) break;
            HAL_Delay(1);
        }
    }

    GPIO_TypeDef* const kRstPort = GPIOJ;
    constexpr std::uint16_t kRstPin = GPIO_PIN_5;
    GPIO_TypeDef* const kDcPort = GPIOJ;
    constexpr std::uint16_t kDcPin = GPIO_PIN_6;
    GPIO_TypeDef* const kCsPort = GPIOK;
    constexpr std::uint16_t kCsPin = GPIO_PIN_1;
    constexpr auto kPanelGeom = bsp::st7305::kDefaultGeometry;
    constexpr std::size_t kPanelNativeSize = bsp::st7305::native_size_for(kPanelGeom);
    constexpr int kPanelWidth = kPanelGeom.width;
    constexpr int kPanelHeight = kPanelGeom.height;

    void st7305_delay(std::uint32_t ms) noexcept {
        HAL_Delay(ms);
    }

    void st7305_reset(bool level) noexcept {
        HAL_GPIO_WritePin(kRstPort, kRstPin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    void st7305_clean_dcache(const void* addr, std::size_t size) noexcept {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
        if (!addr || size == 0) return;
        if ((SCB->CCR & SCB_CCR_DC_Msk) == 0U) return;
        std::uintptr_t start = reinterpret_cast<std::uintptr_t>(addr);
        std::uintptr_t end = start + size;
        start &= ~static_cast<std::uintptr_t>(31);
        end = (end + 31u) & ~static_cast<std::uintptr_t>(31);
        SCB_CleanDCache_by_Addr(reinterpret_cast<uint32_t*>(start),
            static_cast<int32_t>(end - start));
#else
        (void)addr;
        (void)size;
#endif
    }

    void st7305_transmit(std::uint8_t* data, std::uint16_t len, bool data_or_cmd) noexcept {
        constexpr std::uint16_t kChunk = 4096;
        HAL_GPIO_WritePin(kDcPort, kDcPin, data_or_cmd ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(kCsPort, kCsPin, GPIO_PIN_RESET);
        if (!data || len == 0) {
            HAL_GPIO_WritePin(kCsPort, kCsPin, GPIO_PIN_SET);
            return;
        }
        audio_mp3_set_log_suppressed(true);
        std::uint16_t offset = 0;
        while (offset < len) {
            const std::uint16_t remaining = static_cast<std::uint16_t>(len - offset);
            const std::uint16_t chunk = (remaining > kChunk) ? kChunk : remaining;
            st7305_clean_dcache(data + offset, chunk);
            g_spi5_dma_done = false;
            g_spi5_dma_error = false;
            if (HAL_SPI_Transmit_DMA(&hspi5, data + offset, chunk) != HAL_OK) {
                (void)HAL_SPI_Transmit(&hspi5, data + offset, chunk, 1000);
            } else {
                const util::u32 start = HAL_GetTick();
                while (!g_spi5_dma_done && !g_spi5_dma_error) {
                    if ((HAL_GetTick() - start) > 1000u) {
                        (void)HAL_SPI_Abort(&hspi5);
                        break;
                    }
                    if (g_yield) g_yield();
                }
            }
            offset = static_cast<std::uint16_t>(offset + chunk);
            if (g_yield) g_yield();
        }
        audio_mp3_set_log_suppressed(false);
        HAL_GPIO_WritePin(kCsPort, kCsPin, GPIO_PIN_SET);
    }

    bsp::st7305::Panel& panel() noexcept {
        static bsp::st7305::Io io{
            .transmit = &st7305_transmit,
            .reset = &st7305_reset,
            .delay_ms = &st7305_delay,
            .reset_active_high = false
        };
        static bsp::st7305::Panel panel{io};
        return panel;
    }
}

export void display_set_console_sink(out::channel_sink& sink) noexcept {
    g_sink = &sink;
}

extern "C" void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &hspi5) {
        g_spi5_dma_done = true;
    }
}

extern "C" void HAL_SPI_ErrorCallback(SPI_HandleTypeDef* hspi) {
    if (hspi == &hspi5) {
        g_spi5_dma_error = true;
        g_spi5_dma_done = true;
    }
}

export void display_st7305_set_yield(void (*fn)() noexcept) noexcept {
    g_yield = fn;
}

export bool display_st7305_init() noexcept {
    hspi5.Init.NSS = SPI_NSS_SOFT;
    hspi5.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    if (HAL_SPI_Init(&hspi5) != HAL_OK) {
        log<"display: spi init failed">();
        return false;
    }
    bsp::st7305::InitOptions options{};
    options.high_power = true;
    options.mirror_y = false;
    options.mirror_x = true;
    const auto st = panel().init(options);
    if (st != bsp::st7305::Status::ok) {
        log<"display: init failed">();
        return false;
    }
    (void)panel().invert(true);
    HAL_Delay(60);
    (void)panel().invert(false);
    log<"display: init ok">();
    return true;
}

export void display_st7305_selftest() noexcept {
    static std::array<std::uint8_t, kPanelNativeSize> frame{};
    panel().clear_native(frame, false);
    panel().set_native_pixel(frame, 0, 0, true);
    panel().set_native_pixel(frame, 0, 1, true);
    panel().set_native_pixel(frame, 1, 0, true);
    (void)panel().flush_native(frame);
    log<"display: selftest origin mark">();
}

export bsp::st7305::Panel& display_st7305_panel() noexcept {
    return panel();
}
