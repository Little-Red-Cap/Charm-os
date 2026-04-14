module;

#include <utility>

export module net.api;

import net.common;
import net.socket;
import net.stack;
import util.core;
import util.error;
import util.expected;

export namespace net {
    using RawSocket = Socket;

    class TcpClient {
    public:
        TcpClient() noexcept = default;
        TcpClient(const TcpClient&) = delete;
        TcpClient& operator=(const TcpClient&) = delete;
        TcpClient(TcpClient&&) noexcept = default;
        TcpClient& operator=(TcpClient&&) noexcept = default;

        [[nodiscard]] bool valid() const noexcept {
            return socket_.valid();
        }

        [[nodiscard]] const Socket& raw() const noexcept {
            return socket_;
        }

        [[nodiscard]] Socket& raw() noexcept {
            return socket_;
        }

        [[nodiscard]] Result<void> connect(const Stack& stack, const Endpoint& remote) noexcept {
            if (valid()) return util::unexpected(errc::bad_state);
            if (!stack.valid()) return util::unexpected(errc::invalid_arg);
            auto opened = socket_.open(stack.provider(), SocketKind::tcp);
            if (!opened) return util::unexpected(opened.error());
            auto connected = socket_.connect(remote);
            if (!connected) {
                (void)socket_.close();
                return util::unexpected(connected.error());
            }
            return {};
        }

        [[nodiscard]] Result<void> connect_loopback(const Stack& stack,
                                                    util::u16 remote_port) noexcept {
            return connect(stack, Endpoint::ipv4_loopback(remote_port));
        }

        [[nodiscard]] IoResult send(ByteView buf) noexcept {
            return socket_.send(buf);
        }

        [[nodiscard]] IoResult recv(MutByteView buf) noexcept {
            return socket_.recv(buf);
        }

        [[nodiscard]] Result<EventMask> poll() const noexcept {
            return socket_.poll();
        }

        [[nodiscard]] Result<void> shutdown(ShutdownMode mode = ShutdownMode::both) noexcept {
            return socket_.shutdown(mode);
        }

        [[nodiscard]] Result<void> close() noexcept {
            return socket_.close();
        }

    private:
        friend class TcpListener;
        Socket socket_{};
    };

    class TcpListener {
    public:
        TcpListener() noexcept = default;
        TcpListener(const TcpListener&) = delete;
        TcpListener& operator=(const TcpListener&) = delete;
        TcpListener(TcpListener&&) noexcept = default;
        TcpListener& operator=(TcpListener&&) noexcept = default;

        [[nodiscard]] bool valid() const noexcept {
            return socket_.valid();
        }

        [[nodiscard]] const Socket& raw() const noexcept {
            return socket_;
        }

        [[nodiscard]] Socket& raw() noexcept {
            return socket_;
        }

        [[nodiscard]] Result<void> listen(const Stack& stack,
                                          const Endpoint& local,
                                          util::u16 backlog = 4) noexcept {
            if (valid()) return util::unexpected(errc::bad_state);
            if (!stack.valid()) return util::unexpected(errc::invalid_arg);
            auto opened = socket_.open(stack.provider(), SocketKind::tcp);
            if (!opened) return util::unexpected(opened.error());
            auto bound = socket_.bind(local);
            if (!bound) {
                (void)socket_.close();
                return util::unexpected(bound.error());
            }
            auto listening = socket_.listen(backlog);
            if (!listening) {
                (void)socket_.close();
                return util::unexpected(listening.error());
            }
            return {};
        }

        [[nodiscard]] Result<void> listen_any(const Stack& stack,
                                              util::u16 local_port,
                                              util::u16 backlog = 4) noexcept {
            return listen(stack, Endpoint::ipv4_any(local_port), backlog);
        }

        [[nodiscard]] Result<void> listen_loopback(const Stack& stack,
                                                   util::u16 local_port,
                                                   util::u16 backlog = 4) noexcept {
            return listen(stack, Endpoint::ipv4_loopback(local_port), backlog);
        }

