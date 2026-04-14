#include <chrono>
#include <cstdio>
#include <thread>

import charm.net;
import net.backend.win;
import util.core;

namespace {
    using namespace std::chrono_literals;

    [[nodiscard]] constexpr util::u32 readable_event() noexcept {
        return static_cast<util::u32>(io::Event::readable);
    }

    [[nodiscard]] constexpr util::u32 writable_event() noexcept {
        return static_cast<util::u32>(io::Event::writable);
    }

    [[nodiscard]] constexpr util::u32 closed_event() noexcept {
        return static_cast<util::u32>(io::Event::closed);
    }

    [[nodiscard]] constexpr util::u32 error_event() noexcept {
        return static_cast<util::u32>(io::Event::error);
    }

    bool bytes_eq(const util::u8* lhs, const util::u8* rhs, util::usize count) noexcept {
        for (util::usize i = 0; i < count; ++i) {
            if (lhs[i] != rhs[i]) return false;
        }
        return true;
    }

    struct ListenerCtx {
        net::SocketPoller<8>* poller{nullptr};
        net::TcpListener* listener{nullptr};
        net::TcpClient* accepted_client{nullptr};
        net::SocketChannelBinding* accepted_binding{nullptr};
        net::SocketWatch* accepted_watch{nullptr};
        net::Endpoint peer{};
        bool accepted{false};
        bool failed{false};

        static void on_event(void* ctx, io::Channel&, util::u32 events) noexcept {
            auto* self = static_cast<ListenerCtx*>(ctx);
            if (!self || !self->poller || !self->listener || !self->accepted_client
                || !self->accepted_binding || !self->accepted_watch) {
                return;
            }
            if ((events & (closed_event() | error_event())) != 0u) {
                self->failed = true;
                return;
            }
            if ((events & readable_event()) == 0u || self->accepted) {
                return;
            }

            auto accepted = self->listener->accept(*self->accepted_client, &self->peer);
            if (!accepted) {
                if (accepted.error() == net::errc::would_block) {
                    return;
                }
                self->failed = true;
                return;
            }

            self->accepted_binding->bind(self->accepted_client->raw());
            auto watch = self->poller->watch(self->accepted_client->raw(), self->accepted_binding->channel());
            if (!watch) {
                self->failed = true;
                return;
            }
            *self->accepted_watch = watch.value();
            self->accepted = true;
        }
    };

    struct ClientCtx {
        net::SocketPoller<8>* poller{nullptr};
        net::TcpClient* client{nullptr};
        net::SocketWatch watch{};
        util::u8 tx[8]{};
        util::usize tx_len{0};
        util::usize tx_off{0};
        util::u8 rx[8]{};
        bool sent_ping{false};
        bool received_pong{false};
        bool failed{false};

        static void on_event(void* ctx, io::Channel& ch, util::u32 events) noexcept {
            auto* self = static_cast<ClientCtx*>(ctx);
            if (!self || !self->poller || !self->client) {
                return;
            }
            if ((events & error_event()) != 0u) {
                self->failed = true;
                return;
            }
            if ((events & writable_event()) != 0u) {
                while (self->tx_off < self->tx_len) {
                    auto written = ch.write(io::ByteView{self->tx + self->tx_off, self->tx_len - self->tx_off});
                    if (!written) {
                        if (written.error() == io::errc::would_block) {
                            (void)self->poller->arm(self->watch, writable_event());
                            return;
                        }
                        self->failed = true;
                        return;
                    }
                    self->tx_off += written.value();
                }
                if (self->tx_len != 0) {
                    self->sent_ping = true;
                    self->tx_len = 0;
                    self->tx_off = 0;
                }
            }
            if ((events & readable_event()) != 0u) {
                auto read = ch.read(io::MutByteView{self->rx, sizeof(self->rx)});
                if (!read) {
                    if (read.error() == io::errc::would_block) {
                        return;
                    }
                    self->failed = true;
                    return;
                }
                self->received_pong = read.value() == 4
                    && bytes_eq(self->rx, reinterpret_cast<const util::u8*>("pong"), 4);
                if (!self->received_pong) {
                    self->failed = true;
                    return;
                }
                self->poller->unwatch(self->watch);
                (void)self->client->close();
            }
            if ((events & closed_event()) != 0u) {
                self->poller->unwatch(self->watch);
            }
        }
    };

    struct ServerCtx {
        net::SocketPoller<8>* poller{nullptr};
        net::SocketWatch watch{};
        util::u8 tx[8]{};
        util::usize tx_len{0};
        util::usize tx_off{0};
        util::u8 rx[8]{};
        bool received_ping{false};
        bool sent_pong{false};
        bool saw_closed{false};
        bool failed{false};

