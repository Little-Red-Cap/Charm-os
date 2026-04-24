module;

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>
#include <utility>

export module net.backend.win;

import net.common;
import net.socket;
import util.core;
import util.error;
import util.expected;

export namespace net::backend {
    template <util::usize MaxSockets = 16>
    class WinProvider {
        static_assert(MaxSockets >= 1);

    public:
        WinProvider() noexcept = default;

        WinProvider(const WinProvider&) = delete;
        WinProvider& operator=(const WinProvider&) = delete;

        Result<SocketHandle> open(SocketKind kind) noexcept {
            const auto started = ensure_started();
            if (!started) return util::unexpected(started.error());

            const int type = kind == SocketKind::tcp ? SOCK_STREAM : SOCK_DGRAM;
            const int proto = kind == SocketKind::tcp ? IPPROTO_TCP : IPPROTO_UDP;

            const SOCKET sock = ::socket(AF_INET, type, proto);
            if (sock == invalid_socket) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }

            if (!set_nonblocking(sock)) {
                const auto err = map_wsa_error(::WSAGetLastError());
                ::closesocket(sock);
                return util::unexpected(err);
            }

            const auto slot = allocate_slot();
            if (!slot) {
                ::closesocket(sock);
                return util::unexpected(slot.error());
            }

            auto& entry = slots_[slot.value()];
            entry.used = true;
            entry.sock = sock;
            entry.kind = kind;
            entry.state = SocketState::opened;
            return make_handle(slot.value());
        }

