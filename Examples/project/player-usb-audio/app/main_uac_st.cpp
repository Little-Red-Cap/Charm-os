#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {
#include "main.h"
#include "dma.h"
#include "gpio.h"
#include "i2s.h"
#include "usb.h"
#include "usb_device.h"
#include "usbd_audio_if.h"

void SystemClock_Config(void);

volatile std::uint32_t g_player_usb_audio_i2s_zero_fill_events = 0;
volatile std::uint32_t g_player_usb_audio_i2s_zero_fill_bytes = 0;
volatile std::uint32_t g_player_usb_audio_i2s_last_shortfall = 0;
}

namespace {
    constexpr std::size_t kDmaHalfWords = 256;
    constexpr std::size_t kDmaWordCount = kDmaHalfWords * 2;

    std::array<std::uint16_t, kDmaWordCount> g_i2s_dma{};

    void fill_dma_half(std::size_t word_offset, std::size_t word_count) noexcept {
        auto* dst = reinterpret_cast<std::uint8_t*>(g_i2s_dma.data() + word_offset);
        const auto want_bytes = static_cast<std::uint32_t>(word_count * sizeof(std::uint16_t));
        const auto got_bytes = usb_audio_ring_read(dst, want_bytes);
        if (got_bytes < want_bytes) {
            const auto shortfall = want_bytes - got_bytes;
            g_player_usb_audio_i2s_zero_fill_events =
                g_player_usb_audio_i2s_zero_fill_events + 1U;
            g_player_usb_audio_i2s_zero_fill_bytes += shortfall;
            g_player_usb_audio_i2s_last_shortfall = shortfall;
            std::memset(dst + got_bytes, 0, want_bytes - got_bytes);
        }
    }

    [[noreturn]] void fail_fast() {
        Error_Handler();
        while (true) {
        }
    }
}

extern "C" void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef* hi2s) {
    if (!hi2s || hi2s->Instance != SPI2) {
        return;
    }
    fill_dma_half(0, kDmaHalfWords);
    HalfTransfer_CallBack_FS();
}

extern "C" void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef* hi2s) {
    if (!hi2s || hi2s->Instance != SPI2) {
        return;
    }
    fill_dma_half(kDmaHalfWords, kDmaHalfWords);
    TransferComplete_CallBack_FS();
}

extern "C" void HAL_I2S_ErrorCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s && hi2s->Instance == SPI2) {
        Error_Handler();
    }
}

int main() {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2S2_Init();
    MX_USB_PCD_Init();

    usb_audio_rx_reset();
    fill_dma_half(0, kDmaHalfWords);
    fill_dma_half(kDmaHalfWords, kDmaHalfWords);

    if (HAL_I2S_Transmit_DMA(&hi2s2, g_i2s_dma.data(), static_cast<std::uint16_t>(g_i2s_dma.size())) != HAL_OK) {
        fail_fast();
    }

    MX_USB_DEVICE_Init();

    while (true) {
        __WFI();
    }
}
