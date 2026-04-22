module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module daplink.usb_device_model;

import daplink.app_config;

export namespace daplink::usb_device_model {
    using UsbProfile = daplink::app_config::UsbProfile;

    constexpr auto& kConfig = daplink::app_config::kConfig;
    constexpr UsbProfile kUsbProfile = kConfig.usb.profile;
    constexpr bool kEnableHid =
        (kUsbProfile == UsbProfile::hid) || (kUsbProfile == UsbProfile::composite);
    constexpr bool kEnableCdc =
        (kUsbProfile == UsbProfile::cdc) || (kUsbProfile == UsbProfile::composite);
    constexpr bool kCdcHasCmdEp = kEnableCdc && kConfig.usb.cdc_has_cmd_ep;
    constexpr std::uint32_t kCdcInTimeoutMs = kConfig.cdc.in_timeout_ms;

    constexpr std::uint8_t kEp0Mps = kConfig.usb.ep0_mps;
    constexpr std::uint8_t kHidEpOut = kConfig.usb.hid_ep_out;
    constexpr std::uint8_t kHidEpIn = kConfig.usb.hid_ep_in;
    constexpr std::uint16_t kHidEpMps = kConfig.usb.hid_ep_mps;
    constexpr std::size_t kHidPacketSize = kConfig.usb.hid_packet_size;
    constexpr std::uint8_t kCdcEpCmd = kConfig.usb.cdc_ep_cmd;
    constexpr std::uint8_t kCdcEpOut = kConfig.usb.cdc_ep_out;
    constexpr std::uint8_t kCdcEpIn = kConfig.usb.cdc_ep_in;
    constexpr std::uint16_t kCdcEpCmdMps = kConfig.usb.cdc_ep_cmd_mps;
    constexpr std::uint16_t kCdcEpMps = kConfig.usb.cdc_ep_mps;

    constexpr std::uint8_t kDeviceDescriptorType = 0x01;
    constexpr std::uint8_t kConfigurationDescriptorType = 0x02;
    constexpr std::uint8_t kStringDescriptorType = 0x03;
    constexpr std::uint8_t kHidDescriptorType = 0x21;
    constexpr std::uint8_t kReportDescriptorType = 0x22;
    constexpr std::uint8_t kManufacturerStringIndex = 0x01;
    constexpr std::uint8_t kProductStringIndex = 0x02;
    constexpr std::uint8_t kSerialStringIndex = 0x03;
    constexpr std::uint8_t kHidInterfaceStringIndex = 0x04;
    constexpr std::uint8_t kCdcFunctionStringIndex = 0x05;
    constexpr std::uint8_t kCdcControlStringIndex = 0x06;
    constexpr std::uint8_t kCdcDataStringIndex = 0x07;

    constexpr std::uint8_t kCdcCommInterface = 0;
    constexpr std::uint8_t kCdcDataInterface = 1;
    constexpr std::uint8_t kHidInterface = kEnableCdc ? 2 : 0;

    constexpr std::uint8_t kDeviceClass =
        (kUsbProfile == UsbProfile::composite) ? 0xEF : 0x00;
    constexpr std::uint8_t kDeviceSubClass =
        (kUsbProfile == UsbProfile::composite) ? 0x02 : 0x00;
    constexpr std::uint8_t kDeviceProtocol =
        (kUsbProfile == UsbProfile::composite) ? 0x01 : 0x00;

    constexpr std::array<std::uint8_t, 18> device_descriptor = {
        0x12, 0x01, 0x00, 0x02, kDeviceClass, kDeviceSubClass, kDeviceProtocol, kEp0Mps,
        static_cast<std::uint8_t>(kConfig.usb.vid & 0xFFU),
        static_cast<std::uint8_t>((kConfig.usb.vid >> 8) & 0xFFU),
        static_cast<std::uint8_t>(kConfig.usb.pid & 0xFFU),
        static_cast<std::uint8_t>((kConfig.usb.pid >> 8) & 0xFFU),
        0x00, 0x01, kManufacturerStringIndex, kProductStringIndex,
        kSerialStringIndex, 0x01
    };