        Result<void> close(SocketHandle handle) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);

            if (slot->sock != invalid_socket) {
                (void)::closesocket(slot->sock);
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

            sockaddr_in addr{};
            auto st = endpoint_to_sockaddr(ep, addr, true);
            if (!st) return util::unexpected(st.error());

            BOOL reuse = 1;
            (void)::setsockopt(slot->sock, SOL_SOCKET, SO_REUSEADDR,
                               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

            const auto rc = ::bind(slot->sock,
                                   reinterpret_cast<const sockaddr*>(&addr),
                                   static_cast<int>(sizeof(addr)));
            if (rc == socket_error) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }

            auto local = query_local_endpoint(slot->sock);
            if (!local) return util::unexpected(local.error());
            slot->local = local.value();
            slot->state = SocketState::bound;
            return {};
        }

        Result<void> connect(SocketHandle handle, const Endpoint& ep) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->state != SocketState::opened && slot->state != SocketState::bound) {
                return util::unexpected(errc::bad_state);
            }

            sockaddr_in addr{};
            auto st = endpoint_to_sockaddr(ep, addr, false);
            if (!st) return util::unexpected(st.error());

            const auto rc = ::connect(slot->sock,
                                      reinterpret_cast<const sockaddr*>(&addr),
                                      static_cast<int>(sizeof(addr)));
            if (rc == socket_error) {
                const int wsa = ::WSAGetLastError();
                if (wsa != WSAEWOULDBLOCK && wsa != WSAEINPROGRESS && wsa != WSAEALREADY) {
                    return util::unexpected(map_wsa_error(wsa));
                }
            }

            auto local = query_local_endpoint(slot->sock);
            if (local) {
                slot->local = local.value();
            }
            slot->remote = ep;
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

            const int effective_backlog = backlog == 0 ? 1 : static_cast<int>(backlog);
            const auto rc = ::listen(slot->sock, effective_backlog);
            if (rc == socket_error) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }
            slot->state = SocketState::listening;
            return {};
        }

        Result<SocketHandle> accept(SocketHandle handle, Endpoint* peer) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->kind != SocketKind::tcp || slot->state != SocketState::listening) {
                return util::unexpected(errc::bad_state);
            }

            sockaddr_in addr{};
            int len = static_cast<int>(sizeof(addr));
            const SOCKET accepted = ::accept(slot->sock,
                                             reinterpret_cast<sockaddr*>(&addr),
                                             &len);
            if (accepted == invalid_socket) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }

            if (!set_nonblocking(accepted)) {
                const auto err = map_wsa_error(::WSAGetLastError());
                ::closesocket(accepted);
                return util::unexpected(err);
            }

            const auto child = allocate_slot();
            if (!child) {
                ::closesocket(accepted);
                return util::unexpected(child.error());
            }

            auto& entry = slots_[child.value()];
            entry.used = true;
            entry.sock = accepted;
            entry.kind = SocketKind::tcp;
            entry.state = SocketState::connected;
            entry.local = slot->local;
            const auto remote = sockaddr_to_endpoint(addr);
            if (peer) {
                *peer = remote;
            }
            entry.remote = remote;
            return make_handle(child.value());
        }

        IoResult send(SocketHandle handle, ByteView in) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (in.empty()) return util::unexpected(errc::invalid_arg);
            if (slot->state != SocketState::connected) return util::unexpected(errc::bad_state);

            const int sent = ::send(slot->sock,
                                    reinterpret_cast<const char*>(in.data()),
                                    static_cast<int>(in.size()),
                                    0);
            if (sent == socket_error) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }
            if (sent <= 0) return util::unexpected(errc::closed);
            return ok(static_cast<util::usize>(sent));
        }

        IoResult recv(SocketHandle handle, MutByteView out) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (out.empty()) return util::unexpected(errc::invalid_arg);

            const int received = ::recv(slot->sock,
                                        reinterpret_cast<char*>(out.data()),
                                        static_cast<int>(out.size()),
                                        0);
            if (received == socket_error) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }
            if (received == 0) return util::unexpected(errc::closed);
            return ok(static_cast<util::usize>(received));
        }

        IoResult send_to(SocketHandle handle, const Endpoint& peer, ByteView in) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->kind != SocketKind::udp) return util::unexpected(errc::not_supported);
            if (in.empty()) return util::unexpected(errc::invalid_arg);

            sockaddr_in addr{};
            auto st = endpoint_to_sockaddr(peer, addr, false);
            if (!st) return util::unexpected(st.error());

            const int sent = ::sendto(slot->sock,
                                      reinterpret_cast<const char*>(in.data()),
                                      static_cast<int>(in.size()),
                                      0,
                                      reinterpret_cast<const sockaddr*>(&addr),
                                      static_cast<int>(sizeof(addr)));
            if (sent == socket_error) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }
            if (sent <= 0) return util::unexpected(errc::closed);
            return ok(static_cast<util::usize>(sent));
        }

        IoResult recv_from(SocketHandle handle, Endpoint* peer, MutByteView out) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);
            if (slot->kind != SocketKind::udp) return util::unexpected(errc::not_supported);
            if (out.empty()) return util::unexpected(errc::invalid_arg);

            sockaddr_in addr{};
            int len = static_cast<int>(sizeof(addr));
            const int received = ::recvfrom(slot->sock,
                                            reinterpret_cast<char*>(out.data()),
                                            static_cast<int>(out.size()),
                                            0,
                                            reinterpret_cast<sockaddr*>(&addr),
                                            &len);
            if (received == socket_error) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }
            if (received == 0) return util::unexpected(errc::closed);
            if (peer) {
                *peer = sockaddr_to_endpoint(addr);
            }
            return ok(static_cast<util::usize>(received));
        }

        Result<EventMask> poll(SocketHandle handle) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);

            fd_set readfds{};
            fd_set writefds{};
            fd_set exceptfds{};
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            FD_ZERO(&exceptfds);
            FD_SET(slot->sock, &readfds);
            FD_SET(slot->sock, &writefds);
            FD_SET(slot->sock, &exceptfds);

            timeval timeout{};
            timeout.tv_sec = 0;
            timeout.tv_usec = 0;

            const int rc = ::select(0, &readfds, &writefds, &exceptfds, &timeout);
            if (rc == socket_error) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }

            EventMask mask = 0;
            if (FD_ISSET(slot->sock, &exceptfds)) {
                const auto err = socket_pending_error(slot->sock);
                if (err == errc::closed || err == errc::end_of_stream) {
                    mask |= event_mask(NetEvent::closed);
                } else if (err == errc::ok) {
                    mask |= event_mask(NetEvent::error);
                } else if (err != errc::would_block) {
                    mask |= event_mask(NetEvent::error);
                }
            }
            if (FD_ISSET(slot->sock, &writefds)) {
                mask |= event_mask(NetEvent::writable);
            }
            if (FD_ISSET(slot->sock, &readfds)) {
                if (slot->kind == SocketKind::tcp && slot->state == SocketState::listening) {
                    mask |= event_mask(NetEvent::accepted);
                    mask |= event_mask(NetEvent::readable);
                } else if (slot->kind == SocketKind::tcp) {
                    char probe{};
                    const int peeked = ::recv(slot->sock, &probe, 1, MSG_PEEK);
                    if (peeked == 0) {
                        mask |= event_mask(NetEvent::closed);
                    } else if (peeked == socket_error) {
                        const auto err = map_wsa_error(::WSAGetLastError());
                        if (err == errc::would_block) {
                            // no-op
                        } else if (err == errc::closed || err == errc::end_of_stream) {
                            mask |= event_mask(NetEvent::closed);
                        } else {
                            mask |= event_mask(NetEvent::error);
                        }
                    } else {
                        mask |= event_mask(NetEvent::readable);
                    }
                } else {
                    mask |= event_mask(NetEvent::readable);
                }
            }

            return Result<EventMask>{std::in_place, mask};
        }

        Result<void> shutdown(SocketHandle handle, ShutdownMode mode) noexcept {
            auto* slot = slot_for_handle(handle);
            if (!slot) return util::unexpected(errc::invalid_arg);

            int how = SD_BOTH;
            if (mode == ShutdownMode::read) how = SD_RECEIVE;
            if (mode == ShutdownMode::write) how = SD_SEND;

            const int rc = ::shutdown(slot->sock, how);
            if (rc == socket_error) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }
            return {};
        }

    private:
        static constexpr SOCKET invalid_socket = INVALID_SOCKET;
        static constexpr int socket_error = SOCKET_ERROR;

        struct SocketSlot {
            bool used{false};
            SOCKET sock{invalid_socket};
            SocketKind kind{SocketKind::tcp};
            SocketState state{SocketState::closed};
            Endpoint local{};
            Endpoint remote{};
        };

        [[nodiscard]] static Result<void> ensure_started() noexcept {
            if (startup_done_) {
                if (startup_error_ == errc::ok) return {};
                return util::unexpected(startup_error_);
            }

            WSADATA data{};
            const int rc = ::WSAStartup(MAKEWORD(2, 2), &data);
            startup_done_ = true;
            if (rc != 0) {
                startup_error_ = map_wsa_error(rc);
                return util::unexpected(startup_error_);
            }
            startup_error_ = errc::ok;
            return {};
        }

        [[nodiscard]] static errc map_wsa_error(int wsa) noexcept {
            switch (wsa) {
                case 0:
                    return errc::ok;
                case WSAEWOULDBLOCK:
                case WSAEINPROGRESS:
                case WSAEALREADY:
                    return errc::would_block;
                case WSAENOTSOCK:
                case WSAEINVAL:
                    return errc::invalid_arg;
                case WSAEMFILE:
                case WSAENOBUFS:
                    return errc::buffer_overflow;
                case WSAEADDRINUSE:
                    return errc::exist;
                case WSAEADDRNOTAVAIL:
                case WSAEDESTADDRREQ:
                case WSAEAFNOSUPPORT:
                    return errc::invalid_arg;
                case WSAECONNRESET:
                case WSAECONNABORTED:
                case WSAENETRESET:
                case WSAESHUTDOWN:
                case WSAENOTCONN:
                    return errc::closed;
                case WSAETIMEDOUT:
                    return errc::timeout;
                case WSAEOPNOTSUPP:
                case WSAEPROTONOSUPPORT:
                case WSAESOCKTNOSUPPORT:
                    return errc::not_supported;
                default:
                    return errc::io_error;
            }
        }

        [[nodiscard]] static bool set_nonblocking(SOCKET sock) noexcept {
            u_long mode = 1;
            return ::ioctlsocket(sock, FIONBIO, &mode) == 0;
        }

        [[nodiscard]] static errc socket_pending_error(SOCKET sock) noexcept {
            int so_error = 0;
            int len = static_cast<int>(sizeof(so_error));
            if (::getsockopt(sock,
                             SOL_SOCKET,
                             SO_ERROR,
                             reinterpret_cast<char*>(&so_error),
                             &len) == socket_error) {
                return map_wsa_error(::WSAGetLastError());
            }
            return map_wsa_error(so_error);
        }

        [[nodiscard]] static Result<void> endpoint_to_sockaddr(const Endpoint& ep,
                                                               sockaddr_in& out,
                                                               bool allow_any) noexcept {
            auto valid = allow_any ? validate_bind_endpoint_v0(ep) : validate_remote_endpoint_v0(ep);
            if (!valid) {
                return util::unexpected(valid.error());
            }

            out = sockaddr_in{};
            out.sin_family = AF_INET;
            out.sin_port = ::htons(ep.port);
            if (ep.address.is_any()) {
                out.sin_addr.s_addr = ::htonl(INADDR_ANY);
            } else {
                util::u32 ip = 0;
                ip |= static_cast<util::u32>(ep.address.bytes[0]) << 24;
                ip |= static_cast<util::u32>(ep.address.bytes[1]) << 16;
                ip |= static_cast<util::u32>(ep.address.bytes[2]) << 8;
                ip |= static_cast<util::u32>(ep.address.bytes[3]);
                out.sin_addr.s_addr = ::htonl(ip);
            }
            return {};
        }

        [[nodiscard]] static Endpoint sockaddr_to_endpoint(const sockaddr_in& in) noexcept {
            const util::u32 ip = ::ntohl(in.sin_addr.s_addr);
            return Endpoint::ipv4(static_cast<util::u8>((ip >> 24) & 0xFFu),
                                  static_cast<util::u8>((ip >> 16) & 0xFFu),
                                  static_cast<util::u8>((ip >> 8) & 0xFFu),
                                  static_cast<util::u8>(ip & 0xFFu),
                                  ::ntohs(in.sin_port));
        }

        [[nodiscard]] static Result<Endpoint> query_local_endpoint(SOCKET sock) noexcept {
            sockaddr_in addr{};
            int len = static_cast<int>(sizeof(addr));
            if (::getsockname(sock, reinterpret_cast<sockaddr*>(&addr), &len) == socket_error) {
                return util::unexpected(map_wsa_error(::WSAGetLastError()));
            }
            return Result<Endpoint>{std::in_place, sockaddr_to_endpoint(addr)};
        }

        [[nodiscard]] Result<util::usize> allocate_slot() noexcept {
            for (util::usize i = 0; i < slots_.size(); ++i) {
                if (slots_[i].used) continue;
                return Result<util::usize>{std::in_place, i};
            }
            return util::unexpected(errc::buffer_overflow);
        }

        [[nodiscard]] static constexpr SocketHandle make_handle(util::usize index) noexcept {
            return SocketHandle{static_cast<util::i32>(index) + 1};
        }

        [[nodiscard]] SocketSlot* slot_for_handle(SocketHandle handle) noexcept {
            if (!handle.valid()) return nullptr;
            const util::usize index = static_cast<util::usize>(handle.value - 1);
            if (index >= slots_.size() || !slots_[index].used) return nullptr;
            return &slots_[index];
        }

        std::array<SocketSlot, MaxSockets> slots_{};

        inline static bool startup_done_{false};
        inline static errc startup_error_{errc::ok};
    };
}

#ifndef NDEBUG
namespace net::backend {
    inline bool net_backend_win_self_check() noexcept {
        return true;
    }
}
#endif
