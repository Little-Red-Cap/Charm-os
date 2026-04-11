module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <span>
#include <vector>

export module usb.mock;

import usb.common;
import usb.driver;

export namespace usb::mock {
    struct InPacketView {
        usb::u8 ep{0};
        std::span<const usb::u8> data{};
        bool zlp{false};
    };

    enum class HostEventKind : usb::u8 {
        reset,
        connect,
        setup,
        out,
        in_ack,
    };

    struct HostEvent {
        HostEventKind kind{HostEventKind::reset};
        usb::SetupPacket setup{};
        usb::u8 ep{0};
        std::vector<usb::u8> data{};
        std::size_t size{0};
        bool flag{false};
    };

    enum class DeviceActionKind : usb::u8 {
        ep_open,
        ep_close,
        ep_send,
        ep_stall,
        set_address,
        set_configured,
        connect,
    };

    struct DeviceAction {
        DeviceActionKind kind{DeviceActionKind::ep_open};
        usb::driver::EpConfig ep_cfg{};
        usb::u8 ep{0};
        std::vector<usb::u8> data{};
        usb::u8 address{0};
        bool flag{false};
    };

    struct EndpointState {
        bool opened{false};
        bool stalled{false};
        usb::driver::EpConfig cfg{};
    };

    class Session {
    public:
        Session() noexcept {
            ops_.ep.open = &Session::ep_open_cb;
            ops_.ep.close = &Session::ep_close_cb;
            ops_.ep.send = &Session::ep_send_cb;
            ops_.ep.stall = &Session::ep_stall_cb;
            ops_.set_address = &Session::set_address_cb;
            ops_.set_configured = &Session::set_configured_cb;
            ops_.connect = &Session::connect_cb;
        }

        [[nodiscard]] const usb::driver::DcdOps& dcd_ops() const noexcept { return ops_; }
        [[nodiscard]] usb::driver::DcdDeviceAdapter& adapter() noexcept { return adapter_; }
        [[nodiscard]] const usb::driver::DcdDeviceAdapter& adapter() const noexcept { return adapter_; }

        void signal_reset() noexcept {
            pending_in_.clear();
            address_ = 0;
            configured_ = false;
            clear_stalls();
            ++reset_count_;
            record_host_event(HostEventKind::reset);
            adapter_.handle_reset();
        }

        void signal_connect(bool connected) noexcept {
            host_connected_ = connected;
            record_host_connect(connected);
            adapter_.handle_connect(connected);
        }

        void feed_setup(const usb::SetupPacket& setup) noexcept {
            record_host_setup(setup);
            adapter_.handle_setup(setup);
        }

        [[nodiscard]] bool feed_out(usb::u8 ep, std::span<const usb::u8> data) noexcept {
            if ((ep & 0x7Fu) == 0u) {
                record_host_out(ep, data);
                adapter_.handle_out_data(data);
                return true;
            }

            auto& slot = endpoint_slot_mut(ep);
            if (!slot.opened || !slot.callbacks.on_out) {
                return false;
            }
            record_host_out(ep, data);
            slot.callbacks.on_out(slot.callbacks.ctx, data);
            return true;
        }

        [[nodiscard]] std::optional<InPacketView> poll_in() const noexcept {
            if (pending_in_.empty()) {
                return std::nullopt;
            }

            const auto& transfer = pending_in_.front();
            const auto packet_len = current_packet_size(transfer);
            if (packet_len == 0) {
                if (!transfer.zlp_pending) {
                    return std::nullopt;
                }
                return InPacketView{transfer.ep, {}, true};
            }

            return InPacketView{
                transfer.ep,
                std::span<const usb::u8>(transfer.payload.data() + transfer.offset, packet_len),
                false,
            };
        }

        [[nodiscard]] bool ack_in(usb::u8 ep,
                                   std::size_t sent = invalid_size,
                                   bool sent_zlp = false) noexcept {
            if (pending_in_.empty()) {
                return false;
            }

            auto packet = poll_in();
            if (!packet || packet->ep != ep) {
                return false;
            }

            if (packet->zlp) {
                pending_in_.pop_front();
                record_host_in_ack(ep, 0, true);
                dispatch_in_complete(ep, 0, true);
                return true;
            }

            if (sent == invalid_size) {
                sent = packet->data.size();
            }
            if (sent_zlp || sent != packet->data.size()) {
                return false;
            }

            bool pop_transfer = false;
            {
                auto& transfer = pending_in_.front();
                transfer.offset += sent;
                pop_transfer = (transfer.offset >= transfer.payload.size()) && !transfer.zlp_pending;
            }

            if (pop_transfer) {
                pending_in_.pop_front();
            }

            record_host_in_ack(ep, sent, false);
            dispatch_in_complete(ep, sent, false);
            return true;
        }

