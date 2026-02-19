module;

#include <cstdint>

export module usb.class_uac;

import usb.common;
import usb.device;

export namespace usb::class_driver {
    constexpr u8 uac_class = 0x01;
    constexpr u8 uac_subclass_audio_control = 0x01;
    constexpr u8 uac_subclass_audio_streaming = 0x02;
    constexpr u8 uac_protocol_universal = 0x20;

    struct Uac2HeaderDescriptor {
        u8 length{9};
        u8 type{0x24};
        u8 sub_type{0x01};
        u16 bcd_adc{0x0200};
        u8 category{0x00};
        u16 total_length{0};
        u8 control{0x00};
    };

    struct UacInputTerminalDescriptor {
        u8 length{17};
        u8 type{0x24};
        u8 sub_type{0x02};
        u8 terminal_id{1};
        u16 terminal_type{0x0101};
        u8 assoc_terminal{0};
        u8 clk_source_id{0};
        u8 num_channels{2};
        u32 channel_config{0};
        u8 channel_names{0};
        u16 controls{0};
        u8 terminal{0};
    };

    struct UacOutputTerminalDescriptor {
        u8 length{12};
        u8 type{0x24};
        u8 sub_type{0x03};
        u8 terminal_id{2};
        u16 terminal_type{0x0301};
        u8 assoc_terminal{0};
        u8 source_id{1};
        u8 clk_source_id{0};
        u16 controls{0};
        u8 terminal{0};
    };

    struct UacAsGeneralDescriptor {
        u8 length{16};
        u8 type{0x24};
        u8 sub_type{0x01};
        u8 terminal_link{1};
        u8 controls{0};
        u8 format_type{0x01};
        u32 formats{0x00000001};
        u8 nr_channels{2};
        u32 channel_config{0};
        u8 channel_names{0};
    };

    struct UacConfig {
        u8 ctrl_ifc{0};
        u8 stream_ifc{1};
        u8 ep_out{0x01};
        u8 ep_in{0x81};
        u16 ep_mps{192};
        u8 ep_interval{1};
    };

    struct UacOps {
        void (*on_alt_setting)(void* ctx, u8 alt) noexcept { nullptr };
        void (*on_stream_start)(void* ctx) noexcept { nullptr };
        void (*on_stream_stop)(void* ctx) noexcept { nullptr };
    };

    class UacDevice {
    public:
        explicit UacDevice(void* ctx, const UacOps& ops) noexcept
            : ctx_(ctx), ops_(ops) {}

        device::ClassOps class_ops() noexcept {
            device::ClassOps ops{};
            ops.setup = &UacDevice::handle_setup;
            ops.reset = &UacDevice::handle_reset;
            return ops;
        }

        const UacConfig& config() const noexcept { return cfg_; }

    private:
        static bool handle_setup(void* ctx, const device::ControlRequest& req, device::ControlResponse& resp) noexcept {
            auto* self = static_cast<UacDevice*>(ctx);
            if (!self) return false;
            (void)req;
            resp.data = {};
            resp.zlp = true;
            return true;
        }

        static void handle_reset(void* ctx) noexcept {
            auto* self = static_cast<UacDevice*>(ctx);
            if (self && self->ops_.on_stream_stop) {
                self->ops_.on_stream_stop(self->ctx_);
            }
        }

        void* ctx_{nullptr};
        UacOps ops_{};
        UacConfig cfg_{};
    };
}