    constexpr std::array<std::uint8_t, 27> hid_report_descriptor = {
        0x06, 0x00, 0xFF,
        0x09, 0x01,
        0xA1, 0x01,
        0x09, 0x02,
        0x15, 0x00,
        0x26, 0xFF, 0x00,
        0x75, 0x08,
        0x95, 0x40,
        0x81, 0x02,
        0x09, 0x03,
        0x95, 0x40,
        0x91, 0x02,
        0xC0
    };

    consteval auto make_configuration_descriptor() {
        if constexpr (kUsbProfile == UsbProfile::composite) {
            return std::array<std::uint8_t, 0x6B>{
                0x09, 0x02, 0x6B, 0x00, 0x03, 0x01, 0x00, 0x80, 0x32,
                0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, kCdcFunctionStringIndex,
                0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, kCdcControlStringIndex,
                0x05, 0x24, 0x00, 0x10, 0x01,
                0x05, 0x24, 0x01, 0x03, 0x01,
                0x04, 0x24, 0x02, 0x06,
                0x05, 0x24, 0x06, 0x00, 0x01,
                0x07, 0x05, kCdcEpCmd, 0x03, 0x08, 0x00, 0x10,
                0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, kCdcDataStringIndex,
                0x07, 0x05, kCdcEpOut, 0x02, 0x40, 0x00, 0x00,
                0x07, 0x05, kCdcEpIn, 0x02, 0x40, 0x00, 0x00,
                0x09, 0x04, 0x02, 0x00, 0x02, 0x03, 0x00, 0x00, kHidInterfaceStringIndex,
                0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
                static_cast<std::uint8_t>(hid_report_descriptor.size()), 0x00,
                0x07, 0x05, kHidEpIn, 0x03, 0x40, 0x00, 0x01,
                0x07, 0x05, kHidEpOut, 0x03, 0x40, 0x00, 0x01
            };
        } else if constexpr (kUsbProfile == UsbProfile::cdc) {
            return std::array<std::uint8_t, 0x4B>{
                0x09, 0x02, 0x4B, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
                0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, kCdcFunctionStringIndex,
                0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, kCdcControlStringIndex,
                0x05, 0x24, 0x00, 0x10, 0x01,
                0x05, 0x24, 0x01, 0x03, 0x01,
                0x04, 0x24, 0x02, 0x06,
                0x05, 0x24, 0x06, 0x00, 0x01,
                0x07, 0x05, kCdcEpCmd, 0x03, 0x08, 0x00, 0x10,
                0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, kCdcDataStringIndex,
                0x07, 0x05, kCdcEpOut, 0x02, 0x40, 0x00, 0x00,
                0x07, 0x05, kCdcEpIn, 0x02, 0x40, 0x00, 0x00
            };
        } else {
            return std::array<std::uint8_t, 0x29>{
                0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
                0x09, 0x04, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, kHidInterfaceStringIndex,
                0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22,
                static_cast<std::uint8_t>(hid_report_descriptor.size()), 0x00,
                0x07, 0x05, kHidEpIn, 0x03, 0x40, 0x00, 0x01,
                0x07, 0x05, kHidEpOut, 0x03, 0x40, 0x00, 0x01
            };
        }
    }

    constexpr auto configuration_descriptor = make_configuration_descriptor();
    static_assert((kUsbProfile == UsbProfile::composite && configuration_descriptor.size() == 0x6B) ||
                  (kUsbProfile == UsbProfile::cdc && configuration_descriptor.size() == 0x4B) ||
                  (kUsbProfile == UsbProfile::hid && configuration_descriptor.size() == 0x29));

    constexpr std::array<std::uint8_t, 4> lang_id_descriptor = {0x04, 0x03, 0x09, 0x04};