        [[nodiscard]] Result<void> accept(TcpClient& out, Endpoint* peer = nullptr) noexcept {
            return socket_.accept(out.socket_, peer);
        }

        [[nodiscard]] Result<TcpClient> accept() noexcept {
            Result<TcpClient> accepted{std::in_place};
            auto ok = accept(accepted.value(), nullptr);
            if (!ok) return util::unexpected(ok.error());
            return accepted;
        }

        [[nodiscard]] Result<TcpClient> accept(Endpoint& peer) noexcept {
            Result<TcpClient> accepted{std::in_place};
            auto ok = accept(accepted.value(), &peer);
            if (!ok) return util::unexpected(ok.error());
            return accepted;
        }

        [[nodiscard]] Result<EventMask> poll() const noexcept {
            return socket_.poll();
        }

        [[nodiscard]] Result<void> close() noexcept {
            return socket_.close();
        }

    private:
        Socket socket_{};
    };

    class UdpSocket {
    public:
        UdpSocket() noexcept = default;
        UdpSocket(const UdpSocket&) = delete;
        UdpSocket& operator=(const UdpSocket&) = delete;
        UdpSocket(UdpSocket&&) noexcept = default;
        UdpSocket& operator=(UdpSocket&&) noexcept = default;

        [[nodiscard]] bool valid() const noexcept {
            return socket_.valid();
        }

        [[nodiscard]] const Socket& raw() const noexcept {
            return socket_;
        }

        [[nodiscard]] Socket& raw() noexcept {
            return socket_;
        }

        [[nodiscard]] Result<void> open(const Stack& stack) noexcept {
            if (valid()) return util::unexpected(errc::bad_state);
            if (!stack.valid()) return util::unexpected(errc::invalid_arg);
            return socket_.open(stack.provider(), SocketKind::udp);
        }

        [[nodiscard]] Result<void> bind(const Stack& stack, const Endpoint& local) noexcept {
            if (!valid()) {
                auto opened = open(stack);
                if (!opened) return util::unexpected(opened.error());
            }
            return socket_.bind(local);
        }

        [[nodiscard]] Result<void> bind_any(const Stack& stack, util::u16 local_port) noexcept {
            return bind(stack, Endpoint::ipv4_any(local_port));
        }

        [[nodiscard]] Result<void> bind_loopback(const Stack& stack,
                                                 util::u16 local_port) noexcept {
            return bind(stack, Endpoint::ipv4_loopback(local_port));
        }

        [[nodiscard]] Result<void> connect(const Stack& stack, const Endpoint& remote) noexcept {
            if (!valid()) {
                auto opened = open(stack);
                if (!opened) return util::unexpected(opened.error());
            }
            return socket_.connect(remote);
        }

        [[nodiscard]] Result<void> connect_loopback(const Stack& stack,
                                                    util::u16 remote_port) noexcept {
            return connect(stack, Endpoint::ipv4_loopback(remote_port));
        }

        [[nodiscard]] IoResult send(ByteView buf) noexcept {
            return socket_.send(buf);
        }

        [[nodiscard]] IoResult recv(MutByteView buf) noexcept {
            return socket_.recv(buf);
        }

        [[nodiscard]] IoResult send_to(const Endpoint& peer, ByteView buf) noexcept {
            return socket_.send_to(peer, buf);
        }

        [[nodiscard]] IoResult recv_from(MutByteView buf, Endpoint& peer) noexcept {
            return socket_.recv_from(&peer, buf);
        }

        [[nodiscard]] Result<EventMask> poll() const noexcept {
            return socket_.poll();
        }

        [[nodiscard]] Result<void> close() noexcept {
            return socket_.close();
        }

    private:
        Socket socket_{};
    };
}

#ifndef NDEBUG
namespace net {
    namespace detail {
        struct ApiDummyProvider {
            Result<SocketHandle> open(SocketKind kind) noexcept {
                return SocketHandle{kind == SocketKind::tcp ? 11 : 12};
            }

            Result<void> close(SocketHandle) noexcept {
                return {};
            }

            Result<void> bind(SocketHandle, const Endpoint&) noexcept {
                return {};
            }

