module;

#include <cstdint>
#include <span>

export module usb.class_msc;

import usb.common;
import usb.device;

export namespace usb::class_driver {
    constexpr u8 msc_class = 0x08;
    constexpr u8 msc_subclass_sbc = 0x06;
    constexpr u8 msc_protocol_bulk_only = 0x50;

    struct MscConfig {
        u8 interface_number{0};
        u8 ep_out{0x01};
        u8 ep_in{0x81};
        u16 ep_mps{64};
    };

    struct MscOps {
        bool (*on_command)(void* ctx, std::span<const u8> cbw) noexcept { nullptr };
        void (*on_reset)(void* ctx) noexcept { nullptr };
    };

    class MscDevice {
    public:
        explicit MscDevice(void* ctx, const MscOps& ops) noexcept
            : ctx_(ctx), ops_(ops) {}

        device::ClassOps class_ops() noexcept {
            device::ClassOps ops{};
            ops.setup = &MscDevice::handle_setup;
            ops.reset = &MscDevice::handle_reset;
            return ops;
        }

        const MscConfig& config() const noexcept { return cfg_; }

    private:
        static bool handle_setup(void* ctx, const device::ControlRequest& req, device::ControlResponse& resp) noexcept {
            auto* self = static_cast<MscDevice*>(ctx);
            if (!self) return false;
            (void)req;
            resp.data = {};
            resp.zlp = true;
            return true;
        }

        static void handle_reset(void* ctx) noexcept {
            auto* self = static_cast<MscDevice*>(ctx);
            if (self && self->ops_.on_reset) {
                self->ops_.on_reset(self->ctx_);
            }
        }

        void* ctx_{nullptr};
        MscOps ops_{};
        MscConfig cfg_{};
    };
}
