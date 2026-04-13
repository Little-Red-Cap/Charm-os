export module board.active;

import player.stm32h7.board_config;

export namespace board::active {
    using SdramConfig = player::stm32h7::board::SdramConfig;
    using SdmmcCard = player::stm32h7::board::SdmmcCard;
    using SdmmcConfig = player::stm32h7::board::SdmmcConfig;
    using KeyConfig = player::stm32h7::board::KeyConfig;
    using BoardConfig = player::stm32h7::board::BoardConfig;

    constexpr BoardConfig kConfig = player::stm32h7::board::kConfig;
    constexpr SdramConfig kSdram = player::stm32h7::board::kSdram;
    constexpr SdmmcConfig kSdmmc = player::stm32h7::board::kSdmmc;
    constexpr KeyConfig kKey = player::stm32h7::board::kKey;

    constexpr SdmmcCard kSdmmcCard = player::stm32h7::board::kSdmmcCard;
    constexpr auto kSdmmcInitClockDiv = player::stm32h7::board::kSdmmcInitClockDiv;
    constexpr auto kSdmmcXferClockDiv = player::stm32h7::board::kSdmmcXferClockDiv;
    constexpr auto kSdmmcBusWidth = player::stm32h7::board::kSdmmcBusWidth;
    constexpr bool kSdmmcUseDma = player::stm32h7::board::kSdmmcUseDma;
    constexpr bool kSdmmcTry4Bit = player::stm32h7::board::kSdmmcTry4Bit;
    constexpr bool kSdmmcProbeRead = player::stm32h7::board::kSdmmcProbeRead;
    constexpr bool kSdmmcVerbose = player::stm32h7::board::kSdmmcVerbose;
    constexpr bool kSdmmcVerboseGpio = player::stm32h7::board::kSdmmcVerboseGpio;
    constexpr auto kSdmmcPartitionLba = player::stm32h7::board::kSdmmcPartitionLba;

    constexpr bool kKeyActiveHigh = player::stm32h7::board::kKeyActiveHigh;
}
