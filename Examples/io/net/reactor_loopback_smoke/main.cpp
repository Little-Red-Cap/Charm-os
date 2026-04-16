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

    net::SocketChannelBinding client_binding{};
    net::SocketChannelBinding server_binding{server_side.raw()};

    util::u16 port = 0;
    for (util::u16 candidate = 29000; candidate < 29100; ++candidate) {
        auto listening = net::TcpListener::listening_loopback(stack, candidate, 2);
        if (!listening) continue;
        listener = std::move(listening.value());
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor listen failed\n", stderr);
        return 1;
    }

    ServerCtx server_ctx{};
    ClientCtx client_ctx{};

    server_ctx.poller = &socket_poller;

    client_ctx.poller = &socket_poller;
    client_ctx.client = &client;
    client_ctx.tx[0] = 'p';
    client_ctx.tx[1] = 'i';
    client_ctx.tx[2] = 'n';
    client_ctx.tx[3] = 'g';
    client_ctx.tx_len = 4;

    const util::u32 all_events = readable_event() | writable_event() | closed_event() | error_event();
    net::SocketWatchDriver<8> server_accept_driver{socket_poller, server_binding, server_ctx.watch};
    net::TcpSingleAcceptDriver<net::SocketWatchDriver<8>, 8> listener_driver{
        reactor, socket_poller, listener, server_side, server_accept_driver};

    auto server_sub = reactor.subscribe(server_binding.channel(), all_events, &ServerCtx::on_event, &server_ctx);
    if (!server_sub) {
        std::fputs("reactor subscribe failed\n", stderr);
        return 2;
    }

    auto listener_started = listener_driver.start(all_events);
    if (!listener_started) {
        std::fputs("reactor listener driver start failed\n", stderr);
        return 3;
    }

    auto connected = net::TcpClient::connected_loopback(stack, port);
    if (!connected) {
        std::fputs("reactor client connect failed\n", stderr);
        return 4;
    }
    client = std::move(connected.value());
    client_binding.bind(client.raw());

    auto client_sub = reactor.subscribe(client_binding.channel(), all_events, &ClientCtx::on_event, &client_ctx);
    if (!client_sub) {
        std::fputs("reactor subscribe failed\n", stderr);
        return 2;
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

        if (listener_driver.failed() || client_ctx.failed || server_ctx.failed) {
            std::fputs("reactor callback failed\n", stderr);
            return 7;
        }

        done = listener_driver.accepted()
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
    server_accept_driver.stop();
    listener_driver.stop();
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
