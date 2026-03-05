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
import util.core;

extern "C" SPI_HandleTypeDef hspi5;

namespace {
    static out::channel_sink* g_sink = nullptr;
    constexpr util::u32 kLogRetryMs = 20;

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

    void st7305_delay(std::uint32_t ms) noexcept {
        HAL_Delay(ms);
    }

    void st7305_reset(bool level) noexcept {
        HAL_GPIO_WritePin(kRstPort, kRstPin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    void st7305_transmit(std::uint8_t* data, std::uint16_t len, bool data_or_cmd) noexcept {
        HAL_GPIO_WritePin(kDcPort, kDcPin, data_or_cmd ? GPIO_PIN_SET : GPIO_PIN_RESET);
        HAL_GPIO_WritePin(kCsPort, kCsPin, GPIO_PIN_RESET);
        if (!data || len == 0) return;
        (void)HAL_SPI_Transmit(&hspi5, data, len, 1000);
        HAL_GPIO_WritePin(kCsPort, kCsPin, GPIO_PIN_SET);
    }

    void st7305_send_cmd(std::uint8_t cmd) noexcept {
        HAL_GPIO_WritePin(kDcPort, kDcPin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(kCsPort, kCsPin, GPIO_PIN_RESET);
        (void)HAL_SPI_Transmit(&hspi5, &cmd, 1, 1000);
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

export bool display_st7305_init() noexcept {
    hspi5.Init.NSS = SPI_NSS_SOFT;
    hspi5.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
    hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
    if (HAL_SPI_Init(&hspi5) != HAL_OK) {
        log<"display: spi init failed">();
        return false;
    }
    const auto st = panel().init();
    if (st != bsp::st7305::Status::ok) {
        log<"display: init failed">();
        return false;
    }
    st7305_send_cmd(0x39);
    HAL_Delay(20);
    st7305_send_cmd(0x38);
    HAL_Delay(20);
    (void)panel().invert(true);
    HAL_Delay(60);
    (void)panel().invert(false);
    log<"display: init ok">();
    return true;
}

export void display_st7305_selftest() noexcept {
    static std::array<std::uint8_t, bsp::st7305::kNativeSize> frame{};
    bsp::st7305::Panel::clear_native(frame, true);
    (void)panel().flush_native(frame);
    HAL_Delay(60);
    bsp::st7305::Panel::clear_native(frame, false);
    (void)panel().flush_native(frame);
    HAL_Delay(60);
    bsp::st7305::Panel::clear_native(frame, false);
    const int w = bsp::st7305::kWidth;
    const int h = bsp::st7305::kHeight;
    for (int x = 0; x < w; ++x) {
        bsp::st7305::Panel::set_native_pixel(frame, x, 0, true);
        bsp::st7305::Panel::set_native_pixel(frame, x, h - 1, true);
    }
    for (int y = 0; y < h; ++y) {
        bsp::st7305::Panel::set_native_pixel(frame, 0, y, true);
        bsp::st7305::Panel::set_native_pixel(frame, w - 1, y, true);
    }
    const int diag = (w < h) ? w : h;
    for (int i = 0; i < diag; ++i) {
        bsp::st7305::Panel::set_native_pixel(frame, i, i, true);
        bsp::st7305::Panel::set_native_pixel(frame, w - 1 - i, i, true);
    }
    (void)panel().flush_native(frame);
    log<"display: selftest pattern">();
}
