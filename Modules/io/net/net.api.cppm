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

        [[nodiscard]] Result<void> accept(TcpClient& out, Endpoint* peer = nullptr) noexcept {
            return socket_.accept(out.socket_, peer);
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

        [[nodiscard]] Result<void> connect(const Stack& stack, const Endpoint& remote) noexcept {
            if (!valid()) {
                auto opened = open(stack);
                if (!opened) return util::unexpected(opened.error());
            }
            return socket_.connect(remote);
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
        UdpSocket udp{};
        Endpoint peer{};
        util::u8 rx[4]{};
        util::u8 tx[3]{1, 2, 3};

        if (!stack.valid()) return false;
        if (!listener.listen(stack, Endpoint::ipv4_any(1883), 2)) return false;
        if (!listener.accept(client, &peer)) return false;
        if (!client.valid()) return false;
        if (peer.port != 5001) return false;
        if (!client.send(ByteView{tx, 3})) return false;
        if (!client.recv(MutByteView{rx, 4})) return false;
        if (rx[0] != 0x7E) return false;
        if (!client.close()) return false;
        if (!listener.close()) return false;

        if (!udp.bind(stack, Endpoint::ipv4_any(5000))) return false;
        if (!udp.send_to(Endpoint::ipv4(192, 168, 0, 10, 6000), ByteView{tx, 3})) return false;
        if (!udp.recv_from(MutByteView{rx, 4}, peer)) return false;
        if (peer.port != 6000) return false;
        if (!udp.close()) return false;
        return true;
    }
}
#endif
