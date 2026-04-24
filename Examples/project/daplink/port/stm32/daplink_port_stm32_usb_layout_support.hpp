#ifndef DAPLINK_PORT_STM32_USB_LAYOUT_SUPPORT_HPP
#define DAPLINK_PORT_STM32_USB_LAYOUT_SUPPORT_HPP

#include <cstdint>

namespace daplink::port::stm32 {
    struct UsbPmaLayout {
        std::uint16_t ep0_out = 0;
        std::uint16_t ep0_in = 0;
        std::uint16_t hid_in = 0;
        std::uint16_t hid_out = 0;
        std::uint16_t cdc_cmd = 0;
        std::uint16_t cdc_out = 0;
        std::uint16_t cdc_in = 0;
    };

    consteval auto make_usb_pma_layout_from_f1_scale(const std::uint16_t scale) noexcept -> UsbPmaLayout {
        return {
            static_cast<std::uint16_t>(0x18U * scale),
            static_cast<std::uint16_t>(0x58U * scale),
            static_cast<std::uint16_t>(0x98U * scale),
            static_cast<std::uint16_t>(0xD8U * scale),
            static_cast<std::uint16_t>(0x118U * scale),
            static_cast<std::uint16_t>(0x120U * scale),
            static_cast<std::uint16_t>(0x160U * scale),
        };
    }

    template <UsbPmaLayout Layout>
    struct UsbPmaLayoutTraits {
        static inline constexpr UsbPmaLayout kUsbPmaLayout = Layout;
        static inline constexpr std::uint16_t kUsbPmaEp0Out = Layout.ep0_out;
        static inline constexpr std::uint16_t kUsbPmaEp0In = Layout.ep0_in;
        static inline constexpr std::uint16_t kUsbPmaHidIn = Layout.hid_in;
        static inline constexpr std::uint16_t kUsbPmaHidOut = Layout.hid_out;
        static inline constexpr std::uint16_t kUsbPmaCdcCmd = Layout.cdc_cmd;
        static inline constexpr std::uint16_t kUsbPmaCdcOut = Layout.cdc_out;
        static inline constexpr std::uint16_t kUsbPmaCdcIn = Layout.cdc_in;
    };

    template <std::uint16_t Scale>
    using F1ScaledUsbPmaLayoutTraits = UsbPmaLayoutTraits<make_usb_pma_layout_from_f1_scale(Scale)>;
}

#endif
