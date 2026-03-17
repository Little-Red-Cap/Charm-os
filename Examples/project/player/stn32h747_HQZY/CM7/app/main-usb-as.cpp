#include <cstddef>
#include <cstdio>
#include <cstring>

#include "i2s.h"
#include "sdmmc.h"
#include "usb_device.h"
#include "stm32h7xx_hal_rcc_ex.h"
#include "usbd_audio.h"
#include "usbd_audio_if.h"
#include "usbd_storage_if.h"
#include "usbd_def.h"

import out.api;
import charm.port;
import charm.system.clock;
import charm.system.time;
import player.stm32h7.fs_demo_mmc;
import player.stm32h7.usb_system;

extern "C" {
void MX_I2S1_Init(void);
void MX_SDMMC1_MMC_Init(void);
void Error_Handler(void);
uint32_t usb_out_ep_hits(uint8_t epnum);
uint32_t usb_audio_out_calls(void);
uint32_t usb_audio_out_last_ep(void);
uint32_t usb_audio_iso_out_incomplete(void);
uint32_t usb_audio_out_ep(void);
uint32_t usb_audio_last_set_if_index(void);
extern I2S_HandleTypeDef hi2s1;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern USBD_HandleTypeDef hUsbDeviceFS;
int charm_port_console_write(void* uart, const uint8_t* data, uint16_t len);
}

namespace {
void uart_write(const char* msg);
void* g_console_ctx = nullptr;
volatile uint32_t g_msc_get_max_lun = 0;
volatile uint32_t g_msc_bot_reset = 0;
volatile uint32_t g_msc_cbw = 0;
volatile uint32_t g_msc_cbw_invalid = 0;
volatile uint32_t g_msc_scsi_tur = 0;
volatile uint32_t g_msc_scsi_inq = 0;
volatile uint32_t g_msc_scsi_sense = 0;
volatile uint32_t g_msc_scsi_cap10 = 0;
volatile uint32_t g_msc_scsi_cap16 = 0;
volatile uint32_t g_msc_scsi_fmtcap = 0;
volatile uint32_t g_msc_scsi_read10 = 0;
volatile uint32_t g_msc_scsi_write10 = 0;
volatile uint8_t g_msc_scsi_last = 0;
volatile uint32_t g_msc_cbw_sig = 0;
volatile uint32_t g_msc_cbw_len = 0;
volatile uint8_t g_msc_cbw_flags = 0;
volatile uint8_t g_msc_cbw_cblen = 0;
volatile uint8_t g_msc_cbw_op = 0;
volatile uint32_t g_msc_send_data = 0;
volatile uint32_t g_msc_send_csw = 0;
volatile uint32_t g_msc_send_last_len = 0;
volatile uint32_t g_msc_datain_calls = 0;
volatile uint32_t g_msc_dataout_calls = 0;
volatile uint32_t g_msc_datain_state = 0;
volatile uint32_t g_msc_dataout_state = 0;
volatile uint8_t g_msc_datain_ep = 0;
volatile uint8_t g_msc_dataout_ep = 0;
volatile uint8_t g_msc_cdb0 = 0;
volatile uint8_t g_msc_cdb1 = 0;
volatile uint8_t g_msc_cdb2 = 0;
volatile uint8_t g_msc_cdb3 = 0;
volatile uint8_t g_msc_cdb4 = 0;
volatile uint8_t g_msc_cdb5 = 0;
volatile uint8_t g_msc_cdb_len = 0;

constexpr bool kLogUsbSetup = false;
constexpr bool kLogUsbCfg = false;
constexpr bool kLogAudioStats = true;
constexpr bool kLogMscStats = false;
constexpr bool kLogMscDetail = false;
constexpr bool kLogOutHits = true;
}

