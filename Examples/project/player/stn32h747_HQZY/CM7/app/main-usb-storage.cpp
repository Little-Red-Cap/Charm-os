#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "sdmmc.h"
#include "usb_device.h"

import out.api;
import charm.port;
import charm.system.clock;
import charm.system.time;
import player.stm32h7.fs_demo_mmc;
import player.runtime.hqzy_cm7.usb_storage_bridge;

extern "C" {
void MX_SDMMC1_MMC_Init(void);
void Error_Handler(void);
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
extern USBD_HandleTypeDef hUsbDeviceFS;
int charm_port_console_write(void* uart, const uint8_t* data, uint16_t len);
uint32_t usb_msc_init_calls(void);
uint32_t usb_msc_ready_calls(void);
uint32_t usb_msc_capacity_calls(void);
uint32_t usb_msc_read_calls(void);
uint32_t usb_msc_write_calls(void);
uint32_t usb_msc_last_error(void);
}

namespace {
void* g_console_ctx = nullptr;
std::uint32_t g_scsi_count = 0;
std::uint8_t g_scsi_last = 0;
std::array<std::uint8_t, 8> g_scsi_hist{};
std::size_t g_scsi_hist_head = 0;

void uart_write(const char* msg) {
    if (!msg) return;
    const std::size_t len = std::strlen(msg);
    if (len == 0) return;
    (void)charm_port_console_write(g_console_ctx, reinterpret_cast<const uint8_t*>(msg), static_cast<uint16_t>(len));
}

void dump_bytes(const char* prefix, const uint8_t* data, uint16_t len, uint16_t max_len) {
    if (!prefix || !data || len == 0) return;
    char buf[256];
    int n = std::snprintf(buf, sizeof(buf), "%s size=%u", prefix, static_cast<unsigned>(len));
    if (n <= 0) return;
    std::size_t pos = static_cast<std::size_t>(n);
    const uint16_t count = (len < max_len) ? len : max_len;
    for (uint16_t i = 0; i < count && pos + 4 < sizeof(buf); ++i) {
        n = std::snprintf(buf + pos, sizeof(buf) - pos, " %02X", static_cast<unsigned>(data[i]));
        if (n <= 0) break;
        pos += static_cast<std::size_t>(n);
    }
    if (pos + 2 < sizeof(buf)) {
        buf[pos++] = '\n';
        buf[pos] = '\0';
    }
    uart_write(buf);
}

void dump_usb_descriptors() {
    if (hUsbDeviceFS.pDesc && hUsbDeviceFS.pDesc->GetDeviceDescriptor) {
        uint16_t len = 0;
        const auto* dev = hUsbDeviceFS.pDesc->GetDeviceDescriptor(hUsbDeviceFS.dev_speed, &len);
        dump_bytes("usb: dev_desc", dev, len, len);
    }
    if (hUsbDeviceFS.pClass[0] && hUsbDeviceFS.pClass[0]->GetFSConfigDescriptor) {
        uint16_t len = 0;
        const auto* cfg = hUsbDeviceFS.pClass[0]->GetFSConfigDescriptor(&len);
        dump_bytes("usb: cfg_desc", cfg, len, len < 32 ? len : 32);
    }
}

void dump_scsi_trace() {
    char buf[256];
    const int n = std::snprintf(
        buf,
        sizeof(buf),
        "msc: scsi count=%lu last=0x%02X init=%lu ready=%lu cap=%lu read=%lu write=%lu err=%lu hist=%02X %02X %02X %02X %02X %02X %02X %02X\n",
        static_cast<unsigned long>(g_scsi_count),
        static_cast<unsigned>(g_scsi_last),
        static_cast<unsigned long>(usb_msc_init_calls()),
        static_cast<unsigned long>(usb_msc_ready_calls()),
        static_cast<unsigned long>(usb_msc_capacity_calls()),
        static_cast<unsigned long>(usb_msc_read_calls()),
        static_cast<unsigned long>(usb_msc_write_calls()),
        static_cast<unsigned long>(usb_msc_last_error()),
        static_cast<unsigned>(g_scsi_hist[0]),
        static_cast<unsigned>(g_scsi_hist[1]),
        static_cast<unsigned>(g_scsi_hist[2]),
        static_cast<unsigned>(g_scsi_hist[3]),
        static_cast<unsigned>(g_scsi_hist[4]),
        static_cast<unsigned>(g_scsi_hist[5]),
        static_cast<unsigned>(g_scsi_hist[6]),
        static_cast<unsigned>(g_scsi_hist[7]));
    if (n > 0) {
        uart_write(buf);
    }
}
}