    template <std::size_t N>
    consteval auto make_string_descriptor(const char (&text)[N]) {
        static_assert(N > 0);
        constexpr std::size_t kLen = 2 + (N - 1) * 2;
        std::array<std::uint8_t, kLen> descriptor = {};
        descriptor[0] = static_cast<std::uint8_t>(kLen);
        descriptor[1] = kStringDescriptorType;
        for (std::size_t i = 0; i < N - 1; ++i) {
            descriptor[2 + i * 2] = static_cast<std::uint8_t>(text[i]);
            descriptor[3 + i * 2] = 0;
        }
        return descriptor;
    }

    constexpr auto manufacturer_string = make_string_descriptor(daplink::app_config::kUsbManufacturer);
    constexpr auto product_string = make_string_descriptor(daplink::app_config::kUsbProduct);
    constexpr auto serial_string = make_string_descriptor(daplink::app_config::kUsbSerial);
    constexpr auto hid_interface_string = make_string_descriptor(daplink::app_config::kUsbHidInterface);
    constexpr auto cdc_function_string = make_string_descriptor(daplink::app_config::kUsbCdcFunction);
    constexpr auto cdc_control_string = make_string_descriptor(daplink::app_config::kUsbCdcControlInterface);
    constexpr auto cdc_data_string = make_string_descriptor(daplink::app_config::kUsbCdcDataInterface);

    constexpr std::size_t kInvalidDescriptorOffset = static_cast<std::size_t>(-1);

    template <std::size_t N>
    consteval auto find_descriptor_offset(const std::array<std::uint8_t, N>& descriptor,
                                          const std::uint8_t descriptor_type) noexcept -> std::size_t {
        std::size_t offset = 0;
        while (offset + 1 < descriptor.size()) {
            const std::uint8_t length = descriptor[offset];
            const std::uint8_t type = descriptor[offset + 1];
            if (length == 0) {
                break;
            }
            if (type == descriptor_type) {
                return offset;
            }
            offset += length;
        }
        return kInvalidDescriptorOffset;
    }

    constexpr std::size_t kHidDescriptorOffset =
        kEnableHid ? find_descriptor_offset(configuration_descriptor, kHidDescriptorType)
                   : kInvalidDescriptorOffset;
    static_assert(!kEnableHid || (kHidDescriptorOffset != kInvalidDescriptorOffset));

    constexpr auto hid_descriptor() noexcept -> std::span<const std::uint8_t> {
        if constexpr (!kEnableHid || (kHidDescriptorOffset == kInvalidDescriptorOffset)) {
            return {};
        } else {
            return std::span<const std::uint8_t>(
                configuration_descriptor.data() + kHidDescriptorOffset,
                configuration_descriptor[kHidDescriptorOffset]);
        }
    }

    constexpr auto string_descriptor(const std::uint8_t descriptor_index) noexcept
        -> std::span<const std::uint8_t> {
        switch (descriptor_index) {
            case 0:
                return std::span<const std::uint8_t>(lang_id_descriptor);
            case 1:
                return std::span<const std::uint8_t>(manufacturer_string);
            case 2:
                return std::span<const std::uint8_t>(product_string);
            case 3:
                return std::span<const std::uint8_t>(serial_string);
            case 4:
                return std::span<const std::uint8_t>(hid_interface_string);
            case 5:
                return std::span<const std::uint8_t>(cdc_function_string);
            case 6:
                return std::span<const std::uint8_t>(cdc_control_string);
            case 7:
                return std::span<const std::uint8_t>(cdc_data_string);
            default:
                return {};
        }
    }

    constexpr auto descriptor(const std::uint8_t descriptor_type,
                              const std::uint8_t descriptor_index) noexcept
        -> std::span<const std::uint8_t> {
        switch (descriptor_type) {
            case kDeviceDescriptorType:
                return std::span<const std::uint8_t>(device_descriptor);
            case kConfigurationDescriptorType:
                return std::span<const std::uint8_t>(configuration_descriptor);
            case kStringDescriptorType:
                return string_descriptor(descriptor_index);
            case kReportDescriptorType:
                if constexpr (kEnableHid) {
                    return std::span<const std::uint8_t>(hid_report_descriptor);
                } else {
                    return {};
                }
            case kHidDescriptorType:
                return hid_descriptor();
            default:
                return {};
        }
    }
}
