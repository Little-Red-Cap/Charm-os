module;

#include <array>

export module net.reactor;

import io.channel;
import io.reactor;
import net.common;
import net.socket;
import util.core;
import util.error;
import util.expected;

export namespace net {
    struct SocketWatch {
        util::u32 id{0};

        constexpr explicit operator bool() const noexcept {
            return id != 0;
        }
    };

    [[nodiscard]] constexpr util::u32 default_socket_reactor_events() noexcept {
        return static_cast<util::u32>(io::Event::readable)
            | static_cast<util::u32>(io::Event::closed)
            | static_cast<util::u32>(io::Event::error);
    }

    [[nodiscard]] constexpr EventMask socket_interest_from_reactor_events(util::u32 events) noexcept {
        EventMask interest = 0;
        if ((events & static_cast<util::u32>(io::Event::readable)) != 0u) {
            interest |= event_mask(NetEvent::readable);
            interest |= event_mask(NetEvent::accepted);
        }
        if ((events & static_cast<util::u32>(io::Event::writable)) != 0u) {
            interest |= event_mask(NetEvent::writable);
        }
        if ((events & static_cast<util::u32>(io::Event::closed)) != 0u) {
            interest |= event_mask(NetEvent::closed);
        }
        if ((events & static_cast<util::u32>(io::Event::error)) != 0u) {
            interest |= event_mask(NetEvent::error);
        }
        return interest;
    }

    [[nodiscard]] constexpr util::u32 reactor_events_from_socket_mask(EventMask mask) noexcept {
        util::u32 events = 0;
        if (has_event(mask, NetEvent::readable) || has_event(mask, NetEvent::accepted)) {
            events |= static_cast<util::u32>(io::Event::readable);
        }
        if (has_event(mask, NetEvent::writable)) {
            events |= static_cast<util::u32>(io::Event::writable);
        }
        if (has_event(mask, NetEvent::closed)) {
            events |= static_cast<util::u32>(io::Event::closed);
        }
        if (has_event(mask, NetEvent::error)) {
            events |= static_cast<util::u32>(io::Event::error);
        }
        return events;
    }

    class SocketChannelBinding {
    public:
        SocketChannelBinding() noexcept
            : channel_{
                this,
                io::ChannelOps{
                    &SocketChannelBinding::read_trampoline,
                    &SocketChannelBinding::write_trampoline,
                    &SocketChannelBinding::flush_trampoline
                }
            } {}

        explicit SocketChannelBinding(Socket& socket) noexcept
            : SocketChannelBinding() {
            bind(socket);
        }

        SocketChannelBinding(const SocketChannelBinding&) = delete;
        SocketChannelBinding& operator=(const SocketChannelBinding&) = delete;
        SocketChannelBinding(SocketChannelBinding&&) = delete;
        SocketChannelBinding& operator=(SocketChannelBinding&&) = delete;

        void bind(Socket& socket) noexcept {
            socket_ = &socket;
        }

        void unbind() noexcept {
            socket_ = nullptr;
        }

        [[nodiscard]] bool bound() const noexcept {
            return socket_ != nullptr;
        }

        [[nodiscard]] Socket* socket() noexcept {
            return socket_;
        }

        [[nodiscard]] const Socket* socket() const noexcept {
            return socket_;
        }

        [[nodiscard]] io::Channel& channel() noexcept {
            return channel_;
        }

        [[nodiscard]] const io::Channel& channel() const noexcept {
            return channel_;
        }

    private:
        static io::result read_trampoline(void* ctx, io::MutByteView buf) noexcept {
            auto* self = static_cast<SocketChannelBinding*>(ctx);
            if (!self || !self->socket_ || buf.empty()) {
                return io::fail(io::errc::invalid_arg);
            }
            return self->socket_->recv(MutByteView{buf.data(), buf.size()});
        }

