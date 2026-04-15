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

    bool bytes_eq(net::ByteView lhs, net::ByteView rhs) noexcept {
        if (lhs.size() != rhs.size()) return false;
        for (util::usize i = 0; i < lhs.size(); ++i) {
            if (lhs.data()[i] != rhs.data()[i]) return false;
        }
        return true;
    }

    using Session = net::RequestSession<64, 4>;
    using Driver = net::ReactorSocketDriver<Session, 8>;

    struct ClientState {
        util::u16 request_ok_id{0};
        util::u16 request_timeout_id{0};
        bool sent_ok{false};
        bool sent_timeout_probe{false};
        bool got_ok_response{false};
        bool got_timeout{false};
        bool failed{false};

        static void on_response(void* ctx,
                                util::u16 request_id,
                                util::u8 opcode,
                                bool ok,
                                net::ByteView payload) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;

            static constexpr util::u8 want[]{'p', 'o', 'n', 'g'};
            self->got_ok_response = request_id == self->request_ok_id
                && opcode == 0x10u
                && ok
                && bytes_eq(payload, net::ByteView{want, 4});
            if (!self->got_ok_response) {
                self->failed = true;
            }
        }

        static void on_timeout(void* ctx,
                               util::u16 request_id,
                               util::u8 opcode) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;
            self->got_timeout = request_id == self->request_timeout_id && opcode == 0x20u;
            if (!self->got_timeout) {
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
        bool saw_ping_request{false};
        bool saw_timeout_probe{false};
        bool failed{false};

        static void on_request(void* ctx,
                               Session& session,
                               util::u16 request_id,
                               util::u8 opcode,
                               net::ByteView payload) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) return;

            static constexpr util::u8 ping[]{'p', 'i', 'n', 'g'};
            static constexpr util::u8 pong[]{'p', 'o', 'n', 'g'};

            if (opcode == 0x10u) {
                self->saw_ping_request = bytes_eq(payload, net::ByteView{ping, 4});
                if (!self->saw_ping_request) {
                    self->failed = true;
                    return;
                }
                auto sent = session.send_response(request_id, opcode, net::ByteView{pong, 4}, true);
                if (!sent) {
                    self->failed = true;
                }
                return;
            }

            if (opcode == 0x20u) {
                self->saw_timeout_probe = true;
                return;
            }

            self->failed = true;
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
    for (util::u16 candidate = 32000; candidate < 32100; ++candidate) {
        if (!listener.listen(stack, net::Endpoint::ipv4_loopback(candidate), 2)) continue;
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor request listener failed\n", stderr);
        return 1;
    }

    net::SocketChannelBinding client_binding{client.raw()};
    net::SocketChannelBinding server_binding{server_side.raw()};

    Session client_session{};
    Session server_session{};
    ClientState client_state{};
    ServerState server_state{};

    client_session.set_error_handler(&ClientState::on_error, &client_state);
    server_session.set_request_handler(&ServerState::on_request, &server_state);
    server_session.set_error_handler(&ServerState::on_error, &server_state);

    Driver client_driver{reactor, socket_poller, client_binding, client_session};
    Driver server_driver{reactor, socket_poller, server_binding, server_session};
    net::TcpSingleAcceptDriver<Driver, 8> listener_driver{
        reactor, socket_poller, listener, server_side, server_driver};

    auto listener_started = listener_driver.start(all_reactor_events());
    if (!listener_started) {
        std::fputs("reactor request listener driver start failed\n", stderr);
        return 2;
    }

    if (!client.connect(stack, net::Endpoint::ipv4_loopback(port))) {
        std::fputs("reactor request client connect failed\n", stderr);
        return 3;
    }

    auto client_started = client_driver.start();
    if (!client_started) {
        std::fputs("reactor request client driver start failed\n", stderr);
        return 4;
    }

    static constexpr util::u8 ping[]{'p', 'i', 'n', 'g'};
    static constexpr util::u8 hold[]{'h', 'o', 'l', 'd'};

    auto ok_request = client_session.send_request(
        0x10u,
        net::ByteView{ping, 4},
        0,
        200,
        &ClientState::on_response,
        &ClientState::on_timeout,
        &client_state);
    if (!ok_request) {
        std::fputs("reactor request send ok_request failed\n", stderr);
        return 5;
    }
    client_state.request_ok_id = ok_request.value();
    client_state.sent_ok = true;

    auto timeout_request = client_session.send_request(
        0x20u,
        net::ByteView{hold, 4},
        0,
        30,
        &ClientState::on_response,
        &ClientState::on_timeout,
        &client_state);
    if (!timeout_request) {
        std::fputs("reactor request send timeout_request failed\n", stderr);
        return 6;
    }
    client_state.request_timeout_id = timeout_request.value();
    client_state.sent_timeout_probe = true;

    bool done = false;
    for (int i = 0; i < 400; ++i) {
        const util::u32 now_ms = static_cast<util::u32>(i * 2);

        (void)socket_poller.poll();
        (void)reactor.drain(8);
        client_session.tick(now_ms);
        server_session.tick(now_ms);

        if (listener_driver.failed() || client_state.failed || server_state.failed) {
            std::fputs("reactor request protocol failed\n", stderr);
            return 7;
        }

        done = listener_driver.accepted()
            && client_state.sent_ok
            && client_state.sent_timeout_probe
            && server_state.saw_ping_request
            && server_state.saw_timeout_probe
            && client_state.got_ok_response
            && client_state.got_timeout;
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
        std::fputs("reactor request protocol timeout\n", stderr);
        return 8;
    }

    std::puts("net reactor request echo smoke: ok");
    return 0;
}
