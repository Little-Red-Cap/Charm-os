module;

#include "daplink_board.hpp"

export module daplink.board_config;

export namespace daplink::board_config {
    inline constexpr auto kTClkPin = daplink::board_target::kTClkPin;
    inline GPIO_TypeDef* const kTClkPort = daplink::board_target::kTClkPort;
    inline constexpr auto kTRstPin = daplink::board_target::kTRstPin;
    inline GPIO_TypeDef* const kTRstPort = daplink::board_target::kTRstPort;
    inline constexpr auto kDbgLedPin = daplink::board_target::kDbgLedPin;
    inline GPIO_TypeDef* const kDbgLedPort = daplink::board_target::kDbgLedPort;
    inline constexpr auto kDbgLedOnState = daplink::board_target::kDbgLedOnState;
    inline constexpr auto kDbgLedOffState = daplink::board_target::kDbgLedOffState;
    inline constexpr auto kTdoSwoPin = daplink::board_target::kTdoSwoPin;
    inline GPIO_TypeDef* const kTdoSwoPort = daplink::board_target::kTdoSwoPort;
    inline constexpr auto kTDioInPin = daplink::board_target::kTDioInPin;
    inline GPIO_TypeDef* const kTDioInPort = daplink::board_target::kTDioInPort;
    inline constexpr auto kTDioOutPin = daplink::board_target::kTDioOutPin;
    inline GPIO_TypeDef* const kTDioOutPort = daplink::board_target::kTDioOutPort;
    inline constexpr auto kConnectLedPin = daplink::board_target::kConnectLedPin;
    inline GPIO_TypeDef* const kConnectLedPort = daplink::board_target::kConnectLedPort;
    inline constexpr auto kConnectLedOnState = daplink::board_target::kConnectLedOnState;
    inline constexpr auto kConnectLedOffState = daplink::board_target::kConnectLedOffState;
    inline constexpr bool kHasDbgLed = daplink::board_target::kHasDbgLed;
    inline constexpr bool kHasConnectLed = daplink::board_target::kHasConnectLed;
    inline constexpr bool kHasUsbConnectSwitch = daplink::board_target::kHasUsbConnectSwitch;
    inline constexpr auto kUsbConnectSwitchPin = daplink::board_target::kUsbConnectSwitchPin;
    inline GPIO_TypeDef* const kUsbConnectSwitchPort = daplink::board_target::kUsbConnectSwitchPort;
    inline constexpr auto kUsbConnectSwitchOnState = daplink::board_target::kUsbConnectSwitchOnState;
}