        static io::result write_trampoline(void* ctx, io::ByteView buf) noexcept {
            auto* self = static_cast<SocketChannelBinding*>(ctx);
            if (!self || !self->socket_ || buf.empty()) {
                return io::fail(io::errc::invalid_arg);
            }
            return self->socket_->send(ByteView{buf.data(), buf.size()});
        }

        static io::result flush_trampoline(void* ctx) noexcept {
            auto* self = static_cast<SocketChannelBinding*>(ctx);
            if (!self || !self->socket_) {
                return io::fail(io::errc::invalid_arg);
            }
            return io::ok(0);
        }

        Socket* socket_{nullptr};
        io::Channel channel_{};
    };

    class SocketEventChannelBinding {
    public:
        SocketEventChannelBinding() noexcept
            : channel_{
                this,
                io::ChannelOps{
                    &SocketEventChannelBinding::read_trampoline,
                    &SocketEventChannelBinding::write_trampoline,
                    &SocketEventChannelBinding::flush_trampoline
                }
            } {}

        explicit SocketEventChannelBinding(Socket& socket) noexcept
            : SocketEventChannelBinding() {
            bind(socket);
        }

        SocketEventChannelBinding(const SocketEventChannelBinding&) = delete;
        SocketEventChannelBinding& operator=(const SocketEventChannelBinding&) = delete;
        SocketEventChannelBinding(SocketEventChannelBinding&&) = delete;
        SocketEventChannelBinding& operator=(SocketEventChannelBinding&&) = delete;

        void bind(Socket& socket) noexcept {
            socket_ = &socket;
        }

        void unbind() noexcept {
            socket_ = nullptr;
        }

        [[nodiscard]] bool bound() const noexcept {
            return socket_ != nullptr;
        }

        [[nodiscard]] Socket* socket() noexcept {
            return socket_;
        }

        [[nodiscard]] const Socket* socket() const noexcept {
            return socket_;
        }

        [[nodiscard]] io::Channel& channel() noexcept {
            return channel_;
        }

        [[nodiscard]] const io::Channel& channel() const noexcept {
            return channel_;
        }

    private:
        static io::result read_trampoline(void*, io::MutByteView) noexcept {
            return io::fail(io::errc::not_supported);
        }

        static io::result write_trampoline(void*, io::ByteView) noexcept {
            return io::fail(io::errc::not_supported);
        }

        static io::result flush_trampoline(void*) noexcept {
            return io::ok(0);
        }

        Socket* socket_{nullptr};
        io::Channel channel_{};
    };

    template <util::usize MaxWatches>
    class SocketPoller {
    public:
        SocketPoller() noexcept = default;

        explicit SocketPoller(io::Reactor& reactor) noexcept
            : reactor_(&reactor) {}

        void bind(io::Reactor& reactor) noexcept {
            reactor_ = &reactor;
        }

        void unbind() noexcept {
            reactor_ = nullptr;
        }

        [[nodiscard]] bool ready() const noexcept {
            return reactor_ != nullptr;
        }

        [[nodiscard]] Result<SocketWatch> watch(Socket& socket,
                                                io::Channel& channel,
                                                util::u32 persistent_events = default_socket_reactor_events()) noexcept {
            for (const auto& slot : slots_) {
                if (!slot.used) continue;
                if (slot.socket == &socket || slot.channel == &channel) {
                    return util::unexpected(errc::exist);
                }
            }

            for (auto& slot : slots_) {
                if (slot.used) continue;
                slot.used = true;
                slot.id = next_id_++;
                if (next_id_ == 0) {
                    next_id_ = 1;
                }
                slot.socket = &socket;
                slot.channel = &channel;
                slot.persistent_interest = socket_interest_from_reactor_events(persistent_events);
                slot.one_shot_interest = 0;
                return SocketWatch{slot.id};
            }

            return util::unexpected(errc::busy);
        }

        void unwatch(SocketWatch watch_token) noexcept {
            auto* slot = slot_for(watch_token);
            if (!slot) return;
            *slot = {};
        }

