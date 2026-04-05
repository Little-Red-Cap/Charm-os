module;

#include <array>
#include <span>

export module usb.model;

import usb.class_cdc;
import usb.class_msc;
import usb.class_msc_block;
import usb.common;
import usb.dsl;
import usb.spec;

export namespace usb::model {
    using StringTableView = std::span<const std::span<const usb::u8>>;

    struct DeviceModel {
        usb::dsl::DeviceInfo dev_info{};
        usb::dsl::ConfigInfo cfg_info{};
        StringTableView strings{};
    };

    struct InterfaceIntent {
        usb::dsl::InterfaceInfo info{};
        usb::u8 num_endpoints{0};
        bool fixed_number{false};
    };

    struct EndpointIntent {
        usb::dsl::EndpointInfo info{};
        bool fixed_address{true};
    };

    struct MscFunctionModel {
        const char* cap_name{"usb.msc0"};
        const char* block_cap{"block.sd0"};
        usb::class_driver::MscConfig msc_cfg{};
        usb::class_driver::MscBlockConfig storage_cfg{};
        std::array<InterfaceIntent, 1> interfaces{};
        std::array<EndpointIntent, 2> endpoints{};

        [[nodiscard]] std::span<const InterfaceIntent> interface_intents() const noexcept {
            return std::span<const InterfaceIntent>(interfaces.data(), interfaces.size());
        }

        [[nodiscard]] std::span<const EndpointIntent> endpoint_intents() const noexcept {
            return std::span<const EndpointIntent>(endpoints.data(), endpoints.size());
        }
    };

    struct CdcFunctionModel {
        const char* cap_name{"usb.cdc0"};
        usb::class_driver::CdcConfig cdc_cfg{};
        std::array<InterfaceIntent, 2> interfaces{};
        std::array<EndpointIntent, 3> endpoints{};

        [[nodiscard]] std::span<const InterfaceIntent> interface_intents() const noexcept {
            return std::span<const InterfaceIntent>(interfaces.data(), interfaces.size());
        }

        [[nodiscard]] std::span<const EndpointIntent> endpoint_intents() const noexcept {
            return std::span<const EndpointIntent>(endpoints.data(), endpoints.size());
        }
    };

    struct MscDeviceModel {
        DeviceModel device{};
        MscFunctionModel msc{};
    };

    struct CdcDeviceModel {
        DeviceModel device{};
        CdcFunctionModel cdc{};
    };

    struct MscCdcDeviceModel {
        DeviceModel device{};
        MscFunctionModel msc{};
        CdcFunctionModel cdc{};
    };

    inline DeviceModel build_device(const usb::spec::DeviceSpec& spec) noexcept {
        DeviceModel model{};
        model.dev_info.vendor_id = spec.vendor_id;
        model.dev_info.product_id = spec.product_id;
        model.dev_info.bcd_device = spec.bcd_device;
        model.dev_info.bcd_usb = spec.bcd_usb;
        model.dev_info.device_class = spec.device_class;
        model.dev_info.device_subclass = spec.device_subclass;
        model.dev_info.device_protocol = spec.device_protocol;
        model.dev_info.max_packet_size0 = spec.max_packet_size0;
        model.dev_info.i_manufacturer = spec.i_manufacturer;
        model.dev_info.i_product = spec.i_product;
        model.dev_info.i_serial = spec.i_serial;
        model.dev_info.num_configurations = spec.num_configurations;

        model.cfg_info.configuration_value = spec.configuration_value;
        model.cfg_info.attributes = spec.attributes;
        model.cfg_info.max_power = spec.max_power;
        model.cfg_info.i_configuration = spec.i_configuration;

        model.strings = spec.strings;
        return model;
    }

