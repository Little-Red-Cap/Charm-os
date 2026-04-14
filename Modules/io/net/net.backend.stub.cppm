module;

#include <array>
#include <cstddef>
#include <utility>

export module net.backend.stub;

import net.common;
import net.socket;
import util.core;
import util.error;
import util.expected;

export namespace net::backend {
    template <util::usize Capacity>
    struct StreamBuffer {
        static_assert(Capacity >= 1);

        std::array<util::u8, Capacity> bytes{};
        util::usize head{0};
        util::usize tail{0};
        util::usize size{0};

        constexpr void clear() noexcept {
            head = 0;
            tail = 0;
            size = 0;
        }

        [[nodiscard]] constexpr util::usize available_read() const noexcept {
            return size;
        }

        [[nodiscard]] constexpr util::usize available_write() const noexcept {
            return Capacity - size;
        }

        [[nodiscard]] util::usize write(ByteView in) noexcept {
            const util::usize count = in.size() < available_write() ? in.size() : available_write();
            for (util::usize i = 0; i < count; ++i) {
                bytes[tail] = in[i];
                tail = (tail + 1) % Capacity;
            }
            size += count;
            return count;
        }

        [[nodiscard]] util::usize read(MutByteView out) noexcept {
            const util::usize count = out.size() < size ? out.size() : size;
            for (util::usize i = 0; i < count; ++i) {
                out[i] = bytes[head];
                head = (head + 1) % Capacity;
            }
            size -= count;
            return count;
        }
    };

    template <util::usize MaxSockets = 16,
              util::usize TcpBufferCapacity = 512,
              util::usize UdpPayloadCapacity = 512,
              util::usize PendingAcceptCapacity = 4>
    class StubProvider {
        static_assert(MaxSockets >= 2);
        static_assert(TcpBufferCapacity >= 1);
        static_assert(UdpPayloadCapacity >= 1);
        static_assert(PendingAcceptCapacity >= 1);

    public:
        Result<SocketHandle> open(SocketKind kind) noexcept {
            const auto allocated = allocate_slot(kind, true);
            if (!allocated) return util::unexpected(allocated.error());
            return make_handle(allocated.value());
        }

        Result<void> close(SocketHandle handle) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);

            if (slot->kind == SocketKind::tcp && slot->state == SocketState::listening) {
                while (slot->pending_size > 0) {
                    auto child = pop_pending(*slot);
                    if (!child.valid()) continue;
                    auto* child_slot = slot_for_handle(child);
                    if (child_slot && child_slot->used) {
                        (void)close(child);
                    }
                }
            }

            if (slot->kind == SocketKind::tcp && slot->peer.valid()) {
                auto peer_handle = slot->peer;
                auto* peer_slot = slot_for_handle(peer_handle);
                if (peer_slot && peer_slot->used) {
                    peer_slot->peer = SocketHandle::invalid();
                    if (!peer_slot->published) {
                        (void)close(peer_handle);
                    }
                }
            }

