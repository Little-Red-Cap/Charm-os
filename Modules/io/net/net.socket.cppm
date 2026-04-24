module;

#include <concepts>
#include <utility>

export module net.socket;

import net.common;
import util.core;
import util.error;
import util.expected;

export namespace net {
    template <typename T>
    concept SocketProvider = requires(T& t,
                                      SocketHandle h,
                                      SocketKind kind,
                                      const Endpoint& ep,
                                      Endpoint* peer,
                                      ByteView in,
                                      MutByteView out,
                                      util::u16 backlog,
                                      ShutdownMode mode) {
        { t.open(kind) } noexcept -> std::same_as<Result<SocketHandle>>;
        { t.close(h) } noexcept -> std::same_as<Result<void>>;
        { t.bind(h, ep) } noexcept -> std::same_as<Result<void>>;
        { t.connect(h, ep) } noexcept -> std::same_as<Result<void>>;
        { t.listen(h, backlog) } noexcept -> std::same_as<Result<void>>;
        { t.accept(h, peer) } noexcept -> std::same_as<Result<SocketHandle>>;
        { t.send(h, in) } noexcept -> std::same_as<IoResult>;
        { t.recv(h, out) } noexcept -> std::same_as<IoResult>;
        { t.send_to(h, ep, in) } noexcept -> std::same_as<IoResult>;
        { t.recv_from(h, peer, out) } noexcept -> std::same_as<IoResult>;
        { t.poll(h) } noexcept -> std::same_as<Result<EventMask>>;
        { t.shutdown(h, mode) } noexcept -> std::same_as<Result<void>>;
    };

    struct SocketProviderOps {
        using OpenFn = Result<SocketHandle> (*)(void*, SocketKind) noexcept;
        using CloseFn = Result<void> (*)(void*, SocketHandle) noexcept;
        using BindFn = Result<void> (*)(void*, SocketHandle, const Endpoint&) noexcept;
        using ConnectFn = Result<void> (*)(void*, SocketHandle, const Endpoint&) noexcept;
        using ListenFn = Result<void> (*)(void*, SocketHandle, util::u16) noexcept;
        using AcceptFn = Result<SocketHandle> (*)(void*, SocketHandle, Endpoint*) noexcept;
        using SendFn = IoResult (*)(void*, SocketHandle, ByteView) noexcept;
        using RecvFn = IoResult (*)(void*, SocketHandle, MutByteView) noexcept;
        using SendToFn = IoResult (*)(void*, SocketHandle, const Endpoint&, ByteView) noexcept;
        using RecvFromFn = IoResult (*)(void*, SocketHandle, Endpoint*, MutByteView) noexcept;
        using PollFn = Result<EventMask> (*)(void*, SocketHandle) noexcept;
        using ShutdownFn = Result<void> (*)(void*, SocketHandle, ShutdownMode) noexcept;

        OpenFn open{nullptr};
        CloseFn close{nullptr};
        BindFn bind{nullptr};
        ConnectFn connect{nullptr};
        ListenFn listen{nullptr};
        AcceptFn accept{nullptr};
        SendFn send{nullptr};
        RecvFn recv{nullptr};
        SendToFn send_to{nullptr};
        RecvFromFn recv_from{nullptr};
        PollFn poll{nullptr};
        ShutdownFn shutdown{nullptr};
    };

    struct SocketProviderRef {
        void* self{nullptr};
        const SocketProviderOps* ops{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return self != nullptr && ops != nullptr;
        }

        [[nodiscard]] Result<SocketHandle> open(SocketKind kind) const noexcept {
            if (!valid() || !ops->open) return util::unexpected(errc::invalid_arg);
            return ops->open(self, kind);
        }

        [[nodiscard]] Result<void> close(SocketHandle handle) const noexcept {
            if (!valid() || !ops->close) return util::unexpected(errc::invalid_arg);
            return ops->close(self, handle);
        }

