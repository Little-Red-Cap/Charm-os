module;

#include <array>
#include <cstddef>
#include <cstring>
#include <span>

export module usb.dsl;

import usb.common;
import usb.device;
import usb.class_cdc;
import usb.class_msc;
import usb.class_uac;

export namespace usb::dsl {
    using usb::u8;
    using usb::u16;

    inline bool validate_descriptor_stream(std::span<const u8> bytes) noexcept;

    template <std::size_t N>
    consteval bool validate_descriptor_stream(const std::array<u8, N>& bytes) noexcept;

    struct DeviceInfo {
        u16 vendor_id{0};
        u16 product_id{0};
        u16 bcd_device{0x0100};
        u16 bcd_usb{0x0200};
        u8 device_class{0};
        u8 device_subclass{0};
        u8 device_protocol{0};
        u8 max_packet_size0{64};
        u8 i_manufacturer{0};
        u8 i_product{0};
        u8 i_serial{0};
        u8 num_configurations{1};
    };

    template <u8 Address>
    consteval void validate_endpoint_address() {
        static_assert((Address & 0x0F) != 0, "Endpoint address 0x00 is reserved for EP0");
        static_assert((Address & 0x0F) <= 0x0F, "Endpoint number out of range");
    }

    template <u16 MaxPacketSize>
    consteval void validate_max_packet_size() {
        static_assert(MaxPacketSize > 0, "MaxPacketSize must be non-zero");
        static_assert(MaxPacketSize <= 1024, "MaxPacketSize exceeds USB limits");
    }

    struct ConfigInfo {
        u8 configuration_value{1};
        u8 attributes{0x80};
        u8 max_power{50};
        u8 i_configuration{0};
    };

    struct InterfaceInfo {
        u8 interface_number{0};
        u8 alternate_setting{0};
        u8 interface_class{0};
        u8 interface_subclass{0};
        u8 interface_protocol{0};
        u8 i_interface{0};
    };

    struct EndpointInfo {
        u8 address{0};
        EndpointType type{EndpointType::bulk};
        u16 max_packet_size{64};
        u8 interval{0};
    };

    inline DeviceDescriptor make_device_descriptor(const DeviceInfo& info) noexcept {
        DeviceDescriptor desc{};
        desc.bcd_usb = info.bcd_usb;
        desc.device_class = info.device_class;
        desc.device_subclass = info.device_subclass;
        desc.device_protocol = info.device_protocol;
        desc.max_packet_size0 = info.max_packet_size0;
        desc.vendor_id = info.vendor_id;
        desc.product_id = info.product_id;
        desc.bcd_device = info.bcd_device;
        desc.manufacturer = info.i_manufacturer;
        desc.product = info.i_product;
        desc.serial_number = info.i_serial;
        desc.num_configurations = info.num_configurations;
        return desc;
    }

    inline ConfigDescriptor make_config_descriptor(const ConfigInfo& info) noexcept {
        ConfigDescriptor desc{};
        desc.configuration_value = info.configuration_value;
        desc.attributes = info.attributes;
        desc.max_power = info.max_power;
        desc.configuration = info.i_configuration;
        return desc;
    }

    inline InterfaceDescriptor make_interface_descriptor(const InterfaceInfo& info, u8 num_endpoints) noexcept {
        InterfaceDescriptor desc{};
        desc.interface_number = info.interface_number;
        desc.alternate_setting = info.alternate_setting;
        desc.num_endpoints = num_endpoints;
        desc.interface_class = info.interface_class;
        desc.interface_subclass = info.interface_subclass;
        desc.interface_protocol = info.interface_protocol;
        desc.interface = info.i_interface;
        return desc;
    }

    inline EndpointDescriptor make_endpoint_descriptor(const EndpointInfo& info) noexcept {
        EndpointDescriptor desc{};
        desc.endpoint_address = info.address;
        desc.attributes = static_cast<u8>(info.type);
        desc.max_packet_size = info.max_packet_size;
        desc.interval = info.interval;
        return desc;
    }

    class ConfigBuilder {
    public:
        explicit ConfigBuilder(std::span<u8> buffer) noexcept
            : builder_(buffer), buffer_(buffer) {}

        void set_interface_count(u8 count) noexcept { builder_.set_interface_count(count); }

        bool begin(const ConfigDescriptor& cfg) noexcept {
            begun_ = builder_.begin_config(cfg);
            return begun_;
        }

        bool add_iad(const InterfaceAssociationDescriptor& desc) noexcept {
            if (!begun_) return false;
            return builder_.writer.write_object(desc);
        }

        bool add_interface(const InterfaceDescriptor& desc) noexcept {
            if (!begun_) return false;
            return builder_.add_interface(desc);
        }