            *slot = SocketSlot{};
            return {};
        }

        Result<void> bind(SocketHandle handle, const Endpoint& ep) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->state != SocketState::opened) {
                return util::unexpected(errc::bad_state);
            }
            auto valid = validate_bind_endpoint_v0(ep);
            if (!valid) {
                return util::unexpected(valid.error());
            }
            auto materialized = materialize_bind_endpoint(handle, slot->kind, ep);
            if (!materialized) {
                return util::unexpected(materialized.error());
            }
            if (endpoint_conflicts(handle, slot->kind, materialized.value())) {
                return util::unexpected(errc::exist);
            }
            slot->local = materialized.value();
            slot->state = SocketState::bound;
            return {};
        }

        Result<void> connect(SocketHandle handle, const Endpoint& ep) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->state != SocketState::opened && slot->state != SocketState::bound) {
                return util::unexpected(errc::bad_state);
            }
            auto valid = validate_remote_endpoint_v0(ep);
            if (!valid) {
                return util::unexpected(valid.error());
            }

            auto localized = ensure_local_endpoint(handle, *slot, ep);
            if (!localized) {
                return util::unexpected(localized.error());
            }

            if (slot->kind == SocketKind::udp) {
                slot->remote = ep;
                slot->state = SocketState::connected;
                return {};
            }

            auto* listener = find_tcp_listener(ep);
            if (!listener) return util::unexpected(errc::noent);

            discard_stale_pending(*listener);
            const util::usize backlog_limit = listener->backlog == 0
                ? PendingAcceptCapacity
                : (listener->backlog < PendingAcceptCapacity ? listener->backlog : PendingAcceptCapacity);
            if (listener->pending_size >= backlog_limit) {
                return util::unexpected(errc::would_block);
            }

            const auto accepted_idx = allocate_slot(SocketKind::tcp, false);
            if (!accepted_idx) return util::unexpected(accepted_idx.error());

            auto& child = sockets_[accepted_idx.value()];
            child.local = normalized_local_for_listener(*listener, ep);
            child.remote = slot->local;
            child.state = SocketState::connected;
            child.peer = handle;

            if (!push_pending(*listener, make_handle(accepted_idx.value()))) {
                child = SocketSlot{};
                return util::unexpected(errc::would_block);
            }

            slot->remote = ep;
            slot->peer = make_handle(accepted_idx.value());
            slot->state = SocketState::connected;
            return {};
        }

        Result<void> listen(SocketHandle handle, util::u16 backlog) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->kind != SocketKind::tcp) return util::unexpected(errc::not_supported);
            if (slot->state != SocketState::bound) {
                return util::unexpected(errc::bad_state);
            }
            if (slot->local.family() == AddressFamily::unspecified || slot->local.port == 0) {
                return util::unexpected(errc::invalid_arg);
            }
            slot->backlog = backlog == 0 ? 1 : backlog;
            slot->state = SocketState::listening;
            return {};
        }

        Result<SocketHandle> accept(SocketHandle handle, Endpoint* peer) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->kind != SocketKind::tcp || slot->state != SocketState::listening) {
                return util::unexpected(errc::bad_state);
            }

            discard_stale_pending(*slot);
            while (slot->pending_size > 0) {
                const auto child_handle = pop_pending(*slot);
                auto* child_slot = slot_for_handle(child_handle);
                if (!child_slot || !child_slot->used) continue;
                child_slot->published = true;
                if (peer) {
                    *peer = child_slot->remote;
                }
                return child_handle;
            }
            return util::unexpected(errc::would_block);
        }

        IoResult send(SocketHandle handle, ByteView in) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->write_shutdown) return util::unexpected(errc::closed);
            if (in.empty()) return util::unexpected(errc::invalid_arg);
            if (slot->state != SocketState::connected) return util::unexpected(errc::bad_state);

            if (slot->kind == SocketKind::udp) {
                if (slot->remote.port == 0) return util::unexpected(errc::bad_state);
                return send_to(handle, slot->remote, in);
            }

            if (slot->state != SocketState::connected || !slot->peer.valid()) {
                return util::unexpected(errc::bad_state);
            }
            auto* peer = slot_for_handle(slot->peer);
            if (!peer || !peer->used) return util::unexpected(errc::closed);
            const auto written = peer->stream_rx.write(in);
            if (written == 0) return util::unexpected(errc::would_block);
            return ok(written);
        }

        IoResult recv(SocketHandle handle, MutByteView out) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (out.empty()) return util::unexpected(errc::invalid_arg);
            if (slot->read_shutdown) return util::unexpected(errc::closed);

            if (slot->kind == SocketKind::udp) {
                return recv_from(handle, nullptr, out);
            }

            const auto read = slot->stream_rx.read(out);
            if (read != 0) return ok(read);
            if (!slot->peer.valid()) return util::unexpected(errc::closed);
            const auto* peer = slot_for_handle(slot->peer);
            if (!peer || !peer->used) return util::unexpected(errc::closed);
            return util::unexpected(errc::would_block);
        }

        IoResult send_to(SocketHandle handle, const Endpoint& peer, ByteView in) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->kind != SocketKind::udp) return util::unexpected(errc::not_supported);
            if (slot->write_shutdown) return util::unexpected(errc::closed);
            auto valid = validate_remote_endpoint_v0(peer);
            if (!valid) {
                return util::unexpected(valid.error());
            }
            if (in.empty()) {
                return util::unexpected(errc::invalid_arg);
            }

            auto localized = ensure_local_endpoint(handle, *slot, peer);
            if (!localized) {
                return util::unexpected(localized.error());
            }

            auto* target = find_udp_target(peer);
            if (!target) return util::unexpected(errc::noent);
            if (target->udp_rx.used) return util::unexpected(errc::would_block);

            const auto written = in.size() < UdpPayloadCapacity ? in.size() : UdpPayloadCapacity;
            for (util::usize i = 0; i < written; ++i) {
                target->udp_rx.payload[i] = in[i];
            }
            target->udp_rx.len = written;
            target->udp_rx.peer = slot->local;
            target->udp_rx.used = true;
            return ok(written);
        }

        IoResult recv_from(SocketHandle handle, Endpoint* peer, MutByteView out) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->kind != SocketKind::udp) return util::unexpected(errc::not_supported);
            if (slot->read_shutdown) return util::unexpected(errc::closed);
            if (out.empty()) return util::unexpected(errc::invalid_arg);
            if (!slot->udp_rx.used) return util::unexpected(errc::would_block);

            const auto read = out.size() < slot->udp_rx.len ? out.size() : slot->udp_rx.len;
            for (util::usize i = 0; i < read; ++i) {
                out[i] = slot->udp_rx.payload[i];
            }
            if (peer) {
                *peer = slot->udp_rx.peer;
            }
            slot->udp_rx = Datagram{};
            return ok(read);
        }

        Result<EventMask> poll(SocketHandle handle) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);

            EventMask mask = 0;
            if (slot->kind == SocketKind::tcp && slot->state == SocketState::listening) {
                discard_stale_pending(*slot);
                if (slot->pending_size != 0) {
                    mask |= event_mask(NetEvent::accepted);
                    mask |= event_mask(NetEvent::readable);
                }
                if (slot->pending_size < PendingAcceptCapacity) {
                    mask |= event_mask(NetEvent::writable);
                }
                return Result<EventMask>{std::in_place, mask};
            }

            if (slot->kind == SocketKind::udp) {
                if (slot->udp_rx.used) {
                    mask |= event_mask(NetEvent::readable);
                }
                if (!slot->write_shutdown) {
                    mask |= event_mask(NetEvent::writable);
                }
                return Result<EventMask>{std::in_place, mask};
            }

            if (slot->stream_rx.available_read() != 0) {
                mask |= event_mask(NetEvent::readable);
            }
            if (!slot->write_shutdown) {
                if (!slot->peer.valid()) {
                    if (slot->state == SocketState::connected) {
                        mask |= event_mask(NetEvent::closed);
                    } else if (slot->state != SocketState::closed) {
                        mask |= event_mask(NetEvent::writable);
                    }
                } else {
                    auto* peer = slot_for_handle(slot->peer);
                    if (!peer || !peer->used) {
                        mask |= event_mask(NetEvent::closed);
                    } else if (peer->stream_rx.available_write() != 0) {
                        mask |= event_mask(NetEvent::writable);
                    }
                }
            }

            return Result<EventMask>{std::in_place, mask};
        }

        Result<void> shutdown(SocketHandle handle, ShutdownMode mode) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (mode == ShutdownMode::read || mode == ShutdownMode::both) {
                slot->read_shutdown = true;
            }
            if (mode == ShutdownMode::write || mode == ShutdownMode::both) {
                slot->write_shutdown = true;
            }
            return {};
        }

    private:
        struct Datagram {
            bool used{false};
            Endpoint peer{};
            util::usize len{0};
            std::array<util::u8, UdpPayloadCapacity> payload{};
        };

        struct SocketSlot {
            bool used{false};
            bool published{false};
            bool read_shutdown{false};
            bool write_shutdown{false};
            SocketKind kind{SocketKind::tcp};
            SocketState state{SocketState::closed};
            Endpoint local{};
            Endpoint remote{};
            SocketHandle peer{};
            util::u16 backlog{0};
            StreamBuffer<TcpBufferCapacity> stream_rx{};
            Datagram udp_rx{};
            std::array<SocketHandle, PendingAcceptCapacity> pending{};
            util::usize pending_head{0};
            util::usize pending_tail{0};
            util::usize pending_size{0};
        };

        [[nodiscard]] static constexpr SocketHandle make_handle(util::usize index) noexcept {
            return SocketHandle{static_cast<util::i32>(index) + 1};
        }

        [[nodiscard]] static constexpr bool same_ip(const IpAddress& left, const IpAddress& right) noexcept {
            if (left.family != right.family) return false;
            const util::usize count = left.family == AddressFamily::ipv4 ? 4u : left.bytes.size();
            for (util::usize i = 0; i < count; ++i) {
                if (left.bytes[i] != right.bytes[i]) return false;
            }
            return true;
        }

        [[nodiscard]] static constexpr bool address_overlaps(const IpAddress& left, const IpAddress& right) noexcept {
            if (left.family != right.family) return false;
            return left.is_any() || right.is_any() || same_ip(left, right);
        }

        [[nodiscard]] static constexpr bool endpoint_matches(const Endpoint& bound, const Endpoint& target) noexcept {
            return bound.port == target.port
                && bound.family() == target.family()
                && address_overlaps(bound.address, target.address);
        }

        [[nodiscard]] SocketSlot* slot_for_handle(SocketHandle handle) noexcept {
            if (!handle.valid()) return nullptr;
            const util::usize index = static_cast<util::usize>(handle.value - 1);
            if (index >= sockets_.size()) return nullptr;
            if (!sockets_[index].used) return nullptr;
            return &sockets_[index];
        }

        [[nodiscard]] const SocketSlot* slot_for_handle(SocketHandle handle) const noexcept {
            if (!handle.valid()) return nullptr;
            const util::usize index = static_cast<util::usize>(handle.value - 1);
            if (index >= sockets_.size()) return nullptr;
            if (!sockets_[index].used) return nullptr;
            return &sockets_[index];
        }

        [[nodiscard]] Result<util::usize> allocate_slot(SocketKind kind, bool published) noexcept {
            for (util::usize i = 0; i < sockets_.size(); ++i) {
                if (sockets_[i].used) continue;
                sockets_[i] = SocketSlot{};
                sockets_[i].used = true;
                sockets_[i].published = published;
                sockets_[i].kind = kind;
                sockets_[i].state = SocketState::opened;
                return Result<util::usize>{std::in_place, i};
            }
            return util::unexpected(errc::buffer_overflow);
        }

        [[nodiscard]] bool endpoint_conflicts(SocketHandle current,
                                              SocketKind kind,
                                              const Endpoint& ep) const noexcept {
            if (ep.port == 0 || ep.family() == AddressFamily::unspecified) {
                return false;
            }
            for (util::usize i = 0; i < sockets_.size(); ++i) {
                if (!sockets_[i].used) continue;
                if (make_handle(i).value == current.value) continue;
                if (sockets_[i].kind != kind) continue;
                if (sockets_[i].local.port == 0) continue;
                if (endpoint_matches(sockets_[i].local, ep)) return true;
                if (endpoint_matches(ep, sockets_[i].local)) return true;
            }
            return false;
        }

        [[nodiscard]] Result<void> ensure_local_endpoint(SocketHandle current,
                                                         SocketSlot& slot,
                                                         const Endpoint& remote) noexcept {
            if (slot.local.family() == AddressFamily::unspecified) {
                slot.local.address = remote.family() == AddressFamily::ipv4
                    ? IpAddress::ipv4_loopback()
                    : IpAddress::ipv4_loopback();
            }
            if (slot.local.address.is_any()) {
                slot.local.address = remote.family() == AddressFamily::ipv4
                    ? IpAddress::ipv4_loopback()
                    : IpAddress::ipv4_loopback();
            }
            if (slot.local.port == 0) {
                const auto port = claim_ephemeral_port(current, slot.kind, slot.local.address);
                if (port == 0) {
                    return util::unexpected(errc::buffer_overflow);
                }
                slot.local.port = port;
            }
            if (slot.state == SocketState::opened) {
                slot.state = SocketState::bound;
            }
            return {};
        }

        [[nodiscard]] Result<Endpoint> materialize_bind_endpoint(SocketHandle current,
                                                                 SocketKind kind,
                                                                 const Endpoint& ep) noexcept {
            Endpoint bound = ep;
            if (bound.port != 0) {
                return Result<Endpoint>{std::in_place, bound};
            }
            const auto port = claim_ephemeral_port(current, kind, bound.address);
            if (port == 0) {
                return util::unexpected(errc::buffer_overflow);
            }
            bound.port = port;
            return Result<Endpoint>{std::in_place, bound};
        }

        void advance_ephemeral_port() noexcept {
            if (next_ephemeral_port_ < 49152u || next_ephemeral_port_ >= 65535u) {
                next_ephemeral_port_ = 49152u;
                return;
            }
            ++next_ephemeral_port_;
        }

        [[nodiscard]] util::u16 claim_ephemeral_port(SocketHandle current,
                                                     SocketKind kind,
                                                     const IpAddress& address) noexcept {
            if (next_ephemeral_port_ < 49152u || next_ephemeral_port_ > 65535u) {
                next_ephemeral_port_ = 49152u;
            }
            const util::u16 origin = next_ephemeral_port_;
            util::u16 candidate = origin;
            do {
                Endpoint probe{address, candidate};
                if (!endpoint_conflicts(current, kind, probe)) {
                    advance_ephemeral_port();
                    return candidate;
                }
                advance_ephemeral_port();
                candidate = next_ephemeral_port_;
            } while (candidate != origin);
            return 0;
        }

        [[nodiscard]] Endpoint normalized_local_for_listener(const SocketSlot& listener,
                                                             const Endpoint& remote) const noexcept {
            Endpoint local = listener.local;
            if (local.address.is_any() || local.family() == AddressFamily::unspecified) {
                local.address = remote.address;
            }
            return local;
        }

        [[nodiscard]] SocketSlot* find_tcp_listener(const Endpoint& ep) noexcept {
            for (auto& slot : sockets_) {
                if (!slot.used) continue;
                if (slot.kind != SocketKind::tcp) continue;
                if (slot.state != SocketState::listening) continue;
                if (endpoint_matches(slot.local, ep)) return &slot;
            }
            return nullptr;
        }

        [[nodiscard]] SocketSlot* find_udp_target(const Endpoint& ep) noexcept {
            for (auto& slot : sockets_) {
                if (!slot.used) continue;
                if (slot.kind != SocketKind::udp) continue;
                if (slot.state == SocketState::closed) continue;
                if (endpoint_matches(slot.local, ep)) return &slot;
            }
            return nullptr;
        }

        void discard_stale_pending(SocketSlot& listener) noexcept {
            while (listener.pending_size != 0) {
                const auto current = listener.pending[listener.pending_head];
                if (slot_for_handle(current)) {
                    return;
                }
                listener.pending[listener.pending_head] = SocketHandle::invalid();
                listener.pending_head = (listener.pending_head + 1) % PendingAcceptCapacity;
                --listener.pending_size;
            }
        }

        [[nodiscard]] bool push_pending(SocketSlot& listener, SocketHandle handle) noexcept {
            discard_stale_pending(listener);
            if (listener.pending_size >= PendingAcceptCapacity) {
                return false;
            }
            listener.pending[listener.pending_tail] = handle;
            listener.pending_tail = (listener.pending_tail + 1) % PendingAcceptCapacity;
            ++listener.pending_size;
            return true;
        }

        [[nodiscard]] SocketHandle pop_pending(SocketSlot& listener) noexcept {
            discard_stale_pending(listener);
            if (listener.pending_size == 0) {
                return SocketHandle::invalid();
            }
            const auto handle = listener.pending[listener.pending_head];
            listener.pending[listener.pending_head] = SocketHandle::invalid();
            listener.pending_head = (listener.pending_head + 1) % PendingAcceptCapacity;
            --listener.pending_size;
            return handle;
        }

        std::array<SocketSlot, MaxSockets> sockets_{};
        util::u16 next_ephemeral_port_{49152};
    };
}