        [[nodiscard]] bool has_pending_in() const noexcept { return !pending_in_.empty(); }
        [[nodiscard]] std::size_t pending_in_count() const noexcept { return pending_in_.size(); }
        [[nodiscard]] bool pullup_enabled() const noexcept { return pullup_enabled_; }
        [[nodiscard]] bool host_connected() const noexcept { return host_connected_; }
        [[nodiscard]] bool configured() const noexcept { return configured_; }
        [[nodiscard]] usb::u8 address() const noexcept { return address_; }
        [[nodiscard]] std::size_t reset_count() const noexcept { return reset_count_; }
        [[nodiscard]] std::size_t set_address_count() const noexcept { return set_address_count_; }
        [[nodiscard]] std::size_t set_configured_count() const noexcept { return set_configured_count_; }
        [[nodiscard]] std::span<const HostEvent> host_events() const noexcept {
            return std::span<const HostEvent>(host_events_.data(), host_events_.size());
        }
        [[nodiscard]] std::span<const DeviceAction> device_actions() const noexcept {
            return std::span<const DeviceAction>(device_actions_.data(), device_actions_.size());
        }

        void clear_trace() noexcept {
            host_events_.clear();
            device_actions_.clear();
        }

        [[nodiscard]] const EndpointState& endpoint_state(usb::u8 address) const noexcept {
            const auto index = endpoint_index(address);
            endpoint_views_[index] = endpoints_[index].view();
            return endpoint_views_[index];
        }

    private:
        struct EndpointSlotEx {
            bool opened{false};
            bool stalled{false};
            usb::driver::EpConfig cfg{};
            usb::driver::EpCallbacks callbacks{};

            [[nodiscard]] EndpointState view() const noexcept {
                EndpointState state{};
                state.opened = opened;
                state.stalled = stalled;
                state.cfg = cfg;
                return state;
            }
        };

        struct PendingInTransfer {
            usb::u8 ep{0x80};
            std::vector<usb::u8> payload{};
            std::size_t offset{0};
            bool zlp_pending{false};
        };

        static constexpr std::size_t invalid_size = (std::numeric_limits<std::size_t>::max)();

        [[nodiscard]] static constexpr std::size_t endpoint_index(usb::u8 address) noexcept {
            return static_cast<std::size_t>(address & 0x0Fu) + (((address & 0x80u) != 0u) ? 16u : 0u);
        }

        [[nodiscard]] EndpointSlotEx& endpoint_slot_mut(usb::u8 address) noexcept {
            return endpoints_[endpoint_index(address)];
        }

        [[nodiscard]] usb::u16 max_packet_size(usb::u8 address) const noexcept {
            if ((address & 0x7Fu) == 0u) {
                return ep0_max_packet_size_;
            }
            const auto& slot = endpoints_[endpoint_index(address)];
            return slot.opened ? slot.cfg.max_packet_size : ep0_max_packet_size_;
        }

        [[nodiscard]] std::size_t current_packet_size(const PendingInTransfer& transfer) const noexcept {
            if (transfer.offset < transfer.payload.size()) {
                const auto remaining = transfer.payload.size() - transfer.offset;
                const auto mps = static_cast<std::size_t>(max_packet_size(transfer.ep));
                return remaining < mps ? remaining : mps;
            }
            return transfer.zlp_pending ? 0u : 0u;
        }

        void dispatch_in_complete(usb::u8 ep, std::size_t sent, bool sent_zlp) noexcept {
            if ((ep & 0x7Fu) == 0u) {
                adapter_.handle_in_complete(sent, sent_zlp);
                return;
            }

            auto& slot = endpoint_slot_mut(ep);
            if (slot.callbacks.on_in_complete) {
                slot.callbacks.on_in_complete(slot.callbacks.ctx, sent, sent_zlp);
            }
        }

        void clear_stalls() noexcept {
            for (auto& slot : endpoints_) {
                slot.stalled = false;
            }
        }

        void record_host_event(HostEventKind kind) {
            HostEvent event{};
            event.kind = kind;
            host_events_.push_back(std::move(event));
        }

        void record_host_connect(bool connected) {
            HostEvent event{};
            event.kind = HostEventKind::connect;
            event.flag = connected;
            host_events_.push_back(std::move(event));
        }

        void record_host_setup(const usb::SetupPacket& setup) {
            HostEvent event{};
            event.kind = HostEventKind::setup;
            event.setup = setup;
            host_events_.push_back(std::move(event));
        }

        void record_host_out(usb::u8 ep, std::span<const usb::u8> data) {
            HostEvent event{};
            event.kind = HostEventKind::out;
            event.ep = ep;
            event.data.assign(data.begin(), data.end());
            event.size = data.size();
            host_events_.push_back(std::move(event));
        }

        void record_host_in_ack(usb::u8 ep, std::size_t sent, bool sent_zlp) {
            HostEvent event{};
            event.kind = HostEventKind::in_ack;
            event.ep = ep;
            event.size = sent;
            event.flag = sent_zlp;
            host_events_.push_back(std::move(event));
        }

        void record_device_ep_open(const usb::driver::EpConfig& cfg) {
            DeviceAction action{};
            action.kind = DeviceActionKind::ep_open;
            action.ep_cfg = cfg;
            action.ep = cfg.address;
            device_actions_.push_back(std::move(action));
        }

