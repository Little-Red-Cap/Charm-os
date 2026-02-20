module;

#include <cstddef>
#include <span>

export module usb.ep0_driver;

import usb.common;
import usb.device;

export namespace usb::device {
    enum class Ep0Result : u8 {
        ok,
        stall,
    };

    struct Ep0DriverOps {
        Ep0Result (*send_in)(void* ctx, std::span<const u8> data, bool zlp) noexcept { nullptr };
        Ep0Result (*send_zlp)(void* ctx) noexcept { nullptr };
        Ep0Result (*stall)(void* ctx) noexcept { nullptr };
    };

    class Ep0Driver {
    public:
        explicit Ep0Driver(Device& dev, void* ctx, Ep0DriverOps ops) noexcept
            : dev_(&dev), ctx_(ctx), ops_(ops) {}

        Ep0Result on_setup(const SetupPacket& setup, ControlResponse& resp) noexcept {
            if (!dev_) return Ep0Result::stall;
            if (!dev_->handle_setup(setup, resp)) {
                return stall();
            }
            if (setup.w_length == 0) {
                if (dev_->stage() == Ep0Stage::status_in) {
                    return send_zlp();
                }
                return Ep0Result::ok;
            }
            if (request_direction(setup.bm_request_type) == RequestDirection::in) {
                return send_in(resp.data, resp.zlp);
            }
            return Ep0Result::ok;
        }

        Ep0Result on_out_data(std::span<const u8> data, ControlResponse& resp) noexcept {
            if (!dev_) return Ep0Result::stall;
            if (!dev_->handle_out_data(data, resp)) {
                return stall();
            }
            if (dev_->stage() == Ep0Stage::status_out) {
                return Ep0Result::ok;
            }
            return send_zlp();
        }

        void on_in_complete(std::size_t sent, bool sent_zlp = false) noexcept {
            if (dev_) dev_->on_in_packet(sent, sent_zlp);
        }

    private:
        Ep0Result send_in(std::span<const u8> data, bool zlp) noexcept {
            if (!ops_.send_in) return Ep0Result::stall;
            return ops_.send_in(ctx_, data, zlp);
        }

        Ep0Result send_zlp() noexcept {
            if (ops_.send_zlp) return ops_.send_zlp(ctx_);
            return Ep0Result::ok;
        }

        Ep0Result stall() noexcept {
            if (ops_.stall) return ops_.stall(ctx_);
            return Ep0Result::stall;
        }

        Device* dev_{nullptr};
        void* ctx_{nullptr};
        Ep0DriverOps ops_{};
    };
}
