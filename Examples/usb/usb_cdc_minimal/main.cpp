module;

#include <array>
#include <cstddef>
#include <span>

import usb.common;
import usb.device;
import usb.class_cdc;

namespace {
    constexpr usb::u16 kLangs[] = { 0x0409 };
    constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
    constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
    constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm CDC");
    constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

    static const usb::StringTable<4> kStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
            std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
        }
    };

    constexpr usb::DeviceDescriptor kDeviceDesc{
        18,
        usb::DescriptorType::device,
        0x0200,
        0,
        0,
        0,
        64,
        0x1209,
        0x0001,
        0x0100,
        1,
        2,
        3,
        1
    };

    struct CdcExampleContext {
        std::array<usb::u8, 256> config_buf{};
        std::size_t config_size{0};
    };

    bool get_descriptor(void* ctx, usb::DescriptorType type, usb::u8 index, std::span<const usb::u8>& out) noexcept {
        auto* ex = static_cast<CdcExampleContext*>(ctx);
        switch (type) {
        case usb::DescriptorType::device:
            out = std::span<const usb::u8>(
                reinterpret_cast<const usb::u8*>(&kDeviceDesc),
                sizeof(kDeviceDesc));
            return true;
        case usb::DescriptorType::configuration:
            if (!ex) return false;
            out = std::span<const usb::u8>(ex->config_buf.data(), ex->config_size);
            return true;
        case usb::DescriptorType::string:
            out = kStrings[index];
            return !out.empty();
        default:
            return false;
        }
    }
}

int main() {
    CdcExampleContext ctx{};
    usb::DescriptorBuilder builder{ std::span<usb::u8>(ctx.config_buf.data(), ctx.config_buf.size()) };

    usb::ConfigDescriptor cfg{};
    cfg.attributes = 0x80;
    cfg.max_power = 50;
    builder.begin_config(cfg);

    usb::InterfaceAssociationDescriptor iad{};
    iad.first_interface = 0;
    iad.interface_count = 2;
    iad.function_class = usb::class_driver::cdc_class;
    iad.function_subclass = usb::class_driver::cdc_subclass_acm;
    iad.function_protocol = usb::class_driver::cdc_protocol_at;
    builder.add_class_descriptor(iad);

    usb::InterfaceDescriptor ctrl_ifc{};
    ctrl_ifc.interface_number = 0;
    ctrl_ifc.num_endpoints = 1;
    ctrl_ifc.interface_class = usb::class_driver::cdc_class;
    ctrl_ifc.interface_subclass = usb::class_driver::cdc_subclass_acm;
    ctrl_ifc.interface_protocol = usb::class_driver::cdc_protocol_at;
    builder.add_interface(ctrl_ifc);

    builder.add_class_descriptor(usb::class_driver::CdcHeaderDescriptor{});
    builder.add_class_descriptor(usb::class_driver::CdcCallManagementDescriptor{});
    builder.add_class_descriptor(usb::class_driver::CdcAcmDescriptor{});
    builder.add_class_descriptor(usb::class_driver::CdcUnionDescriptor{});

    usb::EndpointDescriptor ep_notify{};
    ep_notify.endpoint_address = 0x81;
    ep_notify.attributes = static_cast<usb::u8>(usb::EndpointType::interrupt);
    ep_notify.max_packet_size = 8;
    ep_notify.interval = 10;
    builder.add_endpoint(ep_notify);

    usb::InterfaceDescriptor data_ifc{};
    data_ifc.interface_number = 1;
    data_ifc.num_endpoints = 2;
    data_ifc.interface_class = usb::class_driver::cdc_data_class;
    builder.add_interface(data_ifc);

    usb::EndpointDescriptor ep_out{};
    ep_out.endpoint_address = 0x01;
    ep_out.attributes = static_cast<usb::u8>(usb::EndpointType::bulk);
    ep_out.max_packet_size = 64;
    builder.add_endpoint(ep_out);

    usb::EndpointDescriptor ep_in{};
    ep_in.endpoint_address = 0x82;
    ep_in.attributes = static_cast<usb::u8>(usb::EndpointType::bulk);
    ep_in.max_packet_size = 64;
    builder.add_endpoint(ep_in);

    builder.end_config();
    ctx.config_size = builder.writer.offset;

    usb::device::DescriptorProvider desc_provider{};
    desc_provider.ctx = &ctx;
    desc_provider.get_descriptor = get_descriptor;

    usb::device::Device device{};
    device.set_descriptor_provider(desc_provider);

    usb::class_driver::CdcOps cdc_ops{};
    usb::class_driver::CdcAcm cdc{ &device, cdc_ops };
    auto class_ops = cdc.class_ops();
    device.set_class(&cdc, &class_ops);

    return 0;
}