#ifndef NDEBUG
namespace net::backend {
    inline bool net_backend_stub_self_check() noexcept {
        StubProvider<8, 64, 64, 4> provider{};
        auto ref = make_socket_provider_ref(provider);

        Socket listener{};
        Socket client{};
        Socket server{};
        Socket udp_a{};
        Socket udp_b{};
        Endpoint peer{};
        util::u8 rx[8]{};
        util::u8 tx[4]{1, 2, 3, 4};

        if (!listener.open(ref, SocketKind::tcp)) return false;
        if (!listener.bind(Endpoint::ipv4_any(7000))) return false;
        if (!listener.listen(2)) return false;

        if (!client.open(ref, SocketKind::tcp)) return false;
        if (!client.connect(Endpoint::ipv4_loopback(7000))) return false;

        auto listener_events = listener.poll();
        if (!listener_events) return false;
        if (!has_event(listener_events.value(), NetEvent::accepted)) return false;

        if (!listener.accept(server, &peer)) return false;
        if (peer.port == 0) return false;
        if (!client.send(ByteView{tx, 4})) return false;
        if (!server.recv(MutByteView{rx, 8})) return false;
        if (rx[0] != 1 || rx[3] != 4) return false;

        if (!udp_a.open(ref, SocketKind::udp)) return false;
        if (!udp_b.open(ref, SocketKind::udp)) return false;
        if (!udp_a.bind(Endpoint::ipv4_any(9001))) return false;
        if (!udp_b.bind(Endpoint::ipv4_any(9002))) return false;
        if (!udp_a.send_to(Endpoint::ipv4_loopback(9002), ByteView{tx, 4})) return false;
        if (!udp_b.recv_from(&peer, MutByteView{rx, 8})) return false;
        if (peer.port != 9001) return false;
        if (rx[0] != 1 || rx[3] != 4) return false;

        if (!server.close()) return false;
        if (!client.close()) return false;
        if (!listener.close()) return false;
        if (!udp_a.close()) return false;
        if (!udp_b.close()) return false;
        return true;
    }
}
#endif
