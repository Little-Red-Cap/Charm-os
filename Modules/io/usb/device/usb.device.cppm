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
        bool (*control_out)(void* ctx, const ControlRequest& req, ControlResponse& resp) noexcept { nullptr };
        bool (*get_status)(void* ctx, const SetupPacket& setup, ControlResponse& resp) noexcept { nullptr };
        bool (*clear_feature)(void* ctx, const SetupPacket& setup, ControlResponse& resp) noexcept { nullptr };
        bool (*set_feature)(void* ctx, const SetupPacket& setup, ControlResponse& resp) noexcept { nullptr };
        bool (*vendor_setup)(void* ctx, const ControlRequest& req, ControlResponse& resp) noexcept { nullptr };
        bool (*vendor_out)(void* ctx, const ControlRequest& req, ControlResponse& resp) noexcept { nullptr };
        void (*reset)(void* ctx) noexcept { nullptr };
    };

    class Ep0StateMachine {
    public:
        void reset() noexcept {
            state_ = DeviceState::default_state;
            stage_ = Ep0Stage::setup;
            in_remaining_ = 0;
            out_expected_ = 0;
            zlp_pending_ = false;
            address_ = 0;
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
            in_remaining_ = 0;
            out_expected_ = 0;
            zlp_pending_ = false;
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

        bool handle_out_data(std::span<const u8> data, ControlResponse& resp) noexcept {
            if (class_ops_ && class_ops_->control_out) {
                ControlRequest req{};
                req.setup = setup_;
                req.data = data;
                return class_ops_->control_out(class_ctx_, req, resp);
            }
            return false;
        }

        void begin_data_in(std::size_t len, bool zlp) noexcept {
            stage_ = Ep0Stage::data_in;
            in_remaining_ = static_cast<u16>(len);
            zlp_pending_ = zlp;
        }

        void begin_data_out(std::size_t len) noexcept {
            stage_ = Ep0Stage::data_out;
            out_expected_ = static_cast<u16>(len);
        }

        void finish_data_in() noexcept {
            stage_ = Ep0Stage::status_out;
            in_remaining_ = 0;
            zlp_pending_ = false;
        }

        void finish_data_out() noexcept {
            stage_ = Ep0Stage::status_in;
            out_expected_ = 0;
        }

        void mark_data_in_done() noexcept {
            if (zlp_pending_) return;
            finish_data_in();
        }

        void mark_zlp_sent() noexcept {
            if (!zlp_pending_) return;
            zlp_pending_ = false;
            finish_data_in();
        }

        void on_in_packet(std::size_t sent) noexcept {
            if (in_remaining_ > sent) {
                in_remaining_ = static_cast<u16>(in_remaining_ - sent);
                return;
            }
            in_remaining_ = 0;
            if (zlp_pending_) {
                return;
            }
            finish_data_in();
        }

        u16 in_remaining() const noexcept { return in_remaining_; }
        u16 out_expected() const noexcept { return out_expected_; }
        bool zlp_pending() const noexcept { return zlp_pending_; }

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
        u16 in_remaining_{0};
        u16 out_expected_{0};
        bool zlp_pending_{false};
    };

    class Device {
    public:
        void reset() noexcept {
            ep0_.reset();
            configuration_ = 0;
            interface_alt_ = 0;
            if (class_ops_ && class_ops_->reset) {
                class_ops_->reset(class_ctx_);
            }
        }
        void set_max_packet_size0(u16 mps) noexcept { max_packet_size0_ = mps; }
        void set_descriptor_provider(DescriptorProvider provider) noexcept { provider_ = provider; }
        void set_class(void* ctx, const ClassOps* ops) noexcept {
            class_ctx_ = ctx;
            class_ops_ = ops;
            ep0_.set_class(ctx, ops);
        }

        DeviceState state() const noexcept { return ep0_.state(); }
        Ep0Stage stage() const noexcept { return ep0_.stage(); }
        u16 in_remaining() const noexcept { return ep0_.in_remaining(); }
        u16 out_expected() const noexcept { return ep0_.out_expected(); }
        bool zlp_pending() const noexcept { return ep0_.zlp_pending(); }

        bool handle_setup(const SetupPacket& setup, ControlResponse& resp) noexcept {
            ep0_.on_setup(setup);
            setup_cache_ = setup;
            const auto req_type = request_type(setup.bm_request_type);
            if (req_type == RequestType::standard) {
                if (!handle_standard(setup, resp)) return false;
            }
            if (req_type == RequestType::class_request) {
                if (!ep0_.handle_setup(resp)) return false;
            }
            if (req_type == RequestType::vendor) {
                ControlRequest req{};
                req.setup = setup;
                req.data = {};
                if (!handle_vendor(req, resp)) return false;
            }
            if (setup.w_length == 0) {
                return true;
            }
            if (request_direction(setup.bm_request_type) == RequestDirection::in) {
                const auto wlen = static_cast<std::size_t>(setup.w_length);
                const auto len = (resp.data.size() < wlen) ? resp.data.size() : wlen;
                resp.data = resp.data.subspan(0, len);
                resp.zlp = (len < wlen) && (max_packet_size0_ > 0) && ((len % max_packet_size0_) == 0);
                ep0_.begin_data_in(len, resp.zlp);
                return true;
            }
            ep0_.begin_data_out(setup.w_length);
            return true;
        }

        bool handle_out_data(std::span<const u8> data, ControlResponse& resp) noexcept {
            const auto expected = ep0_.out_expected();
            const auto len = (data.size() < expected) ? data.size() : expected;
            if (!ep0_.handle_out_data(data.subspan(0, len), resp)) {
                ControlRequest req{};
                req.setup = setup_cache_;
                req.data = data.subspan(0, len);
                if (!handle_vendor(req, resp)) return false;
            }
            ep0_.finish_data_out();
            return true;
        }

        void finish_in_data(bool sent_zlp) noexcept {
            if (sent_zlp) {
                ep0_.mark_zlp_sent();
                return;
            }
            ep0_.on_in_packet(ep0_.in_remaining());
        }

        void on_in_packet(std::size_t sent, bool sent_zlp = false) noexcept {
            if (sent_zlp) {
                ep0_.mark_zlp_sent();
                return;
            }
            ep0_.on_in_packet(sent);
        }

    private:
        bool handle_vendor(const ControlRequest& req, ControlResponse& resp) noexcept {
            if (!class_ops_) return false;
            if (req.data.empty()) {
                return class_ops_->vendor_setup && class_ops_->vendor_setup(class_ctx_, req, resp);
            }
            return class_ops_->vendor_out && class_ops_->vendor_out(class_ctx_, req, resp);
        }

        bool handle_standard(const SetupPacket& setup, ControlResponse& resp) noexcept {
            switch (static_cast<StandardRequest>(setup.b_request)) {
            case StandardRequest::get_status: {
                if (class_ops_ && class_ops_->get_status) {
                    ControlResponse tmp{};
                    if (class_ops_->get_status(class_ctx_, setup, tmp)) {
                        resp = tmp;
                        return true;
                    }
                }
                status_buf_[0] = 0;
                status_buf_[1] = 0;
                resp.data = std::span<const u8>(status_buf_, 2);
                resp.zlp = false;
                return true;
            }
            case StandardRequest::clear_feature:
                if (class_ops_ && class_ops_->clear_feature) {
                    ControlResponse tmp{};
                    if (class_ops_->clear_feature(class_ctx_, setup, tmp)) {
                        resp = tmp;
                        return true;
                    }
                }
                resp.data = {};
                resp.zlp = true;
                return true;
            case StandardRequest::set_feature:
                if (class_ops_ && class_ops_->set_feature) {
                    ControlResponse tmp{};
                    if (class_ops_->set_feature(class_ctx_, setup, tmp)) {
                        resp = tmp;
                        return true;
                    }
                }
                resp.data = {};
                resp.zlp = true;
                return true;
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
            case StandardRequest::get_interface:
                resp.data = std::span<const u8>(&interface_alt_, 1);
                resp.zlp = false;
                return true;
            case StandardRequest::set_interface:
                interface_alt_ = static_cast<u8>(setup.w_value & 0xFF);
                resp.data = {};
                resp.zlp = true;
                return true;
            default:
                return false;
            }
        }

        Ep0StateMachine ep0_{};
        DescriptorProvider provider_{};
        void* class_ctx_{nullptr};
        const ClassOps* class_ops_{nullptr};
        SetupPacket setup_cache_{};
        u8 configuration_{0};
        u8 interface_alt_{0};
        u8 status_buf_[2]{0, 0};
        u16 max_packet_size0_{64};
    };
} // namespace usb::device
