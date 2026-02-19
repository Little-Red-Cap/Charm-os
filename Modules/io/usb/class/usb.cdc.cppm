module;

#include <cstdint>
#include <cstddef>
#include <span>

export module usb.class_cdc;

import usb.common;
import usb.device;

export namespace usb::class_driver {
    using usb::u8;
    using usb::u16;
    using usb::u32;

    constexpr u8 cdc_class = 0x02;
    constexpr u8 cdc_subclass_acm = 0x02;
    constexpr u8 cdc_protocol_at = 0x01;

    constexpr u8 cdc_data_class = 0x0A;

    enum class CdcDescriptorType : u8 {
        cs_interface = 0x24,
        cs_endpoint = 0x25,
    };

    enum class CdcFunctionalType : u8 {
        header = 0x00,
        call_management = 0x01,
        abstract_control = 0x02,
        union_iface = 0x06,
    };

    struct CdcHeaderDescriptor {
        u8 length{5};
        CdcDescriptorType type{CdcDescriptorType::cs_interface};
        CdcFunctionalType sub_type{CdcFunctionalType::header};
        u16 bcd_cdc{0x0110};
    };

    struct CdcCallManagementDescriptor {
        u8 length{5};
        CdcDescriptorType type{CdcDescriptorType::cs_interface};
        CdcFunctionalType sub_type{CdcFunctionalType::call_management};
        u8 capabilities{0x00};
        u8 data_interface{1};
    };

    struct CdcAcmDescriptor {
        u8 length{4};
        CdcDescriptorType type{CdcDescriptorType::cs_interface};
        CdcFunctionalType sub_type{CdcFunctionalType::abstract_control};
        u8 capabilities{0x02};
    };

    struct CdcUnionDescriptor {
        u8 length{5};
        CdcDescriptorType type{CdcDescriptorType::cs_interface};
        CdcFunctionalType sub_type{CdcFunctionalType::union_iface};
        u8 control_interface{0};
        u8 subordinate_interface{1};
    };

    struct CdcLineCoding {
        u32 baud{115200};
        u8 stop_bits{0}; // 0=1 stop bit
        u8 parity{0};    // 0=none
        u8 data_bits{8};
    };

    struct CdcConfig {
        u8 ctrl_ifc{0};
        u8 data_ifc{1};
        u8 ep_notify{0x81};
        u8 ep_out{0x01};
        u8 ep_in{0x82};
        u16 ep_mps{64};
    };

    struct CdcOps {
        void (*on_line_coding)(void* ctx, const CdcLineCoding& coding) noexcept { nullptr };
        void (*on_control_line)(void* ctx, u16 value) noexcept { nullptr };
        std::span<u8> (*tx_buffer)(void* ctx) noexcept { nullptr };
        std::span<u8> (*rx_buffer)(void* ctx) noexcept { nullptr };
        void (*on_rx_done)(void* ctx, std::size_t len) noexcept { nullptr };
        void (*on_tx_done)(void* ctx, std::size_t len) noexcept { nullptr };
    };

    class CdcAcm {
    public:
        explicit CdcAcm(void* ctx, const CdcOps& ops) noexcept
            : ctx_(ctx), ops_(ops) {}

        device::ClassOps class_ops() noexcept {
            device::ClassOps ops{};
            ops.setup = &CdcAcm::handle_setup;
            ops.reset = &CdcAcm::handle_reset;
            return ops;
        }

        const CdcConfig& config() const noexcept { return cfg_; }

    private:
        static constexpr u8 req_set_line_coding = 0x20;
        static constexpr u8 req_get_line_coding = 0x21;
        static constexpr u8 req_set_control_line_state = 0x22;

        static bool handle_setup(void* ctx, const device::ControlRequest& req, device::ControlResponse& resp) noexcept {
            auto* self = static_cast<CdcAcm*>(ctx);
            if (!self) return false;
            switch (req.setup.b_request) {
            case req_set_control_line_state:
                if (self->ops_.on_control_line) {
                    self->ops_.on_control_line(self->ctx_, req.setup.w_value);
                }
                resp.data = {};
                resp.zlp = true;
                return true;
            case req_get_line_coding:
                resp.data = std::span<const u8>(
                    reinterpret_cast<const u8*>(&self->coding_),
                    sizeof(self->coding_));
                resp.zlp = false;
                return true;
            case req_set_line_coding:
                // Host-to-device data stage not wired in the skeleton.
                resp.data = {};
                resp.zlp = true;
                return true;
            default:
                return false;
            }
        }

        static void handle_reset(void* ctx) noexcept {
            auto* self = static_cast<CdcAcm*>(ctx);
            if (self) self->coding_ = {};
        }

        void* ctx_{nullptr};
        CdcOps ops_{};
        CdcConfig cfg_{};
        CdcLineCoding coding_{};
    };
} // namespace usb::class_driver
