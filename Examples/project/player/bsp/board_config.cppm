module;

#include <cstdint>
#include <cstddef>

export module player.stm32h7.board_config;

import board.active;

export namespace player::stm32h7::board {
    using SdramConfig = ::board::active::SdramConfig;
    using SdmmcCard = ::board::active::SdmmcCard;

    constexpr SdramConfig kSdram = ::board::active::kSdram;

    constexpr SdmmcCard kSdmmcCard = ::board::active::kSdmmcCard;
    constexpr std::uint32_t kSdmmcInitClockDiv = ::board::active::kSdmmcInitClockDiv;
    constexpr std::uint32_t kSdmmcXferClockDiv = ::board::active::kSdmmcXferClockDiv;
    constexpr std::uint8_t kSdmmcBusWidth = ::board::active::kSdmmcBusWidth;
    constexpr bool kSdmmcUseDma = ::board::active::kSdmmcUseDma;
    constexpr bool kSdmmcTry4Bit = ::board::active::kSdmmcTry4Bit;
    constexpr bool kSdmmcProbeRead = ::board::active::kSdmmcProbeRead;
    constexpr bool kSdmmcVerbose = ::board::active::kSdmmcVerbose;
    constexpr bool kSdmmcVerboseGpio = ::board::active::kSdmmcVerboseGpio;
    constexpr std::uint32_t kSdmmcPartitionLba = ::board::active::kSdmmcPartitionLba;

    constexpr bool kKeyActiveHigh = ::board::active::kKeyActiveHigh;

    constexpr bool kFmcInitOnBoot = true;
    constexpr bool kSdramSelftestOnBoot = true;
    constexpr bool kSdramSelftestInBringup = false;
    constexpr bool kEnableSdmmcInit = false;
    constexpr bool kEnableUsbMsc = true;
    constexpr bool kUseStUsbStack = true;
    constexpr bool kEnableUsbAudio = true;
    constexpr bool kUseUsbAudioOnBoot = true;
    constexpr bool kEnableDisplay = false;
    constexpr bool kUseOutLoggerEarly = false;
    constexpr bool kUseDmaConsole = false;
}