        [[nodiscard]] Result<void> bind(SocketHandle handle, const Endpoint& ep) const noexcept {
            if (!valid() || !ops->bind) return util::unexpected(errc::not_supported);
            return ops->bind(self, handle, ep);
        }

        [[nodiscard]] Result<void> connect(SocketHandle handle, const Endpoint& ep) const noexcept {
            if (!valid() || !ops->connect) return util::unexpected(errc::not_supported);
            return ops->connect(self, handle, ep);
        }

        [[nodiscard]] Result<void> listen(SocketHandle handle, util::u16 backlog) const noexcept {
            if (!valid() || !ops->listen) return util::unexpected(errc::not_supported);
            return ops->listen(self, handle, backlog);
        }

        [[nodiscard]] Result<SocketHandle> accept(SocketHandle handle, Endpoint* peer) const noexcept {
            if (!valid() || !ops->accept) return util::unexpected(errc::not_supported);
            return ops->accept(self, handle, peer);
        }

        [[nodiscard]] IoResult send(SocketHandle handle, ByteView buf) const noexcept {
            if (!valid() || !ops->send) return util::unexpected(errc::not_supported);
            return ops->send(self, handle, buf);
        }

        [[nodiscard]] IoResult recv(SocketHandle handle, MutByteView buf) const noexcept {
            if (!valid() || !ops->recv) return util::unexpected(errc::not_supported);
            return ops->recv(self, handle, buf);
        }

        [[nodiscard]] IoResult send_to(SocketHandle handle, const Endpoint& peer, ByteView buf) const noexcept {
            if (!valid() || !ops->send_to) return util::unexpected(errc::not_supported);
            return ops->send_to(self, handle, peer, buf);
        }

        [[nodiscard]] IoResult recv_from(SocketHandle handle, Endpoint* peer, MutByteView buf) const noexcept {
            if (!valid() || !ops->recv_from) return util::unexpected(errc::not_supported);
            return ops->recv_from(self, handle, peer, buf);
        }

        [[nodiscard]] Result<EventMask> poll(SocketHandle handle) const noexcept {
            if (!valid() || !ops->poll) return util::unexpected(errc::not_supported);
            return ops->poll(self, handle);
        }

        [[nodiscard]] Result<void> shutdown(SocketHandle handle, ShutdownMode mode) const noexcept {
            if (!valid() || !ops->shutdown) return util::unexpected(errc::not_supported);
            return ops->shutdown(self, handle, mode);
        }
    };

    template <SocketProvider T>
    inline const SocketProviderOps* socket_provider_ops() noexcept {
        static const SocketProviderOps ops{
            .open = [](void* self, SocketKind kind) noexcept {
                return static_cast<T*>(self)->open(kind);
            },
            .close = [](void* self, SocketHandle handle) noexcept {
                return static_cast<T*>(self)->close(handle);
            },
            .bind = [](void* self, SocketHandle handle, const Endpoint& ep) noexcept {
                return static_cast<T*>(self)->bind(handle, ep);
            },
            .connect = [](void* self, SocketHandle handle, const Endpoint& ep) noexcept {
                return static_cast<T*>(self)->connect(handle, ep);
            },
            .listen = [](void* self, SocketHandle handle, util::u16 backlog) noexcept {
                return static_cast<T*>(self)->listen(handle, backlog);
            },
            .accept = [](void* self, SocketHandle handle, Endpoint* peer) noexcept {
                return static_cast<T*>(self)->accept(handle, peer);
            },
            .send = [](void* self, SocketHandle handle, ByteView buf) noexcept {
                return static_cast<T*>(self)->send(handle, buf);
            },
            .recv = [](void* self, SocketHandle handle, MutByteView buf) noexcept {
                return static_cast<T*>(self)->recv(handle, buf);
            },
            .send_to = [](void* self, SocketHandle handle, const Endpoint& peer, ByteView buf) noexcept {
                return static_cast<T*>(self)->send_to(handle, peer, buf);
            },
            .recv_from = [](void* self, SocketHandle handle, Endpoint* peer, MutByteView buf) noexcept {
                return static_cast<T*>(self)->recv_from(handle, peer, buf);
            },
            .poll = [](void* self, SocketHandle handle) noexcept {
                return static_cast<T*>(self)->poll(handle);
            },
            .shutdown = [](void* self, SocketHandle handle, ShutdownMode mode) noexcept {
                return static_cast<T*>(self)->shutdown(handle, mode);
            }
        };
        return &ops;
    }