        static void on_event(void* ctx, io::Channel& ch, util::u32 events) noexcept {
            auto* self = static_cast<ServerCtx*>(ctx);
            if (!self || !self->poller) {
                return;
            }
            if ((events & error_event()) != 0u) {
                self->failed = true;
                return;
            }
            if ((events & closed_event()) != 0u) {
                self->saw_closed = true;
                return;
            }
            if ((events & readable_event()) != 0u) {
                auto read = ch.read(io::MutByteView{self->rx, sizeof(self->rx)});
                if (!read) {
                    if (read.error() == io::errc::would_block) {
                        return;
                    }
                    self->failed = true;
                    return;
                }
                self->received_ping = read.value() == 4
                    && bytes_eq(self->rx, reinterpret_cast<const util::u8*>("ping"), 4);
                if (!self->received_ping) {
                    self->failed = true;
                    return;
                }
                self->tx[0] = 'p';
                self->tx[1] = 'o';
                self->tx[2] = 'n';
                self->tx[3] = 'g';
                self->tx_len = 4;
                self->tx_off = 0;
                (void)self->poller->arm(self->watch, writable_event());
            }
            if ((events & writable_event()) != 0u) {
                while (self->tx_off < self->tx_len) {
                    auto written = ch.write(io::ByteView{self->tx + self->tx_off, self->tx_len - self->tx_off});
                    if (!written) {
                        if (written.error() == io::errc::would_block) {
                            (void)self->poller->arm(self->watch, writable_event());
                            return;
                        }
                        self->failed = true;
                        return;
                    }
                    self->tx_off += written.value();
                }
                if (self->tx_len != 0) {
                    self->sent_pong = true;
                    self->tx_len = 0;
                    self->tx_off = 0;
                }
            }
        }
    };
}

int main() {
    net::backend::WinProvider<16> provider{};
    net::Stack stack{provider};

    io::Reactor reactor{};
    net::SocketPoller<8> socket_poller{reactor};

    net::TcpListener listener{};
    net::TcpClient client{};
    net::TcpClient server_side{};

    net::SocketEventChannelBinding listener_binding{listener.raw()};
    net::SocketChannelBinding client_binding{client.raw()};
    net::SocketChannelBinding server_binding{server_side.raw()};

    util::u16 port = 0;
    for (util::u16 candidate = 29000; candidate < 29100; ++candidate) {
        if (!listener.listen(stack, net::Endpoint::ipv4_loopback(candidate), 2)) continue;
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor listen failed\n", stderr);
        return 1;
    }

    ListenerCtx listener_ctx{};
    ServerCtx server_ctx{};
    ClientCtx client_ctx{};

    listener_ctx.poller = &socket_poller;
    listener_ctx.listener = &listener;
    listener_ctx.accepted_client = &server_side;
    listener_ctx.accepted_binding = &server_binding;
    listener_ctx.accepted_watch = &server_ctx.watch;

    server_ctx.poller = &socket_poller;

    client_ctx.poller = &socket_poller;
    client_ctx.client = &client;
    client_ctx.tx[0] = 'p';
    client_ctx.tx[1] = 'i';
    client_ctx.tx[2] = 'n';
    client_ctx.tx[3] = 'g';
    client_ctx.tx_len = 4;

    const util::u32 all_events = readable_event() | writable_event() | closed_event() | error_event();

    auto listener_sub = reactor.subscribe(listener_binding.channel(), all_events, &ListenerCtx::on_event, &listener_ctx);
    auto server_sub = reactor.subscribe(server_binding.channel(), all_events, &ServerCtx::on_event, &server_ctx);
    auto client_sub = reactor.subscribe(client_binding.channel(), all_events, &ClientCtx::on_event, &client_ctx);
    if (!listener_sub || !server_sub || !client_sub) {
        std::fputs("reactor subscribe failed\n", stderr);
        return 2;
    }

    auto listener_watch = socket_poller.watch(listener.raw(), listener_binding.channel());
    if (!listener_watch) {
        std::fputs("reactor listener watch failed\n", stderr);
        return 3;
    }

    if (!client.connect(stack, net::Endpoint::ipv4_loopback(port))) {
        std::fputs("reactor client connect failed\n", stderr);
        return 4;
    }

    auto client_watch = socket_poller.watch(client.raw(), client_binding.channel());
    if (!client_watch) {
        std::fputs("reactor client watch failed\n", stderr);
        return 5;
    }
    client_ctx.watch = client_watch.value();
    if (!socket_poller.arm(client_ctx.watch, writable_event())) {
        std::fputs("reactor client arm writable failed\n", stderr);
        return 6;
    }

    bool done = false;
    for (int i = 0; i < 400; ++i) {
        (void)socket_poller.poll();
        (void)reactor.drain(8);

        if (listener_ctx.failed || client_ctx.failed || server_ctx.failed) {
            std::fputs("reactor callback failed\n", stderr);
            return 7;
        }

        done = listener_ctx.accepted
            && client_ctx.sent_ping
            && client_ctx.received_pong
            && server_ctx.received_ping
            && server_ctx.sent_pong
            && server_ctx.saw_closed;
        if (done) break;
        std::this_thread::sleep_for(2ms);
    }

    reactor.unsubscribe(client_sub.value());
    reactor.unsubscribe(server_sub.value());
    reactor.unsubscribe(listener_sub.value());
    socket_poller.clear();

    (void)client.close();
    (void)server_side.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor loopback timeout\n", stderr);
        return 8;
    }

    std::puts("net reactor loopback smoke: ok");
    return 0;
}
