module;

#include <span>

export module usb.spec;

import usb.common;

export namespace usb::spec {
    using StringTableView = std::span<const std::span<const usb::u8>>;

    struct DeviceSpec {
        usb::u16 vendor_id{0};
        usb::u16 product_id{0};
        usb::u16 bcd_device{0x0100};
        usb::u16 bcd_usb{0x0200};
        usb::u8 device_class{0};
        usb::u8 device_subclass{0};
        usb::u8 device_protocol{0};
        usb::u8 i_manufacturer{1};
        usb::u8 i_product{2};
        usb::u8 i_serial{3};
        usb::u8 max_packet_size0{64};
        usb::u8 num_configurations{1};
        usb::u8 configuration_value{1};
        usb::u8 attributes{0x80};
        usb::u8 max_power{50};
        usb::u8 i_configuration{0};
        StringTableView strings{};
    };

    struct MscFunctionSpec {
        const char* cap_name{"usb.msc0"};
        const char* block_cap{"block.sd0"};
        const char* vendor{"Charm"};
        const char* product{"BlockDevice"};
        const char* revision{"1.00"};
        bool removable{true};
        bool read_only{false};
        usb::u8 ep_out{0x01};
        usb::u8 ep_in{0x81};
        usb::u16 ep_mps{64};
    };

    struct CdcFunctionSpec {
        const char* cap_name{"usb.cdc0"};
        usb::u8 ctrl_ifc{0};
        usb::u8 data_ifc{1};
        usb::u8 ep_notify{0x81};
        usb::u8 ep_out{0x01};
        usb::u8 ep_in{0x82};
        usb::u16 ep_mps{64};
    };

    struct MscDeviceSpec {
        DeviceSpec device{};
        MscFunctionSpec msc{};
    };

    struct MscCdcDeviceSpec {
        DeviceSpec device{};
        MscFunctionSpec msc{};
        CdcFunctionSpec cdc{};
    };

    inline constexpr MscDeviceSpec msc_device(const DeviceSpec& device,
                                              const MscFunctionSpec& msc) noexcept {
        return MscDeviceSpec{device, msc};
    }

    inline constexpr MscCdcDeviceSpec msc_cdc_device(const DeviceSpec& device,
                                                     const MscFunctionSpec& msc,
                                                     const CdcFunctionSpec& cdc) noexcept {
        return MscCdcDeviceSpec{device, msc, cdc};
    }
}
