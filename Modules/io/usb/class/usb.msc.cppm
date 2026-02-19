module;

#include <cstdint>
#include <cstring>
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

    struct MscCbw {
        u32 signature{0x43425355};
        u32 tag{0};
        u32 data_transfer_length{0};
        u8 flags{0};
        u8 lun{0};
        u8 cb_length{0};
        u8 cb[16]{};
    };

    struct MscCsw {
        u32 signature{0x53425355};
        u32 tag{0};
        u32 residue{0};
        u8 status{0};
    };

    enum class MscStatus : u8 {
        passed = 0,
        failed = 1,
        phase_error = 2,
    };

    enum class MscPhase : u8 {
        cbw,
        data,
        csw,
    };

    struct MscOps {
        bool (*on_command)(void* ctx, std::span<const u8> cbw) noexcept { nullptr };
        void (*on_reset)(void* ctx) noexcept { nullptr };
    };

    class MscDevice {
    public:
        explicit MscDevice(void* ctx, const MscOps& ops) noexcept
            : ctx_(ctx), ops_(ops) {}

        const device::ClassOps* class_ops() const noexcept {
            static const device::ClassOps ops{
                &MscDevice::handle_setup,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                &MscDevice::handle_reset,
            };
            return &ops;
        }

        const MscConfig& config() const noexcept { return cfg_; }
        MscPhase phase() const noexcept { return phase_; }
        void reset_phase() noexcept { phase_ = MscPhase::cbw; }
        const MscCbw& last_cbw() const noexcept { return last_cbw_; }

        static bool validate_cbw(const MscCbw& cbw) noexcept {
            if (cbw.signature != 0x43425355) return false;
            if (cbw.cb_length == 0 || cbw.cb_length > 16) return false;
            return true;
        }

        bool handle_cbw(std::span<const u8> data) noexcept {
            if (data.size() < sizeof(MscCbw)) return false;
            std::memcpy(&last_cbw_, data.data(), sizeof(MscCbw));
            if (!validate_cbw(last_cbw_)) return false;
            if (ops_.on_command && !ops_.on_command(ctx_, std::span<const u8>(last_cbw_.cb, last_cbw_.cb_length))) {
                return false;
            }
            if (last_cbw_.data_transfer_length > 0) {
                begin_data();
            } else {
                begin_csw();
            }
            return true;
        }

        std::span<const u8> make_csw(MscStatus status, u32 residue = 0) noexcept {
            last_csw_.tag = last_cbw_.tag;
            last_csw_.residue = residue;
            last_csw_.status = static_cast<u8>(status);
            return std::span<const u8>(
                reinterpret_cast<const u8*>(&last_csw_),
                sizeof(MscCsw));
        }

        void begin_data() noexcept { phase_ = MscPhase::data; }
        void begin_csw() noexcept { phase_ = MscPhase::csw; }
        void begin_cbw() noexcept { phase_ = MscPhase::cbw; }

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
            if (self) {
                self->phase_ = MscPhase::cbw;
            }
        }

        void* ctx_{nullptr};
        MscOps ops_{};
        MscConfig cfg_{};
        MscPhase phase_{MscPhase::cbw};
        MscCbw last_cbw_{};
        MscCsw last_csw_{};
    };
}
