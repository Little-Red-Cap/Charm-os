#include <cstdint>

#include "sdmmc.h"
#include "usb_device.h"

import out.api;
import charm.port;
import charm.system.clock;
import charm.system.time;
import player.stm32h7.fs_demo_mmc;
import player.stm32h7.usb_system;

extern "C" {
void MX_SDMMC1_MMC_Init(void);
void Error_Handler(void);
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

int main() {
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

    while (true) {
        charm::system::time::sleep_ms(1000);
    }
}