    template <SocketProvider T>
    inline SocketProviderRef make_socket_provider_ref(T& provider) noexcept {
        return SocketProviderRef{&provider, socket_provider_ops<T>()};
    }

    class Socket {
    public:
        Socket() noexcept = default;
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        Socket(Socket&& other) noexcept
            : provider_(other.provider_),
              handle_(other.handle_),
              kind_(other.kind_),
              state_(other.state_) {
            other.clear();
        }

        Socket& operator=(Socket&& other) noexcept {
            if (this == &other) return *this;
            if (valid()) util::halt();
            provider_ = other.provider_;
            handle_ = other.handle_;
            kind_ = other.kind_;
            state_ = other.state_;
            other.clear();
            return *this;
        }

        [[nodiscard]] constexpr bool valid() const noexcept {
            return provider_.valid() && handle_.valid();
        }

        [[nodiscard]] constexpr SocketKind kind() const noexcept {
            return kind_;
        }

        [[nodiscard]] constexpr SocketState state() const noexcept {
            return state_;
        }

        [[nodiscard]] constexpr SocketHandle handle() const noexcept {
            return handle_;
        }

        [[nodiscard]] constexpr SocketProviderRef provider() const noexcept {
            return provider_;
        }

        [[nodiscard]] Result<void> open(SocketProviderRef provider, SocketKind kind) noexcept {
            if (valid()) return util::unexpected(errc::bad_state);
            auto opened = provider.open(kind);
            if (!opened) return util::unexpected(opened.error());
            provider_ = provider;
            handle_ = opened.value();
            kind_ = kind;
            state_ = SocketState::opened;
            return {};
        }

