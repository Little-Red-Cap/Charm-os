#include <chrono>
#include <cstdio>
#include <thread>

import charm.net;
import net.backend.win;
import util.core;

namespace {
    using namespace std::chrono_literals;

    [[nodiscard]] constexpr util::u32 all_reactor_events() noexcept {
        return static_cast<util::u32>(io::Event::readable)
            | static_cast<util::u32>(io::Event::writable)
            | static_cast<util::u32>(io::Event::closed)
            | static_cast<util::u32>(io::Event::error);
    }

    struct ClientState {
        net::LineSession<64>* session{nullptr};
        bool sent_ping{false};
        bool received_pong{false};
        bool failed{false};

        static void on_line(void* ctx, std::string_view line) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;
            self->received_pong = line == "pong";
            if (!self->received_pong) {
                self->failed = true;
            }
        }

        static void on_error(void* ctx, net::errc) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;
            self->failed = true;
        }
    };

    struct ServerState {
        net::LineSession<64>* session{nullptr};
        bool received_ping{false};
        bool sent_pong{false};
        bool failed{false};

        static void on_line(void* ctx, std::string_view line) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self || !self->session) return;
            if (line != "ping") {
                self->failed = true;
                return;
            }
            self->received_ping = true;
            auto sent = self->session->send_line("pong", net::LineEnding::lf);
            if (!sent) {
                self->failed = true;
                return;
            }
            self->sent_pong = true;
        }

        static void on_error(void* ctx, net::errc) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) return;
            self->failed = true;
        }
    };

    template <class Driver>
    struct ListenerState {
        net::TcpListener* listener{nullptr};
        net::TcpClient* server_socket{nullptr};
        net::SocketChannelBinding* server_binding{nullptr};
        Driver* server_driver{nullptr};
        bool accepted{false};
        bool failed{false};

        static void on_event(void* ctx, io::Channel&, util::u32 events) noexcept {
            auto* self = static_cast<ListenerState*>(ctx);
            if (!self || !self->listener || !self->server_socket || !self->server_binding || !self->server_driver) {
                return;
            }
            if ((events & static_cast<util::u32>(io::Event::error)) != 0u) {
                self->failed = true;
                return;
            }
            if ((events & static_cast<util::u32>(io::Event::readable)) == 0u || self->accepted) {
                return;
            }

            net::Endpoint peer{};
            auto accepted = self->listener->accept(*self->server_socket, &peer);
            if (!accepted) {
                if (accepted.error() == net::errc::would_block) {
                    return;
                }
                self->failed = true;
                return;
            }

            self->server_binding->bind(self->server_socket->raw());
            auto started = self->server_driver->start();
            if (!started) {
                self->failed = true;
                return;
            }
            self->accepted = true;
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

    util::u16 port = 0;
    for (util::u16 candidate = 30000; candidate < 30100; ++candidate) {
        if (!listener.listen(stack, net::Endpoint::ipv4_loopback(candidate), 2)) continue;
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor line listener failed\n", stderr);
        return 1;
    }

    net::SocketEventChannelBinding listener_binding{listener.raw()};
    net::SocketChannelBinding client_binding{client.raw()};
    net::SocketChannelBinding server_binding{server_side.raw()};

    net::LineSession<64> client_session{};
    net::LineSession<64> server_session{};
    ClientState client_state{};
    ServerState server_state{};

    client_state.session = &client_session;
    client_session.set_line_handler(&ClientState::on_line, &client_state);
    client_session.set_error_handler(&ClientState::on_error, &client_state);

    server_state.session = &server_session;
    server_session.set_line_handler(&ServerState::on_line, &server_state);
    server_session.set_error_handler(&ServerState::on_error, &server_state);

    using DriverType = net::ReactorSocketDriver<net::LineSession<64>, 8>;
    DriverType client_driver{reactor, socket_poller, client_binding, client_session};
    DriverType server_driver{reactor, socket_poller, server_binding, server_session};

    ListenerState<DriverType> listener_state{};
    listener_state.listener = &listener;
    listener_state.server_socket = &server_side;
    listener_state.server_binding = &server_binding;
    listener_state.server_driver = &server_driver;

    auto listener_sub = reactor.subscribe(listener_binding.channel(), all_reactor_events(), &ListenerState<DriverType>::on_event, &listener_state);
    if (!listener_sub) {
        std::fputs("reactor line listener subscribe failed\n", stderr);
        return 2;
    }

    auto listener_watch = socket_poller.watch(listener.raw(), listener_binding.channel());
    if (!listener_watch) {
        std::fputs("reactor line listener watch failed\n", stderr);
        return 3;
    }

    if (!client.connect(stack, net::Endpoint::ipv4_loopback(port))) {
        std::fputs("reactor line client connect failed\n", stderr);
        return 4;
    }

    auto client_started = client_driver.start();
    if (!client_started) {
        std::fputs("reactor line client driver start failed\n", stderr);
        return 5;
    }

    auto ping_sent = client_session.send_line("ping", net::LineEnding::lf);
    if (!ping_sent) {
        std::fputs("reactor line client send failed\n", stderr);
        return 6;
    }
    client_state.sent_ping = true;

    bool done = false;
    for (int i = 0; i < 400; ++i) {
        (void)socket_poller.poll();
        (void)reactor.drain(8);

        if (listener_state.failed || client_state.failed || server_state.failed) {
            std::fputs("reactor line protocol failed\n", stderr);
            return 7;
        }

        done = listener_state.accepted
            && client_state.sent_ping
            && server_state.received_ping
            && server_state.sent_pong
            && client_state.received_pong;
        if (done) break;
        std::this_thread::sleep_for(2ms);
    }

    client_driver.stop();
    server_driver.stop();
    socket_poller.unwatch(listener_watch.value());
    reactor.unsubscribe(listener_sub.value());

    (void)client.close();
    (void)server_side.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor line protocol timeout\n", stderr);
        return 8;
    }

    std::puts("net reactor line echo smoke: ok");
    return 0;
}
