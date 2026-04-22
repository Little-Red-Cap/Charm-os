#ifndef DAPLINK_PORT_API_HPP
#define DAPLINK_PORT_API_HPP

#include "daplink_port_main.hpp"
#include "gpio.h"
#include "usart.h"
#include "usb.h"

#include "port/stm32/daplink_port_stm32_api_support.hpp"
#include "port/stm32/daplink_port_stm32_usb_layout_support.hpp"

namespace daplink::port {
    inline constexpr auto kUsbPmaLayout = daplink::port::stm32::make_usb_pma_layout_from_f1_scale(2U);
    inline constexpr std::uint16_t kUsbPmaEp0Out = kUsbPmaLayout.ep0_out;
    inline constexpr std::uint16_t kUsbPmaEp0In = kUsbPmaLayout.ep0_in;
    inline constexpr std::uint16_t kUsbPmaHidIn = kUsbPmaLayout.hid_in;
    inline constexpr std::uint16_t kUsbPmaHidOut = kUsbPmaLayout.hid_out;
    inline constexpr std::uint16_t kUsbPmaCdcCmd = kUsbPmaLayout.cdc_cmd;
    inline constexpr std::uint16_t kUsbPmaCdcOut = kUsbPmaLayout.cdc_out;
    inline constexpr std::uint16_t kUsbPmaCdcIn = kUsbPmaLayout.cdc_in;
}

#endif
