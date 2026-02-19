module;

#include <cstdint>
#include <span>

export module usb.device;

import usb.common;

export namespace usb::device {
    enum class DeviceState : u8 {
        default_state,
        addressed,
        configured,
    };

    enum class Ep0Stage : u8 {
        setup,
        data_in,
        data_out,
        status_in,
        status_out,
    };

    struct ControlRequest {
        SetupPacket setup{};
        std::span<const u8> data{};
    };

    struct ControlResponse {
        std::span<const u8> data{};
        bool zlp{false};
    };

    struct DescriptorProvider {
        void* ctx{nullptr};
        bool (*get_descriptor)(void* ctx, DescriptorType type, u8 index, std::span<const u8>& out) noexcept { nullptr };
    };

    struct ClassOps {
        bool (*setup)(void* ctx, const ControlRequest& req, ControlResponse& resp) noexcept { nullptr };
        void (*reset)(void* ctx) noexcept { nullptr };
    };

    class Ep0StateMachine {
    public:
        void reset() noexcept {
            state_ = DeviceState::default_state;
            stage_ = Ep0Stage::setup;
        }

        void set_class(void* ctx, const ClassOps* ops) noexcept {
            class_ctx_ = ctx;
            class_ops_ = ops;
        }

        DeviceState state() const noexcept { return state_; }
        Ep0Stage stage() const noexcept { return stage_; }

        void on_setup(const SetupPacket& setup) noexcept {
            setup_ = setup;
            stage_ = Ep0Stage::setup;
        }

        bool handle_setup(ControlResponse& resp) noexcept {
            if (class_ops_ && class_ops_->setup) {
                ControlRequest req{};
                req.setup = setup_;
                req.data = {};
                return class_ops_->setup(class_ctx_, req, resp);
            }
            return false;
        }

        void set_address(u8 addr) noexcept {
            address_ = addr;
            state_ = (addr == 0) ? DeviceState::default_state : DeviceState::addressed;
        }

        void set_configured(bool configured) noexcept {
            state_ = configured ? DeviceState::configured : DeviceState::addressed;
        }

    private:
        DeviceState state_{DeviceState::default_state};
        Ep0Stage stage_{Ep0Stage::setup};
        SetupPacket setup_{};
        u8 address_{0};
        void* class_ctx_{nullptr};
        const ClassOps* class_ops_{nullptr};
    };

    class Device {
    public:
        void set_descriptor_provider(DescriptorProvider provider) noexcept { provider_ = provider; }
        void set_class(void* ctx, const ClassOps* ops) noexcept { ep0_.set_class(ctx, ops); }

        DeviceState state() const noexcept { return ep0_.state(); }

        bool handle_setup(const SetupPacket& setup, ControlResponse& resp) noexcept {
            ep0_.on_setup(setup);
            const auto req_type = request_type(setup.bm_request_type);
            if (req_type == RequestType::standard) {
                return handle_standard(setup, resp);
            }
            if (req_type == RequestType::class_request) {
                return ep0_.handle_setup(resp);
            }
            return false;
        }

    private:
        bool handle_standard(const SetupPacket& setup, ControlResponse& resp) noexcept {
            switch (static_cast<StandardRequest>(setup.b_request)) {
            case StandardRequest::get_descriptor: {
                const auto type = static_cast<DescriptorType>(setup.w_value >> 8);
                const auto index = static_cast<u8>(setup.w_value & 0xFF);
                if (provider_.get_descriptor && provider_.get_descriptor(provider_.ctx, type, index, resp.data)) {
                    resp.zlp = false;
                    return true;
                }
                return false;
            }
            case StandardRequest::set_address:
                ep0_.set_address(static_cast<u8>(setup.w_value & 0x7F));
                resp.data = {};
                resp.zlp = true;
                return true;
            case StandardRequest::set_configuration:
                configuration_ = static_cast<u8>(setup.w_value & 0xFF);
                ep0_.set_configured(configuration_ != 0);
                resp.data = {};
                resp.zlp = true;
                return true;
            case StandardRequest::get_configuration:
                resp.data = std::span<const u8>(&configuration_, 1);
                resp.zlp = false;
                return true;
            default:
                return false;
            }
        }

        Ep0StateMachine ep0_{};
        DescriptorProvider provider_{};
        u8 configuration_{0};
    };
} // namespace usb::device