        [[nodiscard]] Result<void> attach(SocketProviderRef provider,
                                          SocketHandle handle,
                                          SocketKind kind,
                                          SocketState state = SocketState::opened) noexcept {
            if (valid()) return util::unexpected(errc::bad_state);
            if (!provider.valid() || !handle.valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            provider_ = provider;
            handle_ = handle;
            kind_ = kind;
            state_ = state;
            return {};
        }

        [[nodiscard]] Result<void> bind(const Endpoint& local) noexcept {
            if (!valid()) return util::unexpected(errc::bad_state);
            if (state_ != SocketState::opened) {
                return util::unexpected(errc::bad_state);
            }
            auto bound = provider_.bind(handle_, local);
            if (!bound) return util::unexpected(bound.error());
            state_ = SocketState::bound;
            return {};
        }

        [[nodiscard]] Result<void> connect(const Endpoint& remote) noexcept {
            if (!valid()) return util::unexpected(errc::bad_state);
            if (state_ != SocketState::opened && state_ != SocketState::bound) {
                return util::unexpected(errc::bad_state);
            }
            auto connected = provider_.connect(handle_, remote);
            if (!connected) return util::unexpected(connected.error());
            state_ = SocketState::connected;
            return {};
        }

        [[nodiscard]] Result<void> listen(util::u16 backlog = 4) noexcept {
            if (!valid()) return util::unexpected(errc::bad_state);
            if (kind_ != SocketKind::tcp) return util::unexpected(errc::not_supported);
            if (state_ != SocketState::bound) {
                return util::unexpected(errc::bad_state);
            }
            auto listening = provider_.listen(handle_, backlog);
            if (!listening) return util::unexpected(listening.error());
            state_ = SocketState::listening;
            return {};
        }

        [[nodiscard]] Result<void> accept(Socket& out, Endpoint* peer = nullptr) noexcept {
            if (!valid()) return util::unexpected(errc::bad_state);
            if (kind_ != SocketKind::tcp || state_ != SocketState::listening) {
                return util::unexpected(errc::bad_state);
            }
            if (out.valid()) return util::unexpected(errc::bad_state);
            auto accepted = provider_.accept(handle_, peer);
            if (!accepted) return util::unexpected(accepted.error());
            return out.attach(provider_, accepted.value(), SocketKind::tcp, SocketState::connected);
        }

        [[nodiscard]] Result<Socket> accept() noexcept {
            Socket accepted{};
            auto ok = accept(accepted, nullptr);
            if (!ok) return util::unexpected(ok.error());
            return Result<Socket>{std::in_place, std::move(accepted)};
        }

        [[nodiscard]] Result<Socket> accept(Endpoint& peer) noexcept {
            Socket accepted{};
            auto ok = accept(accepted, &peer);
            if (!ok) return util::unexpected(ok.error());
            return Result<Socket>{std::in_place, std::move(accepted)};
        }

        [[nodiscard]] IoResult send(ByteView buf) noexcept {
            if (!valid()) return util::unexpected(errc::bad_state);
            if (buf.empty()) return util::unexpected(errc::invalid_arg);
            if (state_ != SocketState::connected) return util::unexpected(errc::bad_state);
            auto r = provider_.send(handle_, buf);
            if (r && r.value() == 0u) util::halt();
            return r;
        }

        [[nodiscard]] IoResult recv(MutByteView buf) noexcept {
            if (!valid()) return util::unexpected(errc::bad_state);
            if (buf.empty()) return util::unexpected(errc::invalid_arg);
            if (kind_ == SocketKind::tcp && state_ != SocketState::connected) {
                return util::unexpected(errc::bad_state);
            }
            auto r = provider_.recv(handle_, buf);
            if (r && r.value() == 0u) util::halt();
            return r;
        }

        [[nodiscard]] IoResult send_to(const Endpoint& peer, ByteView buf) noexcept {
            if (!valid()) return util::unexpected(errc::bad_state);
            if (buf.empty()) return util::unexpected(errc::invalid_arg);
            if (kind_ != SocketKind::udp) return util::unexpected(errc::not_supported);
            auto r = provider_.send_to(handle_, peer, buf);
            if (r && r.value() == 0u) util::halt();
            return r;
        }

        [[nodiscard]] IoResult recv_from(Endpoint* peer, MutByteView buf) noexcept {
            if (!valid()) return util::unexpected(errc::bad_state);
            if (buf.empty()) return util::unexpected(errc::invalid_arg);
            if (kind_ != SocketKind::udp) return util::unexpected(errc::not_supported);
            auto r = provider_.recv_from(handle_, peer, buf);
            if (r && r.value() == 0u) util::halt();
            return r;
        }

        [[nodiscard]] Result<EventMask> poll() const noexcept {
            if (!valid()) return util::unexpected(errc::bad_state);
            return provider_.poll(handle_);
        }

        [[nodiscard]] Result<void> shutdown(ShutdownMode mode = ShutdownMode::both) noexcept {
            if (!valid()) return util::unexpected(errc::bad_state);
            return provider_.shutdown(handle_, mode);
        }

        [[nodiscard]] Result<void> close() noexcept {
            if (!valid()) {
                clear();
                return {};
            }
            auto closed = provider_.close(handle_);
            if (!closed) return util::unexpected(closed.error());
            clear();
            return {};
        }

    private:
        constexpr void clear() noexcept {
            provider_ = {};
            handle_ = SocketHandle::invalid();
            kind_ = SocketKind::tcp;
            state_ = SocketState::closed;
        }

        SocketProviderRef provider_{};
        SocketHandle handle_{};
        SocketKind kind_{SocketKind::tcp};
        SocketState state_{SocketState::closed};
    };
}

#ifndef NDEBUG
namespace net {
    namespace detail {
        struct DummySocketProvider {
            SocketKind last_kind{SocketKind::tcp};
            Endpoint last_bind{};
            Endpoint last_connect{};
            Endpoint last_peer{};
            util::u16 last_backlog{0};
            util::u32 close_count{0};