        bool add_endpoint(const EndpointDescriptor& desc) noexcept {
            if (!begun_) return false;
            const auto addr = desc.endpoint_address;
            if ((addr & 0x0F) == 0) return false;
            const auto idx = static_cast<std::size_t>((addr & 0x0F) | ((addr & 0x80) ? 0x10 : 0x00));
            if (idx < ep_used_.size()) {
                if (ep_used_[idx]) return false;
                ep_used_[idx] = true;
            }
            return builder_.add_endpoint(desc);
        }

        bool add_class_descriptor(std::span<const u8> bytes) noexcept {
            if (!begun_) return false;
            if (!validate_descriptor_stream(bytes)) return false;
            return builder_.writer.write_bytes(bytes);
        }

        template <std::size_t N>
        bool add_class_descriptor(const std::array<u8, N>& bytes) noexcept {
            static_assert(N <= 255, "USB class descriptor stream too long");
            static_assert(validate_descriptor_stream(bytes), "Invalid class descriptor stream");
            return add_class_descriptor(std::span<const u8>(bytes.data(), bytes.size()));
        }

        bool end() noexcept {
            if (!begun_) return false;
            return builder_.end_config();
        }

        std::span<const u8> view() const noexcept {
            return buffer_.subspan(0, builder_.writer.offset);
        }

    private:
        DescriptorBuilder builder_;
        std::span<u8> buffer_{};
        std::array<bool, 32> ep_used_{};
        bool begun_{false};
    };

    struct CdcAcmClassDescriptors {
        std::array<u8, 19> bytes{};

        std::span<const u8> view() const noexcept {
            return std::span<const u8>(bytes.data(), bytes.size());
        }
    };

    inline CdcAcmClassDescriptors make_cdc_acm_class_descriptors(const class_driver::CdcConfig& cfg) noexcept {
        using namespace class_driver;
        CdcHeaderDescriptor header{};
        CdcCallManagementDescriptor call_mgmt{};
        CdcAcmDescriptor acm{};
        CdcUnionDescriptor udesc{};

        call_mgmt.data_interface = cfg.data_ifc;
        udesc.control_interface = cfg.ctrl_ifc;
        udesc.subordinate_interface = cfg.data_ifc;

        CdcAcmClassDescriptors out{};
        std::size_t offset = 0;
        auto copy = [&](const auto& obj) noexcept {
            std::memcpy(out.bytes.data() + offset, &obj, sizeof(obj));
            offset += sizeof(obj);
        };
        copy(header);
        copy(call_mgmt);
        copy(acm);
        copy(udesc);
        return out;
    }

    inline bool build_cdc_acm(ConfigBuilder& builder,
                              const class_driver::CdcConfig& cfg,
                              std::span<const u8> class_desc) noexcept {
        InterfaceInfo ctrl{};
        ctrl.interface_number = cfg.ctrl_ifc;
        ctrl.interface_class = class_driver::cdc_class;
        ctrl.interface_subclass = class_driver::cdc_subclass_acm;
        ctrl.interface_protocol = class_driver::cdc_protocol_at;

        InterfaceInfo data{};
        data.interface_number = cfg.data_ifc;
        data.interface_class = class_driver::cdc_data_class;
        data.interface_subclass = 0;
        data.interface_protocol = 0;

        EndpointInfo notify{};
        notify.address = cfg.ep_notify;
        notify.type = EndpointType::interrupt;
        notify.max_packet_size = cfg.ep_mps;
        notify.interval = 10;

        EndpointInfo out{};
        out.address = cfg.ep_out;
        out.type = EndpointType::bulk;
        out.max_packet_size = cfg.ep_mps;

        EndpointInfo in{};
        in.address = cfg.ep_in;
        in.type = EndpointType::bulk;
        in.max_packet_size = cfg.ep_mps;

        if (!builder.add_interface(make_interface_descriptor(ctrl, 1))) return false;
        if (!class_desc.empty()) {
            if (!builder.add_class_descriptor(class_desc)) return false;
        }
        if (!builder.add_endpoint(make_endpoint_descriptor(notify))) return false;

        if (!builder.add_interface(make_interface_descriptor(data, 2))) return false;
        if (!builder.add_endpoint(make_endpoint_descriptor(out))) return false;
        if (!builder.add_endpoint(make_endpoint_descriptor(in))) return false;
        return true;
    }

