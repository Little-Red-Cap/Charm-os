module;

#include <cstddef>
#include <cstdint>

export module player.stm32h7.app_config;

export namespace player::stm32h7::app::config {
    constexpr std::size_t kRxCap = 64;
    constexpr std::size_t kTxCap = 640;

    constexpr bool kBringupKeySelect = true;
    constexpr bool kBringupWaitKey = true;
    constexpr bool kEnableAudio = true;

    constexpr bool kDebugStopAfterBringup = false;
    constexpr bool kDebugStopAfterChannel = false;
    constexpr bool kDebugStopAfterFs = false;
    constexpr bool kDebugDumpRoot = false;

    constexpr bool kEncoderTestOnBoot = false;
    constexpr std::uint32_t kEncoderTestMs = 5000;
}
