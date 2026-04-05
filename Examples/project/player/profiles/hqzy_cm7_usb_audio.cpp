#include <cstddef>
#include <cstdio>
#include <cstring>

#include "i2s.h"
#include "usb_device.h"
#include "stm32h7xx_hal_rcc_ex.h"
#include "usbd_audio.h"
#include "usbd_audio_if.h"

import out.api;
import charm.port;
import charm.system.clock;
import charm.system.time;

extern "C" {
void MX_I2S1_Init(void);
void Error_Handler(void);
extern I2S_HandleTypeDef hi2s1;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
int charm_port_console_write(void* uart, const uint8_t* data, uint16_t len);
}

namespace {
void uart_write(const char* msg);
void* g_console_ctx = nullptr;
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
    (void)charm_port_console_write(
        g_console_ctx,
        reinterpret_cast<const uint8_t*>(msg),
        static_cast<uint16_t>(len));
}

constexpr uint32_t kAudioBufBytes = 4096;
alignas(4) uint8_t g_audio_buf[kAudioBufBytes];
volatile bool g_i2s_started = false;
volatile bool g_i2s_active = false;
volatile bool g_audio_stream_primed = false;
constexpr uint32_t kFrameBytes = 4;
constexpr uint32_t kUsbSyncChunkBytes = AUDIO_TOTAL_BUF_SIZE / 2U;
constexpr uint32_t kRingStartThreshold = 24576;
constexpr uint32_t kRingTarget = 24576;
constexpr uint32_t kRingBand = 8192;
constexpr uint32_t kRingEmergencyHighWater = 49152;
alignas(4) uint8_t g_discard_buf[2048];
alignas(4) uint8_t g_last_frame[kFrameBytes]{};
alignas(4) uint8_t g_slip_in_buf[(kAudioBufBytes / 2) + kFrameBytes]{};
volatile uint32_t g_audio_ring_min = 0xFFFFFFFFu;
volatile uint32_t g_audio_ring_max = 0;
volatile uint32_t g_audio_slip_drop_bytes = 0;
volatile uint32_t g_audio_slip_dup_bytes = 0;
volatile uint32_t g_audio_sync_credit_bytes = 0;
volatile uint32_t g_audio_slip_cursor = 0;

void audio_health_reset() {
    g_audio_ring_min = 0xFFFFFFFFu;
    g_audio_ring_max = 0;
    g_audio_slip_drop_bytes = 0;
    g_audio_slip_dup_bytes = 0;
    g_audio_sync_credit_bytes = 0;
    g_audio_slip_cursor = 0;
    g_audio_stream_primed = false;
    (void)memset(g_last_frame, 0, sizeof(g_last_frame));
}

void audio_health_note(uint32_t available) {
    if (available < g_audio_ring_min) {
        g_audio_ring_min = available;
    }
    if (available > g_audio_ring_max) {
        g_audio_ring_max = available;
    }
}

void mix_frame(const uint8_t* a, const uint8_t* b, uint8_t* out) {
    int16_t al = 0;
    int16_t ar = 0;
    int16_t bl = 0;
    int16_t br = 0;
    (void)memcpy(&al, a + 0, sizeof(al));
    (void)memcpy(&ar, a + 2, sizeof(ar));
    (void)memcpy(&bl, b + 0, sizeof(bl));
    (void)memcpy(&br, b + 2, sizeof(br));
    const int16_t ol = static_cast<int16_t>((static_cast<int32_t>(al) + static_cast<int32_t>(bl)) / 2);
    const int16_t orr = static_cast<int16_t>((static_cast<int32_t>(ar) + static_cast<int32_t>(br)) / 2);
    (void)memcpy(out + 0, &ol, sizeof(ol));
    (void)memcpy(out + 2, &orr, sizeof(orr));
}

void audio_note_consumed(uint32_t bytes) {
    g_audio_sync_credit_bytes += bytes;
    while (g_audio_sync_credit_bytes >= kUsbSyncChunkBytes) {
        TransferComplete_CallBack_FS();
        g_audio_sync_credit_bytes -= kUsbSyncChunkBytes;
    }
}

