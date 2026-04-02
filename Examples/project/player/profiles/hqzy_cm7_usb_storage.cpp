#include <cstdint>

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
}

int charm_player_profile_usb_storage_run() {
    auto kit = charm::port::init();
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
    usb_system_init(fs_sd_block_device(), false);

    MX_USB_DEVICE_Init();
    out::println<"usb: device init ok">();
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

    while (true) {
        charm::system::time::sleep_ms(1000);
    }
}
