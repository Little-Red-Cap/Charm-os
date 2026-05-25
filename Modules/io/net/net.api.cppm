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

        [[nodiscard]] static Result<TcpClient> connected(const Stack& stack,
                                                         const Endpoint& remote) noexcept {
            TcpClient client{};
            auto ok = client.connect(stack, remote);
            if (!ok) return util::unexpected(ok.error());
            return Result<TcpClient>{std::in_place, std::move(client)};
        }

        [[nodiscard]] static Result<TcpClient> connected_loopback(const Stack& stack,
                                                                  util::u16 remote_port) noexcept {
            return connected(stack, Endpoint::ipv4_loopback(remote_port));
        }

        [[nodiscard]] Result<void> connect_loopback(const Stack& stack,
                                                    util::u16 remote_port) noexcept {
            return connect(stack, Endpoint::ipv4_loopback(remote_port));
        }

        [[nodiscard]] IoResult send(ByteView buf) noexcept {
            return socket_.send(buf);
        }

        template <util::usize Size>
        [[nodiscard]] IoResult send(const util::u8 (&buf)[Size]) noexcept {
            return send(ByteView{buf, Size});
        }

        [[nodiscard]] IoResult recv(MutByteView buf) noexcept {
            return socket_.recv(buf);
        }

        template <util::usize Size>
        [[nodiscard]] IoResult recv(util::u8 (&buf)[Size]) noexcept {
            return recv(MutByteView{buf, Size});
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

        [[nodiscard]] static Result<TcpListener> listening(const Stack& stack,
                                                           const Endpoint& local,
                                                           util::u16 backlog = 4) noexcept {
            TcpListener listener{};
            auto ok = listener.listen(stack, local, backlog);
            if (!ok) return util::unexpected(ok.error());
            return Result<TcpListener>{std::in_place, std::move(listener)};
        }

        [[nodiscard]] static Result<TcpListener> listening_any(const Stack& stack,
                                                               util::u16 local_port,
                                                               util::u16 backlog = 4) noexcept {
            return listening(stack, Endpoint::ipv4_any(local_port), backlog);
        }

        [[nodiscard]] static Result<TcpListener> listening_loopback(const Stack& stack,
                                                                    util::u16 local_port,
                                                                    util::u16 backlog = 4) noexcept {
            return listening(stack, Endpoint::ipv4_loopback(local_port), backlog);
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
            TcpClient accepted{};
            auto ok = accept(accepted, nullptr);
            if (!ok) return util::unexpected(ok.error());
            return Result<TcpClient>{std::in_place, std::move(accepted)};
        }

        [[nodiscard]] Result<TcpClient> accept(Endpoint& peer) noexcept {
            TcpClient accepted{};
            auto ok = accept(accepted, &peer);
            if (!ok) return util::unexpected(ok.error());
            return Result<TcpClient>{std::in_place, std::move(accepted)};
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

        [[nodiscard]] static Result<UdpSocket> bound(const Stack& stack,
                                                     const Endpoint& local) noexcept {
            UdpSocket socket{};
            auto ok = socket.bind(stack, local);
            if (!ok) return util::unexpected(ok.error());
            return Result<UdpSocket>{std::in_place, std::move(socket)};
        }

        [[nodiscard]] static Result<UdpSocket> bound_any(const Stack& stack,
                                                         util::u16 local_port) noexcept {
            return bound(stack, Endpoint::ipv4_any(local_port));
        }

        [[nodiscard]] static Result<UdpSocket> bound_loopback(const Stack& stack,
                                                              util::u16 local_port) noexcept {
            return bound(stack, Endpoint::ipv4_loopback(local_port));
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

        [[nodiscard]] static Result<UdpSocket> connected(const Stack& stack,
                                                         const Endpoint& remote) noexcept {
            UdpSocket socket{};
            auto ok = socket.connect(stack, remote);
            if (!ok) return util::unexpected(ok.error());
            return Result<UdpSocket>{std::in_place, std::move(socket)};
        }

        [[nodiscard]] static Result<UdpSocket> connected_loopback(const Stack& stack,
                                                                  util::u16 remote_port) noexcept {
            return connected(stack, Endpoint::ipv4_loopback(remote_port));
        }

        [[nodiscard]] Result<void> connect_loopback(const Stack& stack,
                                                    util::u16 remote_port) noexcept {
            return connect(stack, Endpoint::ipv4_loopback(remote_port));
        }

        [[nodiscard]] IoResult send(ByteView buf) noexcept {
            return socket_.send(buf);
        }

        template <util::usize Size>
        [[nodiscard]] IoResult send(const util::u8 (&buf)[Size]) noexcept {
            return send(ByteView{buf, Size});
        }

        [[nodiscard]] IoResult recv(MutByteView buf) noexcept {
            return socket_.recv(buf);
        }

        template <util::usize Size>
        [[nodiscard]] IoResult recv(util::u8 (&buf)[Size]) noexcept {
            return recv(MutByteView{buf, Size});
        }

        [[nodiscard]] IoResult send_to(const Endpoint& peer, ByteView buf) noexcept {
            return socket_.send_to(peer, buf);
        }

        template <util::usize Size>
        [[nodiscard]] IoResult send_to(const Endpoint& peer, const util::u8 (&buf)[Size]) noexcept {
            return send_to(peer, ByteView{buf, Size});
        }

        [[nodiscard]] IoResult recv_from(MutByteView buf, Endpoint& peer) noexcept {
            return socket_.recv_from(&peer, buf);
        }

        template <util::usize Size>
        [[nodiscard]] IoResult recv_from(util::u8 (&buf)[Size], Endpoint& peer) noexcept {
            return recv_from(MutByteView{buf, Size}, peer);
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