void fill_audio_half(uint32_t half_index) {
    if (!g_i2s_active || !g_audio_stream_primed) {
        (void)memset(g_audio_buf + (half_index * (kAudioBufBytes / 2)), 0, kAudioBufBytes / 2);
        return;
    }
    uint8_t* dst = g_audio_buf + (half_index * (kAudioBufBytes / 2));
    const uint32_t want = kAudioBufBytes / 2;
    uint32_t available = usb_audio_ring_available();
    audio_health_note(available);
    if (available > kRingEmergencyHighWater) {
        uint32_t drop = available - kRingTarget;
        while (drop > 0) {
            const uint32_t chunk = (drop > sizeof(g_discard_buf)) ? sizeof(g_discard_buf) : drop;
            (void)usb_audio_ring_read(g_discard_buf, chunk);
            drop -= chunk;
        }
        available = usb_audio_ring_available();
        audio_health_note(available);
    }

    uint32_t read_want = want;
    uint32_t drop_after = 0;
    uint32_t dup_after = 0;
    if (available > (kRingTarget + kRingBand)) {
        drop_after = kFrameBytes;
    } else if (available < (kRingTarget - kRingBand)) {
        dup_after = kFrameBytes;
        if (dup_after < read_want) {
            read_want -= dup_after;
        } else {
            dup_after = 0;
        }
    }

    const uint32_t got = usb_audio_ring_read(g_slip_in_buf, read_want);
    if (got < read_want) {
        (void)memcpy(dst, g_slip_in_buf, got);
        if (got >= kFrameBytes) {
            (void)memcpy(g_last_frame, g_slip_in_buf + got - kFrameBytes, kFrameBytes);
        }
        (void)memset(dst + got, 0, want - got);
        return;
    }

    const uint32_t out_frames = want / kFrameBytes;
    const uint32_t in_frames = read_want / kFrameBytes;

    if (dup_after > 0 && in_frames > 1U) {
        const uint32_t insert_pos = 1U + (g_audio_slip_cursor % (in_frames - 1U));
        const uint32_t before_bytes = insert_pos * kFrameBytes;
        const uint32_t tail_frames = in_frames - insert_pos;
        (void)memcpy(dst, g_slip_in_buf, before_bytes);
        mix_frame(g_slip_in_buf + ((insert_pos - 1U) * kFrameBytes),
                  g_slip_in_buf + (insert_pos * kFrameBytes),
                  dst + before_bytes);
        (void)memcpy(dst + before_bytes + kFrameBytes,
                     g_slip_in_buf + before_bytes,
                     tail_frames * kFrameBytes);
        g_audio_slip_dup_bytes += dup_after;
        g_audio_slip_cursor += 113U;
    } else if (drop_after > 0 && in_frames > out_frames) {
        const uint32_t drop_pos = 1U + (g_audio_slip_cursor % (out_frames - 1U));
        const uint32_t before_bytes = drop_pos * kFrameBytes;
        const uint32_t after_frames = out_frames - (drop_pos + 1U);
        (void)memcpy(dst, g_slip_in_buf, before_bytes);
        mix_frame(g_slip_in_buf + (drop_pos * kFrameBytes),
                  g_slip_in_buf + ((drop_pos + 1U) * kFrameBytes),
                  dst + before_bytes);
        if (after_frames > 0) {
            (void)memcpy(dst + before_bytes + kFrameBytes,
                         g_slip_in_buf + ((drop_pos + 1U) * kFrameBytes),
                         after_frames * kFrameBytes);
        }
        g_audio_slip_drop_bytes += drop_after;
        g_audio_slip_cursor += 97U;
    } else {
        (void)memcpy(dst, g_slip_in_buf, want);
    }

    if (want >= kFrameBytes) {
        (void)memcpy(g_last_frame, dst + want - kFrameBytes, kFrameBytes);
    }
}
}

extern "C" void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s != &hi2s1) return;
    audio_note_consumed(kAudioBufBytes / 2);
    fill_audio_half(0);
}