            Result<SocketHandle> open(SocketKind kind) noexcept {
                last_kind = kind;
                return SocketHandle{kind == SocketKind::tcp ? 1 : 2};
            }

            Result<void> close(SocketHandle) noexcept {
                ++close_count;
                return {};
            }

            Result<void> bind(SocketHandle, const Endpoint& ep) noexcept {
                last_bind = ep;
                return {};
            }

            Result<void> connect(SocketHandle, const Endpoint& ep) noexcept {
                last_connect = ep;
                return {};
            }

            Result<void> listen(SocketHandle, util::u16 backlog) noexcept {
                last_backlog = backlog;
                return {};
            }

            Result<SocketHandle> accept(SocketHandle, Endpoint* peer) noexcept {
                last_peer = Endpoint::ipv4(10, 0, 0, 2, 1883);
                if (peer) {
                    *peer = last_peer;
                }
                return SocketHandle{3};
            }

            IoResult send(SocketHandle, ByteView in) noexcept {
                if (in.empty()) return util::unexpected(errc::invalid_arg);
                return ok(in.size());
            }

            IoResult recv(SocketHandle, MutByteView out) noexcept {
                if (out.empty()) return util::unexpected(errc::invalid_arg);
                out[0] = 0x2A;
                return ok(1);
            }

            IoResult send_to(SocketHandle, const Endpoint& peer, ByteView in) noexcept {
                last_peer = peer;
                if (in.empty()) return util::unexpected(errc::invalid_arg);
                return ok(in.size());
            }

            IoResult recv_from(SocketHandle, Endpoint* peer, MutByteView out) noexcept {
                last_peer = Endpoint::ipv4(10, 0, 0, 3, 9000);
                if (peer) {
                    *peer = last_peer;
                }
                if (out.empty()) return util::unexpected(errc::invalid_arg);
                out[0] = 0x55;
                return ok(1);
            }

            Result<EventMask> poll(SocketHandle) noexcept {
                return Result<EventMask>{std::in_place, NetEvent::readable | NetEvent::writable};
            }

            Result<void> shutdown(SocketHandle, ShutdownMode) noexcept {
                return {};
            }
        };
    }

    inline bool net_socket_self_check() noexcept {
        detail::DummySocketProvider provider{};
        auto ref = make_socket_provider_ref(provider);
        Socket listener{};
        Socket udp{};
        util::u8 rx[4]{};
        util::u8 tx[4]{1, 2, 3, 4};
        Endpoint peer{};

        if (!listener.open(ref, SocketKind::tcp)) return false;
        if (!listener.bind(Endpoint::ipv4_any(8080))) return false;
        if (!listener.listen(2)) return false;
        auto accepted = listener.accept(peer);
        if (!accepted) return false;
        if (!accepted->valid()) return false;
        if (peer.port != 1883) return false;
        Socket moved{std::move(accepted.value())};
        if (!moved.valid()) return false;
        if (accepted->valid()) return false;
        if (!moved.send(ByteView{tx, 4})) return false;
        if (!moved.recv(MutByteView{rx, 4})) return false;
        if (rx[0] != 0x2A) return false;
        if (!moved.close()) return false;

        if (!udp.open(ref, SocketKind::udp)) return false;
        if (!udp.send_to(Endpoint::ipv4(192, 168, 1, 10, 5000), ByteView{tx, 4})) return false;
        if (!udp.recv_from(&peer, MutByteView{rx, 4})) return false;
        if (peer.port != 9000) return false;
        if (!udp.shutdown()) return false;
        if (!udp.close()) return false;
        return provider.close_count == 2;
    }
}
#endif