    inline MscFunctionModel build_msc(const usb::spec::MscFunctionSpec& spec) noexcept {
        MscFunctionModel model{};
        model.cap_name = spec.cap_name;
        model.block_cap = spec.block_cap;
        model.msc_cfg.ep_out = spec.ep_out;
        model.msc_cfg.ep_in = spec.ep_in;
        model.msc_cfg.ep_mps = spec.ep_mps;
        model.storage_cfg.vendor = spec.vendor;
        model.storage_cfg.product = spec.product;
        model.storage_cfg.revision = spec.revision;
        model.storage_cfg.removable = spec.removable;
        model.storage_cfg.read_only = spec.read_only;

        auto& iface = model.interfaces[0];
        iface.info.interface_number = 0;
        iface.info.interface_class = usb::class_driver::msc_class;
        iface.info.interface_subclass = usb::class_driver::msc_subclass_sbc;
        iface.info.interface_protocol = usb::class_driver::msc_protocol_bulk_only;
        iface.num_endpoints = 2;
        iface.fixed_number = false;

        auto& out = model.endpoints[0];
        out.info.address = spec.ep_out;
        out.info.type = usb::EndpointType::bulk;
        out.info.max_packet_size = spec.ep_mps;
        out.fixed_address = true;

        auto& in = model.endpoints[1];
        in.info.address = spec.ep_in;
        in.info.type = usb::EndpointType::bulk;
        in.info.max_packet_size = spec.ep_mps;
        in.fixed_address = true;

        return model;
    }

    inline CdcFunctionModel build_cdc(const usb::spec::CdcFunctionSpec& spec) noexcept {
        CdcFunctionModel model{};
        model.cap_name = spec.cap_name;
        model.cdc_cfg.ctrl_ifc = spec.ctrl_ifc;
        model.cdc_cfg.data_ifc = spec.data_ifc;
        model.cdc_cfg.ep_notify = spec.ep_notify;
        model.cdc_cfg.ep_out = spec.ep_out;
        model.cdc_cfg.ep_in = spec.ep_in;
        model.cdc_cfg.ep_mps = spec.ep_mps;

        auto& ctrl = model.interfaces[0];
        ctrl.info.interface_number = 0;
        ctrl.info.interface_class = usb::class_driver::cdc_class;
        ctrl.info.interface_subclass = usb::class_driver::cdc_subclass_acm;
        ctrl.info.interface_protocol = usb::class_driver::cdc_protocol_at;
        ctrl.num_endpoints = 1;
        ctrl.fixed_number = false;

        auto& data = model.interfaces[1];
        data.info.interface_number = 0;
        data.info.interface_class = usb::class_driver::cdc_data_class;
        data.info.interface_subclass = 0;
        data.info.interface_protocol = 0;
        data.num_endpoints = 2;
        data.fixed_number = false;

        auto& notify = model.endpoints[0];
        notify.info.address = spec.ep_notify;
        notify.info.type = usb::EndpointType::interrupt;
        notify.info.max_packet_size = spec.ep_mps;
        notify.info.interval = 10;
        notify.fixed_address = true;

        auto& out = model.endpoints[1];
        out.info.address = spec.ep_out;
        out.info.type = usb::EndpointType::bulk;
        out.info.max_packet_size = spec.ep_mps;
        out.fixed_address = true;

        auto& in = model.endpoints[2];
        in.info.address = spec.ep_in;
        in.info.type = usb::EndpointType::bulk;
        in.info.max_packet_size = spec.ep_mps;
        in.fixed_address = true;

        return model;
    }

    inline MscDeviceModel build(const usb::spec::MscDeviceSpec& spec) noexcept {
        MscDeviceModel model{};
        model.device = build_device(spec.device);
        model.msc = build_msc(spec.msc);
        return model;
    }

    inline CdcDeviceModel build(const usb::spec::CdcDeviceSpec& spec) noexcept {
        CdcDeviceModel model{};
        model.device = build_device(spec.device);
        model.cdc = build_cdc(spec.cdc);
        return model;
    }

    inline MscCdcDeviceModel build(const usb::spec::MscCdcDeviceSpec& spec) noexcept {
        MscCdcDeviceModel model{};
        model.device = build_device(spec.device);
        model.msc = build_msc(spec.msc);
        model.cdc = build_cdc(spec.cdc);
        return model;
    }
}

export namespace usb {
    inline usb::model::MscDeviceModel build(const usb::spec::MscDeviceSpec& spec) noexcept {
        return usb::model::build(spec);
    }

    inline usb::model::CdcDeviceModel build(const usb::spec::CdcDeviceSpec& spec) noexcept {
        return usb::model::build(spec);
    }

    inline usb::model::MscCdcDeviceModel build(const usb::spec::MscCdcDeviceSpec& spec) noexcept {
        return usb::model::build(spec);
    }
}