        void record_device_ep_close(usb::u8 ep) {
            DeviceAction action{};
            action.kind = DeviceActionKind::ep_close;
            action.ep = ep;
            device_actions_.push_back(std::move(action));
        }

        void record_device_ep_send(usb::u8 ep, std::span<const usb::u8> data, bool zlp) {
            DeviceAction action{};
            action.kind = DeviceActionKind::ep_send;
            action.ep = ep;
            action.data.assign(data.begin(), data.end());
            action.flag = zlp;
            device_actions_.push_back(std::move(action));
        }

        void record_device_ep_stall(usb::u8 ep) {
            DeviceAction action{};
            action.kind = DeviceActionKind::ep_stall;
            action.ep = ep;
            device_actions_.push_back(std::move(action));
        }

        void record_device_set_address(usb::u8 address) {
            DeviceAction action{};
            action.kind = DeviceActionKind::set_address;
            action.address = address;
            device_actions_.push_back(std::move(action));
        }

        void record_device_set_configured(bool configured) {
            DeviceAction action{};
            action.kind = DeviceActionKind::set_configured;
            action.flag = configured;
            device_actions_.push_back(std::move(action));
        }

        void record_device_connect(bool enable) {
            DeviceAction action{};
            action.kind = DeviceActionKind::connect;
            action.flag = enable;
            device_actions_.push_back(std::move(action));
        }

        static bool ep_open_cb(void* ctx,
                               const usb::driver::EpConfig& cfg,
                               usb::driver::EpCallbacks cb) noexcept {
            auto* self = static_cast<Session*>(ctx);
            if (!self) {
                return false;
            }
            auto& slot = self->endpoint_slot_mut(cfg.address);
            slot.opened = true;
            slot.stalled = false;
            slot.cfg = cfg;
            slot.callbacks = cb;
            self->record_device_ep_open(cfg);
            return true;
        }

        static bool ep_close_cb(void* ctx, usb::u8 address) noexcept {
            auto* self = static_cast<Session*>(ctx);
            if (!self) {
                return false;
            }
            self->endpoint_slot_mut(address) = EndpointSlotEx{};
            self->record_device_ep_close(address);
            return true;
        }

        static bool ep_send_cb(void* ctx,
                               usb::u8 address,
                               std::span<const usb::u8> data,
                               bool zlp) noexcept {
            auto* self = static_cast<Session*>(ctx);
            if (!self) {
                return false;
            }
            if ((address & 0x7Fu) != 0u) {
                const auto& slot = self->endpoint_slot_mut(address);
                if (!slot.opened) {
                    return false;
                }
            }

            PendingInTransfer transfer{};
            transfer.ep = address;
            transfer.payload.assign(data.begin(), data.end());
            transfer.zlp_pending = zlp;
            self->pending_in_.push_back(std::move(transfer));
            self->record_device_ep_send(address, data, zlp);
            return true;
        }

        static bool ep_stall_cb(void* ctx, usb::u8 address) noexcept {
            auto* self = static_cast<Session*>(ctx);
            if (!self) {
                return false;
            }
            auto& slot = self->endpoint_slot_mut(address);
            slot.stalled = true;
            if (slot.callbacks.on_stall) {
                slot.callbacks.on_stall(slot.callbacks.ctx);
            }
            self->record_device_ep_stall(address);
            return true;
        }

        static bool set_address_cb(void* ctx, usb::u8 address) noexcept {
            auto* self = static_cast<Session*>(ctx);
            if (!self) {
                return false;
            }
            self->address_ = address;
            ++self->set_address_count_;
            self->record_device_set_address(address);
            return true;
        }

        static bool set_configured_cb(void* ctx, bool configured) noexcept {
            auto* self = static_cast<Session*>(ctx);
            if (!self) {
                return false;
            }
            self->configured_ = configured;
            ++self->set_configured_count_;
            self->record_device_set_configured(configured);
            return true;
        }

        static bool connect_cb(void* ctx, bool enable) noexcept {
            auto* self = static_cast<Session*>(ctx);
            if (!self) {
                return false;
            }
            self->pullup_enabled_ = enable;
            self->record_device_connect(enable);
            return true;
        }

        usb::driver::DcdOps ops_{};
        usb::driver::DcdDeviceAdapter adapter_{};
        std::array<EndpointSlotEx, 32> endpoints_{};
        mutable std::array<EndpointState, 32> endpoint_views_{};
        std::deque<PendingInTransfer> pending_in_{};
        std::vector<HostEvent> host_events_{};
        std::vector<DeviceAction> device_actions_{};
        usb::u16 ep0_max_packet_size_{64};
        bool pullup_enabled_{false};
        bool host_connected_{false};
        bool configured_{false};
        usb::u8 address_{0};
        std::size_t reset_count_{0};
        std::size_t set_address_count_{0};
        std::size_t set_configured_count_{0};
    };
}