    inline bool build_msc(ConfigBuilder& builder,
                          const class_driver::MscConfig& cfg,
                          std::span<const u8> class_desc) noexcept {
        InterfaceInfo iface{};
        iface.interface_number = cfg.interface_number;
        iface.interface_class = class_driver::msc_class;
        iface.interface_subclass = class_driver::msc_subclass_sbc;
        iface.interface_protocol = class_driver::msc_protocol_bulk_only;

        EndpointInfo out{};
        out.address = cfg.ep_out;
        out.type = EndpointType::bulk;
        out.max_packet_size = cfg.ep_mps;

        EndpointInfo in{};
        in.address = cfg.ep_in;
        in.type = EndpointType::bulk;
        in.max_packet_size = cfg.ep_mps;

        if (!builder.add_interface(make_interface_descriptor(iface, 2))) return false;
        if (!class_desc.empty()) {
            if (!builder.add_class_descriptor(class_desc)) return false;
        }
        if (!builder.add_endpoint(make_endpoint_descriptor(out))) return false;
        if (!builder.add_endpoint(make_endpoint_descriptor(in))) return false;
        return true;
    }

    inline bool build_uac(ConfigBuilder& builder,
                          const class_driver::UacConfig& cfg,
                          std::span<const u8> control_desc,
                          std::span<const u8> stream_desc) noexcept {
        InterfaceInfo ctrl{};
        ctrl.interface_number = cfg.ctrl_ifc;
        ctrl.interface_class = class_driver::uac_class;
        ctrl.interface_subclass = class_driver::uac_subclass_audio_control;
        ctrl.interface_protocol = class_driver::uac_protocol_universal;

        InterfaceInfo stream{};
        stream.interface_number = cfg.stream_ifc;
        stream.interface_class = class_driver::uac_class;
        stream.interface_subclass = class_driver::uac_subclass_audio_streaming;
        stream.interface_protocol = class_driver::uac_protocol_universal;

        EndpointInfo out{};
        out.address = cfg.ep_out;
        out.type = EndpointType::isochronous;
        out.max_packet_size = cfg.ep_mps;
        out.interval = cfg.ep_interval;

        EndpointInfo in{};
        in.address = cfg.ep_in;
        in.type = EndpointType::isochronous;
        in.max_packet_size = cfg.ep_mps;
        in.interval = cfg.ep_interval;

        if (!builder.add_interface(make_interface_descriptor(ctrl, 0))) return false;
        if (!control_desc.empty()) {
            if (!builder.add_class_descriptor(control_desc)) return false;
        }

        u8 stream_eps = 0;
        if (cfg.ep_out != 0) stream_eps++;
        if (cfg.ep_in != 0) stream_eps++;
        if (!builder.add_interface(make_interface_descriptor(stream, stream_eps))) return false;
        if (!stream_desc.empty()) {
            if (!builder.add_class_descriptor(stream_desc)) return false;
        }
        if (cfg.ep_out != 0) {
            if (!builder.add_endpoint(make_endpoint_descriptor(out))) return false;
        }
        if (cfg.ep_in != 0) {
            if (!builder.add_endpoint(make_endpoint_descriptor(in))) return false;
        }
        return true;
    }

    struct DeviceBuildContext {
        std::span<u8> device_desc_storage{};
        std::span<u8> config_storage{};
        device::DescriptorTable* table{nullptr};
        device::ConfigTree* tree{nullptr};
    };

    inline bool build_cdc_acm_device(DeviceBuildContext& ctx,
                                     const DeviceInfo& dev_info,
                                     const ConfigInfo& cfg_info,
                                     const class_driver::CdcConfig& cdc_cfg,
                                     std::span<const u8> class_desc,
                                     const std::span<const u8>* strings,
                                     std::size_t string_count) noexcept {
        if (!ctx.table || !ctx.tree) return false;
        if (ctx.device_desc_storage.size() < sizeof(DeviceDescriptor)) return false;
        if (ctx.config_storage.empty()) return false;

        const auto device_desc = make_device_descriptor(dev_info);
        std::memcpy(ctx.device_desc_storage.data(), &device_desc, sizeof(device_desc));

        ConfigBuilder builder(ctx.config_storage);
        if (!builder.begin(make_config_descriptor(cfg_info))) return false;
        if (!build_cdc_acm(builder, cdc_cfg, class_desc)) return false;
        if (!builder.end()) return false;

        ctx.tree->buffer = ctx.config_storage;
        ctx.tree->view = builder.view();

        ctx.table->device = std::span<const u8>(ctx.device_desc_storage.data(), sizeof(DeviceDescriptor));
        ctx.table->configuration = ctx.tree->view;
        ctx.table->strings = strings;
        ctx.table->string_count = string_count;
        return true;
    }