extern "C" void app_usb_setup_sniff(const uint8_t setup[8]) {
    if (!kLogUsbSetup) {
        return;
    }
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

extern "C" void usbd_msc_debug_event(uint32_t ev) {
    switch (ev) {
        case 1:
            g_msc_get_max_lun++;
            break;
        case 2:
            g_msc_bot_reset++;
            break;
        case 10:
            g_msc_cbw++;
            break;
        case 11:
            g_msc_cbw_invalid++;
            break;
        default:
            break;
    }
}

extern "C" void usbd_msc_debug_scsi(uint8_t op) {
    g_msc_scsi_last = op;
    switch (op) {
        case 0x00: // TEST_UNIT_READY
            g_msc_scsi_tur++;
            break;
        case 0x03: // REQUEST_SENSE
            g_msc_scsi_sense++;
            break;
        case 0x12: // INQUIRY
            g_msc_scsi_inq++;
            break;
        case 0x23: // READ_FORMAT_CAPACITIES
            g_msc_scsi_fmtcap++;
            break;
        case 0x25: // READ_CAPACITY10
            g_msc_scsi_cap10++;
            break;
        case 0x9E: // READ_CAPACITY16
            g_msc_scsi_cap16++;
            break;
        case 0x28: // READ10
            g_msc_scsi_read10++;
            break;
        case 0x2A: // WRITE10
            g_msc_scsi_write10++;
            break;
        default:
            break;
    }
}

extern "C" void usbd_msc_debug_cbw(uint32_t sig, uint32_t data_len, uint8_t flags, uint8_t cb_len, uint8_t opcode) {
    g_msc_cbw_sig = sig;
    g_msc_cbw_len = data_len;
    g_msc_cbw_flags = flags;
    g_msc_cbw_cblen = cb_len;
    g_msc_cbw_op = opcode;
}

extern "C" void usbd_msc_debug_cdb(const uint8_t* cb, uint8_t cb_len) {
    g_msc_cdb_len = cb_len;
    if (!cb || cb_len == 0) {
        return;
    }
    g_msc_cdb0 = cb_len > 0 ? cb[0] : 0;
    g_msc_cdb1 = cb_len > 1 ? cb[1] : 0;
    g_msc_cdb2 = cb_len > 2 ? cb[2] : 0;
    g_msc_cdb3 = cb_len > 3 ? cb[3] : 0;
    g_msc_cdb4 = cb_len > 4 ? cb[4] : 0;
    g_msc_cdb5 = cb_len > 5 ? cb[5] : 0;
}

extern "C" void usbd_msc_debug_send(uint32_t kind, uint32_t len) {
    g_msc_send_last_len = len;
    if (kind == 1) {
        g_msc_send_data++;
    } else if (kind == 2) {
        g_msc_send_csw++;
    }
}

extern "C" void usbd_msc_debug_bot_state(uint32_t kind, uint32_t state, uint8_t epnum) {
    if (kind == 1) {
        g_msc_datain_calls++;
        g_msc_datain_state = state;
        g_msc_datain_ep = epnum;
    } else if (kind == 2) {
        g_msc_dataout_calls++;
        g_msc_dataout_state = state;
        g_msc_dataout_ep = epnum;
    }
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

void dump_cfg_summary(const uint8_t* cfg, uint16_t total) {
    if (!cfg || total < 9) {
        uart_write("usb: cfg dump invalid\n");
        return;
    }
    uint16_t off = 0;
    while (off + 2 <= total) {
        const uint8_t len = cfg[off];
        const uint8_t type = cfg[off + 1];
        if (len == 0 || (off + len) > total) {
            uart_write("usb: cfg dump break\n");
            break;
        }
        if (type == 0x04 && len >= 9) {
            char buf[120];
            const uint8_t if_num = cfg[off + 2];
            const uint8_t alt = cfg[off + 3];
            const uint8_t ep_num = cfg[off + 4];
            const uint8_t cls = cfg[off + 5];
            const uint8_t sub = cfg[off + 6];
            const uint8_t proto = cfg[off + 7];
            const int n = snprintf(
                buf,
                sizeof(buf),
                "usb: if=%u alt=%u eps=%u cls=0x%02X sub=0x%02X proto=0x%02X\n",
                static_cast<unsigned>(if_num),
                static_cast<unsigned>(alt),
                static_cast<unsigned>(ep_num),
                static_cast<unsigned>(cls),
                static_cast<unsigned>(sub),
                static_cast<unsigned>(proto));
            if (n > 0) {
                uart_write(buf);
            }
        } else if (type == 0x0B && len >= 8) {
            char buf[120];
            const uint8_t first_if = cfg[off + 2];
            const uint8_t if_count = cfg[off + 3];
            const uint8_t cls = cfg[off + 4];
            const uint8_t sub = cfg[off + 5];
            const uint8_t proto = cfg[off + 6];
            const int n = snprintf(
                buf,
                sizeof(buf),
                "usb: iad first=%u count=%u cls=0x%02X sub=0x%02X proto=0x%02X\n",
                static_cast<unsigned>(first_if),
                static_cast<unsigned>(if_count),
                static_cast<unsigned>(cls),
                static_cast<unsigned>(sub),
                static_cast<unsigned>(proto));
            if (n > 0) {
                uart_write(buf);
            }
        } else if (type == 0x05 && len >= 7) {
            char buf[120];
            const uint8_t ep = cfg[off + 2];
            const uint8_t attr = cfg[off + 3];
            const uint16_t mps = static_cast<uint16_t>(cfg[off + 4] | (cfg[off + 5] << 8));
            const uint8_t interval = cfg[off + 6];
            const int n = snprintf(
                buf,
                sizeof(buf),
                "usb: ep=0x%02X attr=0x%02X mps=%u int=%u\n",
                static_cast<unsigned>(ep),
                static_cast<unsigned>(attr),
                static_cast<unsigned>(mps),
                static_cast<unsigned>(interval));
            if (n > 0) {
                uart_write(buf);
            }
        }
        off = static_cast<uint16_t>(off + len);
    }
}

void dump_cfg_hex(const uint8_t* cfg, uint16_t total) {
    if (!cfg || total == 0) {
        uart_write("usb: cfg hex invalid\n");
        return;
    }
    uart_write("usb: cfg hex begin\n");
    char line[80];
    for (uint16_t i = 0; i < total; i += 16) {
        const uint16_t remain = (total - i);
        const uint16_t count = (remain > 16) ? 16 : remain;
        int n = snprintf(line, sizeof(line), "%03u:", static_cast<unsigned>(i));
        if (n < 0) n = 0;
        for (uint16_t j = 0; j < count && n < static_cast<int>(sizeof(line) - 4); ++j) {
            n += snprintf(line + n, sizeof(line) - static_cast<size_t>(n),
                " %02X", static_cast<unsigned>(cfg[i + j]));
        }
        if (n > 0) {
            line[sizeof(line) - 1] = '\0';
            uart_write(line);
            uart_write("\n");
        }
    }
    uart_write("usb: cfg hex end\n");
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
    auto kit = charm::port::init();
    g_console_ctx = kit.console.ctx;
    charm::system::Clock clock{nullptr, charm::system::ClockOps{&charm::port::now_ms, nullptr}};
    charm::system::time::bind(clock);
    out::Scope scope{kit.console};
    MX_I2S1_Init();
    uart_write("boot: uart ok\n");

    MX_SDMMC1_MMC_Init();
    uart_write("boot: sdmmc ok\n");
    if (!fs_boot_init()) {
        uart_write("boot: fs mount failed\n");
    } else {
        uart_write("boot: fs mount ok\n");
    }
    usb_system_init(fs_sd_block_device(), false);

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

    uart_write("usb: device init ok\n");
    if (hUsbDeviceFS.pConfDesc) {
        const auto* cfg = reinterpret_cast<const uint8_t*>(hUsbDeviceFS.pConfDesc);
        const uint16_t total =
            static_cast<uint16_t>(cfg[2] | (static_cast<uint16_t>(cfg[3]) << 8));
        if (kLogUsbCfg) {
            char buf[120];
            const int n = snprintf(
                buf,
                sizeof(buf),
                "usb: cfg len=%u ifs=%u cfg=%u\n",
                static_cast<unsigned>(total),
                static_cast<unsigned>(cfg[4]),
                static_cast<unsigned>(cfg[5]));
            if (n > 0) {
                uart_write(buf);
            }
            dump_cfg_summary(cfg, total);
            dump_cfg_hex(cfg, total);
        }
    } else {
        uart_write("usb: cfg desc null\n");
    }
    if (HAL_PCD_Start(&hpcd_USB_OTG_FS) != HAL_OK) {
        uart_write("usb: start failed\n");
        Error_Handler();
    }
    uart_write("usb: pcd start ok\n");

    while (1) {
        static uint32_t last_log = 0;
        const uint32_t now = charm::port::now_ms(nullptr);
        if ((now - last_log) >= 3000) {
            char buf[200];
            if (kLogAudioStats) {
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
                const uint32_t out_last_ep = usb_audio_out_last_ep();
                const uint32_t out_inc = usb_audio_iso_out_incomplete();
                const uint32_t out_ep = usb_audio_out_ep();
                const uint32_t last_if = usb_audio_last_set_if_index();
                const int n = snprintf(
                    buf,
                    sizeof(buf),
                    "usb: audio bytes=%lu pkts=%lu last=%lu freq=%lu cmd=%lu init=%lu cmd_calls=%lu set_if=%lu alt=%lu if=%lu ovf=%lu ring=%lu ring_ovf=%lu out=%lu ep=%lu out_ep=%lu iso_inc=%lu\n",
                    static_cast<unsigned long>(bytes),
                    static_cast<unsigned long>(pkts),
                    static_cast<unsigned long>(last_size),
                    static_cast<unsigned long>(freq),
                    static_cast<unsigned long>(cmd),
                    static_cast<unsigned long>(init_calls),
                    static_cast<unsigned long>(cmd_calls),
                    static_cast<unsigned long>(set_if_calls),
                    static_cast<unsigned long>(last_alt),
                    static_cast<unsigned long>(last_if),
                    static_cast<unsigned long>(overflows),
                    static_cast<unsigned long>(ring_avail),
                    static_cast<unsigned long>(ring_ovf),
                    static_cast<unsigned long>(out_calls),
                    static_cast<unsigned long>(out_last_ep),
                    static_cast<unsigned long>(out_ep),
                    static_cast<unsigned long>(out_inc));
                if (n > 0) {
                    uart_write(buf);
                }
            }
            if (kLogMscStats) {
                const uint32_t init_calls = usb_msc_init_calls();
                const uint32_t ready_calls = usb_msc_ready_calls();
                const uint32_t cap_calls = usb_msc_capacity_calls();
                const uint32_t read_calls = usb_msc_read_calls();
                const uint32_t write_calls = usb_msc_write_calls();
                const uint32_t last_err = usb_msc_last_error();
                const int n2 = snprintf(
                    buf,
                    sizeof(buf),
                    "usb: msc init=%lu ready=%lu cap=%lu read=%lu write=%lu err=%lu getlun=%lu reset=%lu cbw=%lu cbw_bad=%lu\n",
                    static_cast<unsigned long>(init_calls),
                    static_cast<unsigned long>(ready_calls),
                    static_cast<unsigned long>(cap_calls),
                    static_cast<unsigned long>(read_calls),
                    static_cast<unsigned long>(write_calls),
                    static_cast<unsigned long>(last_err),
                    static_cast<unsigned long>(g_msc_get_max_lun),
                    static_cast<unsigned long>(g_msc_bot_reset),
                    static_cast<unsigned long>(g_msc_cbw),
                    static_cast<unsigned long>(g_msc_cbw_invalid));
                if (n2 > 0) {
                    uart_write(buf);
                }
            }
            if (kLogMscDetail) {
                const int n3 = snprintf(
                    buf,
                    sizeof(buf),
                    "usb: msc scsi last=0x%02X tur=%lu inq=%lu sense=%lu fmtcap=%lu cap10=%lu cap16=%lu r10=%lu w10=%lu\n",
                    static_cast<unsigned>(g_msc_scsi_last),
                    static_cast<unsigned long>(g_msc_scsi_tur),
                    static_cast<unsigned long>(g_msc_scsi_inq),
                    static_cast<unsigned long>(g_msc_scsi_sense),
                    static_cast<unsigned long>(g_msc_scsi_fmtcap),
                    static_cast<unsigned long>(g_msc_scsi_cap10),
                    static_cast<unsigned long>(g_msc_scsi_cap16),
                    static_cast<unsigned long>(g_msc_scsi_read10),
                    static_cast<unsigned long>(g_msc_scsi_write10));
                if (n3 > 0) {
                    uart_write(buf);
                }
                const int n4 = snprintf(
                    buf,
                    sizeof(buf),
                    "usb: msc cbw sig=0x%08lX len=%lu flags=0x%02X cblen=%u op=0x%02X send=%lu csw=%lu last_len=%lu\n",
                    static_cast<unsigned long>(g_msc_cbw_sig),
                    static_cast<unsigned long>(g_msc_cbw_len),
                    static_cast<unsigned>(g_msc_cbw_flags),
                    static_cast<unsigned>(g_msc_cbw_cblen),
                    static_cast<unsigned>(g_msc_cbw_op),
                    static_cast<unsigned long>(g_msc_send_data),
                    static_cast<unsigned long>(g_msc_send_csw),
                    static_cast<unsigned long>(g_msc_send_last_len));
                if (n4 > 0) {
                    uart_write(buf);
                }
                const int n5 = snprintf(
                    buf,
                    sizeof(buf),
                    "usb: msc bot di=%lu(do=%lu) state=%lu/%lu ep=%u/%u cdb_len=%u cdb=%02X %02X %02X %02X %02X %02X\n",
                    static_cast<unsigned long>(g_msc_datain_calls),
                    static_cast<unsigned long>(g_msc_dataout_calls),
                    static_cast<unsigned long>(g_msc_datain_state),
                    static_cast<unsigned long>(g_msc_dataout_state),
                    static_cast<unsigned>(g_msc_datain_ep),
                    static_cast<unsigned>(g_msc_dataout_ep),
                    static_cast<unsigned>(g_msc_cdb_len),
                    static_cast<unsigned>(g_msc_cdb0),
                    static_cast<unsigned>(g_msc_cdb1),
                    static_cast<unsigned>(g_msc_cdb2),
                    static_cast<unsigned>(g_msc_cdb3),
                    static_cast<unsigned>(g_msc_cdb4),
                    static_cast<unsigned>(g_msc_cdb5));
                if (n5 > 0) {
                    uart_write(buf);
                }
            }
            if (kLogOutHits) {
                const int n6 = snprintf(
                    buf,
                    sizeof(buf),
                    "usb: out hits ep1=%lu ep2=%lu\n",
                    static_cast<unsigned long>(usb_out_ep_hits(1)),
                    static_cast<unsigned long>(usb_out_ep_hits(2)));
                if (n6 > 0) {
                    uart_write(buf);
                }
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
        charm::system::time::sleep_ms(10);
    }
}