            Result<void> connect(SocketHandle, const Endpoint&) noexcept {
                return {};
            }

            Result<void> listen(SocketHandle, util::u16) noexcept {
                return {};
            }

            Result<SocketHandle> accept(SocketHandle, Endpoint* peer) noexcept {
                if (peer) {
                    *peer = Endpoint::ipv4(10, 1, 1, 20, 5001);
                }
                return SocketHandle{13};
            }

            IoResult send(SocketHandle, ByteView in) noexcept {
                if (in.empty()) return util::unexpected(errc::invalid_arg);
                return ok(in.size());
            }

            IoResult recv(SocketHandle, MutByteView out) noexcept {
                if (out.empty()) return util::unexpected(errc::invalid_arg);
                out[0] = 0x7E;
                return ok(1);
            }

            IoResult send_to(SocketHandle, const Endpoint&, ByteView in) noexcept {
                if (in.empty()) return util::unexpected(errc::invalid_arg);
                return ok(in.size());
            }

            IoResult recv_from(SocketHandle, Endpoint* peer, MutByteView out) noexcept {
                if (peer) {
                    *peer = Endpoint::ipv4(10, 1, 1, 30, 6000);
                }
                if (out.empty()) return util::unexpected(errc::invalid_arg);
                out[0] = 0x33;
                return ok(1);
            }

            Result<EventMask> poll(SocketHandle) noexcept {
                return Result<EventMask>{std::in_place, event_mask(NetEvent::readable)};
            }

            Result<void> shutdown(SocketHandle, ShutdownMode) noexcept {
                return {};
            }
        };
    }

    inline bool net_api_self_check() noexcept {
        detail::ApiDummyProvider provider{};
        Stack stack{provider};
        TcpListener listener{};
        TcpClient client{};
        TcpListener listener_any{};
        TcpClient client_any{};
        UdpSocket udp{};
        UdpSocket udp_connected{};
        UdpSocket udp_loopback{};
        UdpSocket udp_connected_loopback{};
        Endpoint peer{};
        util::u8 rx[4]{};
        util::u8 tx[3]{1, 2, 3};

        if (!stack.valid()) return false;
        if (!listener.listen_loopback(stack, 1883, 2)) return false;
        if (!client.connect_loopback(stack, 1883)) return false;
        auto accepted_loopback = listener.accept(peer);
        if (!accepted_loopback) return false;
        if (!accepted_loopback->valid()) return false;
        if (peer.port != 5001) return false;
        if (!client.send(ByteView{tx, 3})) return false;
        if (!accepted_loopback->recv(MutByteView{rx, 4})) return false;
        if (rx[0] != 0x7E) return false;
        if (!client.close()) return false;
        if (!accepted_loopback->close()) return false;
        if (!listener.close()) return false;

        if (!listener_any.listen_any(stack, 1884, 2)) return false;
        if (!client_any.connect_loopback(stack, 1884)) return false;
        auto accepted_any = listener_any.accept();
        if (!accepted_any) return false;
        if (!accepted_any->valid()) return false;
        if (!client_any.close()) return false;
        if (!accepted_any->close()) return false;
        if (!listener_any.close()) return false;

        if (!udp.bind_any(stack, 5000)) return false;
        if (!udp_connected.connect_loopback(stack, 5000)) return false;
        if (!udp_connected.send(ByteView{tx, 3})) return false;
        if (!udp.recv(MutByteView{rx, 4})) return false;
        if (rx[0] != 0x7E) return false;
        if (!udp.close()) return false;
        if (!udp_connected.close()) return false;

        if (!udp_loopback.bind_loopback(stack, 5001)) return false;
        if (!udp_connected_loopback.connect_loopback(stack, 5001)) return false;
        if (!udp_connected_loopback.send(ByteView{tx, 3})) return false;
        if (!udp_loopback.recv(MutByteView{rx, 4})) return false;
        if (rx[0] != 0x7E) return false;
        if (!udp_loopback.close()) return false;
        if (!udp_connected_loopback.close()) return false;
        return true;
    }
}
#endif
