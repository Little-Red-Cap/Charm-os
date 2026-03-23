module;

#include <cstdint>
#include <cstddef>

export module board.hqzy;

export namespace board::hqzy {
    struct SdramConfig {
        std::uintptr_t base;
        std::size_t test_words;
        std::uint32_t test_pattern;
        std::uint32_t refresh_rate;
    };

    enum class SdmmcCard : std::uint8_t {
        mmc,
        sd
    };

    constexpr SdramConfig kSdram{
        0xD0000000u,
        1024u,
        0xA5A50000u,
        0x0603u
    };

    constexpr SdmmcCard kSdmmcCard = SdmmcCard::mmc;
    constexpr std::uint32_t kSdmmcInitClockDiv = 480u;
    constexpr std::uint32_t kSdmmcXferClockDiv = 4u;
    constexpr std::uint8_t kSdmmcBusWidth = 4u;
    constexpr bool kSdmmcUseDma = false;
    constexpr bool kSdmmcTry4Bit = false;
    constexpr bool kSdmmcProbeRead = false;
    constexpr bool kSdmmcVerbose = false;
    constexpr bool kSdmmcVerboseGpio = false;
    constexpr std::uint32_t kSdmmcPartitionLba = 496u;
}