extern "C" void app_usb_setup_sniff(const uint8_t setup[8]) {
    const uint8_t bm = setup[0];
    const uint8_t b = setup[1];
    const uint16_t wValue = static_cast<uint16_t>(setup[2] | (setup[3] << 8));
    const uint16_t wIndex = static_cast<uint16_t>(setup[4] | (setup[5] << 8));
    const uint16_t wLen = static_cast<uint16_t>(setup[6] | (setup[7] << 8));
    char buf[120];
    const int n = std::snprintf(
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

extern "C" void usbd_msc_debug_scsi(uint8_t op) {
    g_scsi_last = op;
    g_scsi_count++;
    g_scsi_hist[g_scsi_hist_head] = op;
    g_scsi_hist_head = (g_scsi_hist_head + 1u) % g_scsi_hist.size();
}

int charm_player_selected_profile_main() {
    auto kit = charm::port::init();
    g_console_ctx = kit.console.ctx;
    charm::system::Clock clock{nullptr, charm::system::ClockOps{&charm::port::now_ms, nullptr}};
    charm::system::time::bind(clock);
    out::Scope scope{kit.console};
    out::println<"boot: uart ok">();

    MX_SDMMC1_MMC_Init();
    out::println<"boot: sdmmc ok">();
    if (!fs_boot_init()) {
        out::println<"boot: fs mount failed">();
    } else {
        out::println<"boot: fs mount ok">();
    }
    usb_storage_bind_device(fs_sd_block_device(), false);

    MX_USB_DEVICE_STORAGE_Init();
    out::println<"usb: device init ok">();
    dump_usb_descriptors();
    if (HAL_PCD_Start(&hpcd_USB_OTG_FS) != HAL_OK) {
        out::println<"usb: start failed">();
        Error_Handler();
    }
    out::println<"usb: pcd start ok">();
    {
        const auto* usb = USB_OTG_FS;
        const auto* usb_dev = reinterpret_cast<USB_OTG_DeviceTypeDef*>(
            USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE);
        const auto* in0 = reinterpret_cast<USB_OTG_INEndpointTypeDef*>(
            USB_OTG_FS_PERIPH_BASE + USB_OTG_IN_ENDPOINT_BASE);
        const auto* out0 = reinterpret_cast<USB_OTG_OUTEndpointTypeDef*>(
            USB_OTG_FS_PERIPH_BASE + USB_OTG_OUT_ENDPOINT_BASE);
        out::println<"usb: reg gusbcfg=0x{:08X} gahbcfg=0x{:08X} gintsts=0x{:08X} gintmsk=0x{:08X} dctl=0x{:08X} dsts=0x{:08X} gotgctl=0x{:08X} gccfg=0x{:08X}">(
            static_cast<std::uint32_t>(usb->GUSBCFG),
            static_cast<std::uint32_t>(usb->GAHBCFG),
            static_cast<std::uint32_t>(usb->GINTSTS),
            static_cast<std::uint32_t>(usb->GINTMSK),
            static_cast<std::uint32_t>(usb_dev->DCTL),
            static_cast<std::uint32_t>(usb_dev->DSTS),
            static_cast<std::uint32_t>(usb->GOTGCTL),
            static_cast<std::uint32_t>(usb->GCCFG));
        out::println<"usb: ep0 diepctl=0x{:08X} diepint=0x{:08X} doepctl=0x{:08X} doepint=0x{:08X}">(
            static_cast<std::uint32_t>(in0->DIEPCTL),
            static_cast<std::uint32_t>(in0->DIEPINT),
            static_cast<std::uint32_t>(out0->DOEPCTL),
            static_cast<std::uint32_t>(out0->DOEPINT));
    }

    std::uint32_t last_scsi_count = 0;
    while (true) {
        if (g_scsi_count != last_scsi_count) {
            dump_scsi_trace();
            last_scsi_count = g_scsi_count;
        }
        charm::system::time::sleep_ms(1000);
    }
}
