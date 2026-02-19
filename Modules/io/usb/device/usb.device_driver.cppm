module;

#include <cstddef>
#include <span>

export module usb.device_driver;

import usb.common;
import usb.device;
import usb.driver;
import usb.ep0_driver;

export namespace usb::device {
    class DeviceDriver {
    public:
        DeviceDriver(Device& dev, void* dcd_ctx, driver::DcdOps ops) noexcept
            : dev_(dev),
              dcd_ctx_(dcd_ctx),
              dcd_ops_(ops),
              ep0_(dev_, this, device::Ep0DriverOps{
                  &DeviceDriver::ep0_send_in,
                  &DeviceDriver::ep0_send_zlp,
                  &DeviceDriver::ep0_stall}) {}

        void on_setup(const SetupPacket& setup) noexcept {
            ControlResponse resp{};
            set_pending(setup);
            const auto result = ep0_.on_setup(setup, resp);
            if (result != Ep0Result::ok) {
                clear_pending();
            }
        }

        void on_out_data(std::span<const u8> data) noexcept {
            ControlResponse resp{};
            const auto result = ep0_.on_out_data(data, resp);
            if (result != Ep0Result::ok) {
                clear_pending();
            }
        }

        void on_in_complete(std::size_t sent, bool sent_zlp = false) noexcept {
            ep0_.on_in_complete(sent, sent_zlp);
            apply_pending_if_ready();
        }

        void on_reset() noexcept {
            clear_pending();
            dev_.reset();
            if (dcd_ops_.set_address) {
                dcd_ops_.set_address(dcd_ctx_, 0);
            }
            if (dcd_ops_.set_configured) {
                dcd_ops_.set_configured(dcd_ctx_, false);
            }
        }

    private:
        static Ep0Result ep0_send_in(void* ctx, std::span<const u8> data, bool zlp) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (!self || !self->dcd_ops_.ep.send) return Ep0Result::stall;
            const auto ok = self->dcd_ops_.ep.send(self->dcd_ctx_, 0x80, data, zlp);
            return ok ? Ep0Result::ok : Ep0Result::stall;
        }

        static Ep0Result ep0_send_zlp(void* ctx) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (!self || !self->dcd_ops_.ep.send) return Ep0Result::stall;
            const auto ok = self->dcd_ops_.ep.send(self->dcd_ctx_, 0x80, {}, true);
            return ok ? Ep0Result::ok : Ep0Result::stall;
        }

        static Ep0Result ep0_stall(void* ctx) noexcept {
            auto* self = static_cast<DeviceDriver*>(ctx);
            if (!self || !self->dcd_ops_.ep.stall) return Ep0Result::stall;
            self->dcd_ops_.ep.stall(self->dcd_ctx_, 0x00);
            self->dcd_ops_.ep.stall(self->dcd_ctx_, 0x80);
            return Ep0Result::stall;
        }

        void set_pending(const SetupPacket& setup) noexcept {
            if (request_type(setup.bm_request_type) != RequestType::standard) {
                return;
            }
            switch (static_cast<StandardRequest>(setup.b_request)) {
            case StandardRequest::set_address:
                pending_address_valid_ = true;
                pending_address_ = static_cast<u8>(setup.w_value & 0x7F);
                break;
            case StandardRequest::set_configuration:
                pending_config_valid_ = true;
                pending_configured_ = (static_cast<u8>(setup.w_value & 0xFF) != 0);
                break;
            default:
                break;
            }
        }

        void apply_pending_if_ready() noexcept {
            if (dev_.stage() != Ep0Stage::status_out) {
                return;
            }
            if (pending_address_valid_) {
                if (dcd_ops_.set_address) {
                    dcd_ops_.set_address(dcd_ctx_, pending_address_);
                }
                pending_address_valid_ = false;
            }
            if (pending_config_valid_) {
                if (dcd_ops_.set_configured) {
                    dcd_ops_.set_configured(dcd_ctx_, pending_configured_);
                }
                pending_config_valid_ = false;
            }
        }

        void clear_pending() noexcept {
            pending_address_valid_ = false;
            pending_config_valid_ = false;
        }

        Device& dev_;
        void* dcd_ctx_{nullptr};
        driver::DcdOps dcd_ops_{};
        Ep0Driver ep0_;
        bool pending_address_valid_{false};
        u8 pending_address_{0};
        bool pending_config_valid_{false};
        bool pending_configured_{false};
    };
}
