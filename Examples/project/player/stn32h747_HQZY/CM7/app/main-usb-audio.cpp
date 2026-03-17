#include <cstddef>
#include <cstdio>
#include <cstring>

#include "main.h"
#include "dma.h"
#include "gpio.h"
#include "i2s.h"
#include "usart.h"
#include "usb_device.h"
#include "stm32h7xx_hal_rcc_ex.h"
#include "usbd_audio.h"
#include "usbd_audio_if.h"

extern "C" {
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_DMA_Init(void);
void MX_I2S1_Init(void);
void MX_USART1_UART_Init(void);
void Error_Handler(void);
extern UART_HandleTypeDef huart1;
extern I2S_HandleTypeDef hi2s1;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

namespace {
void uart_write(const char* msg);
}

extern "C" void app_usb_setup_sniff(const uint8_t setup[8]) {
    const uint8_t bm = setup[0];
    const uint8_t b = setup[1];
    const uint16_t wValue = static_cast<uint16_t>(setup[2] | (setup[3] << 8));
    const uint16_t wIndex = static_cast<uint16_t>(setup[4] | (setup[5] << 8));
    const uint16_t wLen = static_cast<uint16_t>(setup[6] | (setup[7] << 8));
    char buf[120];
    const int n = snprintf(
        buf,
        sizeof(buf),
        "usb: setup bm=0x%02X b=0x%02X wValue=0x%04X wIndex=0x%04X wLen=0x%04X\n",
        static_cast<unsigned>(bm),
        static_cast<unsigned>(b),
        static_cast<unsigned>(wValue),
        static_cast<unsigned>(wIndex),
        static_cast<unsigned>(wLen));
    if (n > 0) {
        uart_write(buf);
    }
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

constexpr uint32_t kAudioBufBytes = 32768;
alignas(4) uint8_t g_audio_buf[kAudioBufBytes];
volatile bool g_i2s_started = false;
volatile bool g_i2s_active = false;
constexpr uint32_t kRingLowWater = 24576;
constexpr uint32_t kRingHighWater = 98304;
alignas(4) uint8_t g_last_frame[kAudioBufBytes / 2];
alignas(4) uint8_t g_discard_buf[2048];

void fill_audio_half(uint32_t half_index) {
    if (!g_i2s_active) {
        (void)memset(g_audio_buf + (half_index * (kAudioBufBytes / 2)), 0, kAudioBufBytes / 2);
        return;
    }
    uint8_t* dst = g_audio_buf + (half_index * (kAudioBufBytes / 2));
    const uint32_t want = kAudioBufBytes / 2;
    uint32_t available = usb_audio_ring_available();
    if (available > kRingHighWater) {
        uint32_t drop = available - kRingHighWater;
        while (drop > 0) {
            const uint32_t chunk = (drop > sizeof(g_discard_buf)) ? sizeof(g_discard_buf) : drop;
            (void)usb_audio_ring_read(g_discard_buf, chunk);
            drop -= chunk;
        }
        available = usb_audio_ring_available();
    }
    if (available < kRingLowWater) {
        (void)memcpy(dst, g_last_frame, want);
        return;
    }
    const uint32_t got = usb_audio_ring_read(dst, want);
    if (got < want) {
        (void)memset(dst + got, 0, want - got);
    }
    (void)memcpy(g_last_frame, dst, want);
}
}

extern "C" void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s != &hi2s1) return;
    fill_audio_half(0);
}

extern "C" void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s != &hi2s1) return;
    fill_audio_half(1);
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2S1_Init();
    MX_USART1_UART_Init();
    uart_write("boot: uart ok\n");
#if defined(RCC_PERIPHCLK_SPI123)
    const uint32_t i2s_clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI123);
#elif defined(RCC_PERIPHCLK_SPI1)
    const uint32_t i2s_clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SPI1);
#else
    const uint32_t i2s_clk = 0;
#endif
    {
        char buf[120];
        const uint32_t target_mclk = 48000U * 256U;
        const int n = snprintf(
            buf,
            sizeof(buf),
            "i2s: kern_clk=%luHz target_mclk=%luHz freq=%lu\n",
            static_cast<unsigned long>(i2s_clk),
            static_cast<unsigned long>(target_mclk),
            static_cast<unsigned long>(hi2s1.Init.AudioFreq));
        if (n > 0) {
            uart_write(buf);
        }
    }

    MX_USB_DEVICE_Init();
    uart_write("usb: device init ok\n");
    if (HAL_PCD_Start(&hpcd_USB_OTG_FS) != HAL_OK) {
        uart_write("usb: start failed\n");
        Error_Handler();
    }
    uart_write("usb: pcd start ok\n");

    while (1) {
        static uint32_t last_log = 0;
        const uint32_t now = HAL_GetTick();
        if ((now - last_log) >= 3000) {
            const uint32_t bytes = usb_audio_rx_bytes();
            const uint32_t pkts = usb_audio_rx_pkts();
            const uint32_t last_size = usb_audio_rx_last_size();
            const uint32_t overflows = usb_audio_rx_overflows();
            const uint32_t freq = usb_audio_rx_freq();
            const uint32_t cmd = usb_audio_rx_cmd();
            const uint32_t init_calls = usb_audio_rx_init_calls();
            const uint32_t cmd_calls = usb_audio_rx_cmd_calls();
            const uint32_t set_if_calls = usb_audio_set_interface_calls();
            const uint32_t last_alt = usb_audio_last_alt_setting();
            const uint32_t ring_avail = usb_audio_ring_available();
            const uint32_t ring_ovf = usb_audio_ring_overflows();
            char buf[160];
            const int n = snprintf(
                buf,
                sizeof(buf),
                "usb: audio bytes=%lu pkts=%lu last=%lu freq=%lu cmd=%lu init=%lu cmd_calls=%lu set_if=%lu alt=%lu ovf=%lu ring=%lu ring_ovf=%lu\n",
                static_cast<unsigned long>(bytes),
                static_cast<unsigned long>(pkts),
                static_cast<unsigned long>(last_size),
                static_cast<unsigned long>(freq),
                static_cast<unsigned long>(cmd),
                static_cast<unsigned long>(init_calls),
                static_cast<unsigned long>(cmd_calls),
                static_cast<unsigned long>(set_if_calls),
                static_cast<unsigned long>(last_alt),
                static_cast<unsigned long>(overflows),
                static_cast<unsigned long>(ring_avail),
                static_cast<unsigned long>(ring_ovf));
            if (n > 0) {
                uart_write(buf);
            }
            last_log = now;
        }
        const uint32_t ring_avail = usb_audio_ring_available();
        const bool streaming = (usb_audio_last_alt_setting() == 1);
        if (!streaming && g_i2s_started) {
            g_i2s_active = false;
            (void)HAL_I2S_DMAStop(&hi2s1);
            g_i2s_started = false;
            usb_audio_rx_reset();
            (void)memset(g_last_frame, 0, sizeof(g_last_frame));
        }
        if (!g_i2s_started && ring_avail >= kAudioBufBytes) {
            fill_audio_half(0);
            fill_audio_half(1);
            if (HAL_I2S_Transmit_DMA(&hi2s1,
                    reinterpret_cast<uint16_t*>(g_audio_buf),
                    kAudioBufBytes / 2) == HAL_OK) {
                g_i2s_started = true;
                g_i2s_active = true;
            }
        }
        HAL_Delay(10);
    }
}
