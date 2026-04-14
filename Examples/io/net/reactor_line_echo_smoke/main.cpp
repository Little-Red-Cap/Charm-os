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
    net::TcpSingleAcceptDriver<DriverType, 8> listener_driver{
        reactor, socket_poller, listener, server_side, server_driver};

    auto listener_started = listener_driver.start(all_reactor_events());
    if (!listener_started) {
        std::fputs("reactor line listener driver start failed\n", stderr);
        return 2;
    }

    if (!client.connect(stack, net::Endpoint::ipv4_loopback(port))) {
        std::fputs("reactor line client connect failed\n", stderr);
        return 3;
    }

    auto client_started = client_driver.start();
    if (!client_started) {
        std::fputs("reactor line client driver start failed\n", stderr);
        return 4;
    }

    auto ping_sent = client_session.send_line("ping", net::LineEnding::lf);
    if (!ping_sent) {
        std::fputs("reactor line client send failed\n", stderr);
        return 5;
    }
    client_state.sent_ping = true;

    bool done = false;
    for (int i = 0; i < 400; ++i) {
        (void)socket_poller.poll();
        (void)reactor.drain(8);

        if (listener_driver.failed() || client_state.failed || server_state.failed) {
            std::fputs("reactor line protocol failed\n", stderr);
            return 6;
        }

        done = listener_driver.accepted()
            && client_state.sent_ping
            && server_state.received_ping
            && server_state.sent_pong
            && client_state.received_pong;
        if (done) break;
        std::this_thread::sleep_for(2ms);
    }

    client_driver.stop();
    server_driver.stop();
    listener_driver.stop();

    (void)client.close();
    (void)server_side.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor line protocol timeout\n", stderr);
        return 7;
    }

    std::puts("net reactor line echo smoke: ok");
    return 0;
}