extern "C" void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s != &hi2s1) return;
    audio_note_consumed(kAudioBufBytes / 2);
    fill_audio_half(1);
}

int charm_player_profile_usb_audio_run() {
    auto kit = charm::port::init();
    g_console_ctx = kit.console.ctx;
    charm::system::Clock clock{nullptr, charm::system::ClockOps{&charm::port::now_ms, nullptr}};
    charm::system::time::bind(clock);
    out::Scope scope{kit.console};
    MX_I2S1_Init();
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

    MX_USB_DEVICE_AUDIO_Init();
    uart_write("usb: device init ok\n");
    if (HAL_PCD_Start(&hpcd_USB_OTG_FS) != HAL_OK) {
        uart_write("usb: start failed\n");
        Error_Handler();
    }
    uart_write("usb: pcd start ok\n");

    fill_audio_half(0);
    fill_audio_half(1);
    audio_health_reset();
    if (HAL_I2S_Transmit_DMA(&hi2s1,
            reinterpret_cast<uint16_t*>(g_audio_buf),
            kAudioBufBytes / 2) == HAL_OK) {
        g_i2s_started = true;
        g_i2s_active = false;
        uart_write("i2s: dma start ok\n");
    } else {
        uart_write("i2s: dma start failed\n");
    }

    while (1) {
        static uint32_t last_log = 0;
        static bool last_streaming = false;
        const uint32_t now = charm::port::now_ms(nullptr);
        if ((now - last_log) >= 10000) {
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
            const uint32_t out_calls = usb_audio_out_calls();
            const uint32_t iso_out_incomplete = usb_audio_iso_out_incomplete();
            const uint32_t primed = g_audio_stream_primed ? 1u : 0u;
            const uint32_t ring_min = (g_audio_ring_min == 0xFFFFFFFFu) ? 0u : g_audio_ring_min;
            const uint32_t ring_max = g_audio_ring_max;
            const uint32_t slip_drop = g_audio_slip_drop_bytes;
            const uint32_t slip_dup = g_audio_slip_dup_bytes;
            char buf[220];
            const int n = snprintf(
                buf,
                sizeof(buf),
                "usb: audio bytes=%lu pkts=%lu last=%lu freq=%lu cmd=%lu init=%lu cmd_calls=%lu set_if=%lu alt=%lu ovf=%lu ring=%lu ring_ovf=%lu out=%lu iso_inc=%lu primed=%lu ring_min=%lu ring_max=%lu slip_drop=%lu slip_dup=%lu\n",
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
                static_cast<unsigned long>(ring_ovf),
                static_cast<unsigned long>(out_calls),
                static_cast<unsigned long>(iso_out_incomplete),
                static_cast<unsigned long>(primed),
                static_cast<unsigned long>(ring_min),
                static_cast<unsigned long>(ring_max),
                static_cast<unsigned long>(slip_drop),
                static_cast<unsigned long>(slip_dup));
            if (n > 0) {
                uart_write(buf);
            }
            last_log = now;
        }
        const bool streaming = (usb_audio_last_alt_setting() == 1);
        if (streaming != last_streaming) {
            g_i2s_active = false;
            g_audio_stream_primed = false;
            usb_audio_rx_reset();
            audio_health_reset();
            fill_audio_half(0);
            fill_audio_half(1);
            last_streaming = streaming;
        }
        if (streaming && !g_audio_stream_primed && (usb_audio_ring_available() >= kRingStartThreshold)) {
            g_audio_stream_primed = true;
        }
        g_i2s_active = streaming && g_audio_stream_primed;

        if (!g_i2s_started) {
            fill_audio_half(0);
            fill_audio_half(1);
            if (HAL_I2S_Transmit_DMA(&hi2s1,
                    reinterpret_cast<uint16_t*>(g_audio_buf),
                    kAudioBufBytes / 2) == HAL_OK) {
                g_i2s_started = true;
                g_i2s_active = streaming;
            }
        }

        charm::system::time::sleep_ms(10);
    }
}