    inline bool build_msc_device(DeviceBuildContext& ctx,
                                 const DeviceInfo& dev_info,
                                 const ConfigInfo& cfg_info,
                                 const class_driver::MscConfig& msc_cfg,
                                 std::span<const u8> class_desc,
                                 const std::span<const u8>* strings,
                                 std::size_t string_count) noexcept {
        if (!ctx.table || !ctx.tree) return false;
        if (ctx.device_desc_storage.size() < sizeof(DeviceDescriptor)) return false;
        if (ctx.config_storage.empty()) return false;

        const auto device_desc = make_device_descriptor(dev_info);
        std::memcpy(ctx.device_desc_storage.data(), &device_desc, sizeof(device_desc));

        ConfigBuilder builder(ctx.config_storage);
        if (!builder.begin(make_config_descriptor(cfg_info))) return false;
        if (!build_msc(builder, msc_cfg, class_desc)) return false;
        if (!builder.end()) return false;

        ctx.tree->buffer = ctx.config_storage;
        ctx.tree->view = builder.view();

        ctx.table->device = std::span<const u8>(ctx.device_desc_storage.data(), sizeof(DeviceDescriptor));
        ctx.table->configuration = ctx.tree->view;
        ctx.table->strings = strings;
        ctx.table->string_count = string_count;
        return true;
    }

    inline bool build_uac_device(DeviceBuildContext& ctx,
                                 const DeviceInfo& dev_info,
                                 const ConfigInfo& cfg_info,
                                 const class_driver::UacConfig& uac_cfg,
                                 std::span<const u8> control_desc,
                                 std::span<const u8> stream_desc,
                                 const std::span<const u8>* strings,
                                 std::size_t string_count) noexcept {
        if (!ctx.table || !ctx.tree) return false;
        if (ctx.device_desc_storage.size() < sizeof(DeviceDescriptor)) return false;
        if (ctx.config_storage.empty()) return false;

        const auto device_desc = make_device_descriptor(dev_info);
        std::memcpy(ctx.device_desc_storage.data(), &device_desc, sizeof(device_desc));

        ConfigBuilder builder(ctx.config_storage);
        if (!builder.begin(make_config_descriptor(cfg_info))) return false;
        if (!build_uac(builder, uac_cfg, control_desc, stream_desc)) return false;
        if (!builder.end()) return false;

        ctx.tree->buffer = ctx.config_storage;
        ctx.tree->view = builder.view();

        ctx.table->device = std::span<const u8>(ctx.device_desc_storage.data(), sizeof(DeviceDescriptor));
        ctx.table->configuration = ctx.tree->view;
        ctx.table->strings = strings;
        ctx.table->string_count = string_count;
        return true;
    }

    // Minimal class descriptor payloads (placeholders).
    struct MscClassDescriptors {
        std::array<u8, 0> bytes{};
        std::span<const u8> view() const noexcept { return {}; }
    };

    struct UacClassDescriptors {
        std::array<u8, 0> control{};
        std::array<u8, 0> stream{};
        std::span<const u8> control_view() const noexcept { return {}; }
        std::span<const u8> stream_view() const noexcept { return {}; }
    };

    // Example usage:
    //   std::array<u8, 64> dev_desc{};
    //   std::array<u8, 256> cfg_desc{};
    //   DescriptorTable table{};
    //   ConfigTree tree{};
    //   DeviceBuildContext ctx{dev_desc, cfg_desc, &table, &tree};
    //   DeviceInfo dev_info{};
    //   ConfigInfo cfg_info{};
    //   class_driver::CdcConfig cdc_cfg{};
    //   auto cdc_desc = make_cdc_acm_class_descriptors(cdc_cfg);
    //   build_cdc_acm_device(ctx, dev_info, cfg_info, cdc_cfg,
    //                        cdc_desc.view(), nullptr, 0);
    //
    //   class_driver::MscConfig msc_cfg{};
    //   MscClassDescriptors msc_desc{};
    //   build_msc_device(ctx, dev_info, cfg_info, msc_cfg,
    //                    msc_desc.view(), nullptr, 0);
    //
    //   class_driver::UacConfig uac_cfg{};
    //   UacClassDescriptors uac_desc{};
    //   build_uac_device(ctx, dev_info, cfg_info, uac_cfg,
    //                    uac_desc.control_view(),
    //                    uac_desc.stream_view(),
    //                    nullptr, 0);

    inline bool validate_descriptor_stream(std::span<const u8> bytes) noexcept {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const auto len = bytes[offset];
            if (len < 2) return false;
            const auto next = offset + len;
            if (next > bytes.size()) return false;
            offset = next;
        }
        return offset == bytes.size();
    }

    template <std::size_t N>
    consteval bool validate_descriptor_stream(const std::array<u8, N>& bytes) noexcept {
        std::size_t offset = 0;
        while (offset < N) {
            const auto len = bytes[offset];
            if (len < 2) return false;
            const auto next = offset + len;
            if (next > N) return false;
            offset = next;
        }
        return offset == N;
    }
} // namespace usb::dsl
