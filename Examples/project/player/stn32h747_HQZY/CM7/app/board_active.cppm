module;

#include <cstdint>

export module board.active;

import board.hqzy;

export namespace board::active {
    using SdramConfig = board::hqzy::SdramConfig;
    using SdmmcCard = board::hqzy::SdmmcCard;

    constexpr SdramConfig kSdram = board::hqzy::kSdram;

    constexpr SdmmcCard kSdmmcCard = board::hqzy::kSdmmcCard;
    constexpr std::uint32_t kSdmmcInitClockDiv = board::hqzy::kSdmmcInitClockDiv;
    constexpr std::uint32_t kSdmmcXferClockDiv = board::hqzy::kSdmmcXferClockDiv;
    constexpr std::uint8_t kSdmmcBusWidth = board::hqzy::kSdmmcBusWidth;
    constexpr bool kSdmmcUseDma = board::hqzy::kSdmmcUseDma;
    constexpr bool kSdmmcTry4Bit = board::hqzy::kSdmmcTry4Bit;
    constexpr bool kSdmmcProbeRead = board::hqzy::kSdmmcProbeRead;
    constexpr bool kSdmmcVerbose = board::hqzy::kSdmmcVerbose;
    constexpr bool kSdmmcVerboseGpio = board::hqzy::kSdmmcVerboseGpio;
    constexpr std::uint32_t kSdmmcPartitionLba = board::hqzy::kSdmmcPartitionLba;
}
