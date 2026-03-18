export module player.stm32h7.player_entry;

import charm.system.time;
import player.stm32h7.fs_demo_mmc;
import player.stm32h7.usb_system;

extern "C" {
struct PCD_HandleTypeDef;
using HAL_StatusTypeDef = int;
void MX_I2S1_Init(void);
void MX_SDMMC1_MMC_Init(void);
HAL_StatusTypeDef HAL_PCD_Start(PCD_HandleTypeDef* hpcd);
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

export void player_boot() {}

export bool player_init_fs() {
    MX_SDMMC1_MMC_Init();
    return fs_boot_init();
}

export void player_init_audio() {
    MX_I2S1_Init();
}

export bool player_init_usb() {
    usb_system_init(fs_sd_block_device(), false);
    if (HAL_PCD_Start(&hpcd_USB_OTG_FS) != 0) {
        return false;
    }
    return true;
}

export void player_loop() {
    while (true) {
        charm::system::time::sleep_ms(1000);
    }
}