        [[nodiscard]] Result<void> set_interest(SocketWatch watch_token, util::u32 persistent_events) noexcept {
            auto* slot = slot_for(watch_token);
            if (!slot) {
                return util::unexpected(errc::invalid_arg);
            }
            slot->persistent_interest = socket_interest_from_reactor_events(persistent_events);
            return {};
        }

        [[nodiscard]] Result<void> add_interest(SocketWatch watch_token, util::u32 persistent_events) noexcept {
            auto* slot = slot_for(watch_token);
            if (!slot) {
                return util::unexpected(errc::invalid_arg);
            }
            slot->persistent_interest |= socket_interest_from_reactor_events(persistent_events);
            return {};
        }

        [[nodiscard]] Result<void> remove_interest(SocketWatch watch_token, util::u32 persistent_events) noexcept {
            auto* slot = slot_for(watch_token);
            if (!slot) {
                return util::unexpected(errc::invalid_arg);
            }
            slot->persistent_interest &= ~socket_interest_from_reactor_events(persistent_events);
            return {};
        }

        [[nodiscard]] Result<void> arm(SocketWatch watch_token, util::u32 one_shot_events) noexcept {
            auto* slot = slot_for(watch_token);
            if (!slot) {
                return util::unexpected(errc::invalid_arg);
            }
            slot->one_shot_interest |= socket_interest_from_reactor_events(one_shot_events);
            return {};
        }

        [[nodiscard]] Result<void> disarm(SocketWatch watch_token, util::u32 one_shot_events) noexcept {
            auto* slot = slot_for(watch_token);
            if (!slot) {
                return util::unexpected(errc::invalid_arg);
            }
            slot->one_shot_interest &= ~socket_interest_from_reactor_events(one_shot_events);
            return {};
        }

        void clear() noexcept {
            for (auto& slot : slots_) {
                slot = {};
            }
        }

        [[nodiscard]] bool poll(util::usize budget = 0) noexcept {
            if (!reactor_) {
                return false;
            }
            if (budget == 0) {
                budget = static_cast<util::usize>(-1);
            }

            util::usize dispatched = 0;
            for (util::usize i = 0; i < slots_.size(); ++i) {
                auto& slot = slots_[i];
                if (!slot.used) continue;
                if (!slot.socket || !slot.channel || !slot.socket->valid()) {
                    slot = {};
                    continue;
                }

                const EventMask interest = slot.persistent_interest | slot.one_shot_interest;
                if (interest == 0) {
                    continue;
                }

                EventMask ready_mask = 0;
                auto ready = slot.socket->poll();
                if (!ready) {
                    ready_mask = event_mask(NetEvent::error) & interest;
                } else {
                    ready_mask = ready.value() & interest;
                }

                if (ready_mask == 0) {
                    continue;
                }

                reactor_->notify(*slot.channel, reactor_events_from_socket_mask(ready_mask));
                slot.one_shot_interest &= ~ready_mask;
                ++dispatched;

                if (dispatched >= budget) {
                    for (util::usize j = i + 1; j < slots_.size(); ++j) {
                        if (slots_[j].used) {
                            return true;
                        }
                    }
                    return false;
                }
            }
            return false;
        }

    private:
        struct Slot {
            bool used{false};
            util::u32 id{0};
            Socket* socket{nullptr};
            io::Channel* channel{nullptr};
            EventMask persistent_interest{0};
            EventMask one_shot_interest{0};
        };

        [[nodiscard]] Slot* slot_for(SocketWatch watch_token) noexcept {
            if (!watch_token) {
                return nullptr;
            }
            for (auto& slot : slots_) {
                if (slot.used && slot.id == watch_token.id) {
                    return &slot;
                }
            }
            return nullptr;
        }

        io::Reactor* reactor_{nullptr};
        std::array<Slot, MaxWatches> slots_{};
        util::u32 next_id_{1};
    };
}
