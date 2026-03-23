module;

#include <cstddef>
#include <cstdint>

export module player.stm32h7.player_config;

export namespace player::stm32h7::config {
    constexpr std::size_t kRxCap = 64;
    constexpr std::size_t kTxCap = 640;

    constexpr bool kBringupKeySelect = true;
    constexpr bool kBringupWaitKey = true;
    constexpr bool kFmcInitOnBoot = true;
    constexpr bool kSdramSelftestOnBoot = true;
    constexpr bool kSdramSelftestInBringup = false;
    constexpr bool kEnableSdmmcInit = false;
    constexpr bool kEnableUsbMsc = true;
    constexpr bool kEnableAudio = true;
    constexpr bool kEnableDisplay = false;

    constexpr bool kDebugStopAfterBringup = false;
    constexpr bool kDebugStopAfterChannel = false;
    constexpr bool kDebugStopAfterFs = false;
    constexpr bool kDebugDumpRoot = false;

    constexpr bool kUseOutLoggerEarly = false;
    constexpr bool kUseDmaConsole = false;

    constexpr bool kEncoderTestOnBoot = false;
    constexpr std::uint32_t kEncoderTestMs = 5000;
}
