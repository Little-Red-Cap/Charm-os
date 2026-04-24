export module player.stm32h7.system_entry;

import charm.system.time;
import player.stm32h7.fs_demo_mmc;
import player.runtime.hqzy_cm7.usb_storage_bridge;

extern "C" {
    void MX_SDMMC1_MMC_Init(void);
}

namespace {
constexpr bool kEnableFs = true;
constexpr bool kEnableUsbStorage = true;
constexpr bool kEnableAudio = false;

void init_fs(void* uart) {
    if (!kEnableFs) return;
    MX_SDMMC1_MMC_Init();
    (void)uart;
    (void)fs_boot_init();
}

void init_usb_storage(void* uart) {
    if (!kEnableUsbStorage) return;
    usb_system_init(fs_sd_block_device(), false);
    (void)uart;
}

void init_audio(void* uart) {
    if (!kEnableAudio) return;
    (void)uart;
    // TODO: 统一接入音频播放路径。
}
}

export [[deprecated("改用 PLAYER_PROFILE + profiles/ 主线")]] void system_run(void* uart) {
    init_fs(uart);
    init_usb_storage(uart);
    init_audio(uart);

    // TODO: 汇总显示/输入等系统级运行逻辑。
    while (1) {
        charm::system::time::sleep_ms(1000);
    }
}
