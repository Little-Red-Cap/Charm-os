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
            if (lhs[i] != rhs[i]) return false;
        }
        return true;
    }

    bool copy_bytes(net::MutByteView dst, net::ByteView src, util::usize* written) noexcept {
        if (!written || src.size() > dst.size()) {
            return false;
        }
        for (util::usize i = 0; i < src.size(); ++i) {
            dst[i] = src[i];
        }
        *written = src.size();
        return true;
    }

    using Session = net::ServiceSession<64, 4, 8>;
    using Driver = net::ReactorSocketDriver<Session, 8>;

    struct ClientState {
        util::u16 ping_request_id{0};
        util::u16 reject_request_id{0};
        util::u16 missing_request_id{0};
        bool got_ping_response{false};
        bool got_reject_response{false};
        bool got_missing_response{false};
        bool failed{false};

        void on_response(util::u16 request_id,
                         util::u8 opcode,
                         net::ServiceStatus status,
                         net::ByteView payload) noexcept {
            static constexpr util::u8 pong[]{'p', 'o', 'n', 'g'};
            static constexpr util::u8 reject[]{'b', 'a', 'd', '-', 'p', 'i', 'n', 'g'};

            if (request_id == ping_request_id) {
                got_ping_response = opcode == 0x10u
                    && status == net::ServiceStatus::ok
                    && bytes_eq(payload, net::ByteView{pong, 4});
                if (!got_ping_response) {
                    failed = true;
                }
                return;
            }

            if (request_id == reject_request_id) {
                got_reject_response = opcode == 0x20u
                    && status == net::ServiceStatus::bad_request
                    && bytes_eq(payload, net::ByteView{reject, 8});
                if (!got_reject_response) {
                    failed = true;
                }
                return;
            }

            if (request_id == missing_request_id) {
                got_missing_response = opcode == 0x30u
                    && status == net::ServiceStatus::not_supported
                    && payload.empty();
                if (!got_missing_response) {
                    failed = true;
                }
                return;
            }

            failed = true;
        }

        void on_timeout(util::u16, util::u8) noexcept {
            failed = true;
        }

        static void on_error(void* ctx, net::errc) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;
            self->failed = true;
        }
    };

    struct ServerState {
        bool saw_ping_route{false};
        bool saw_reject_route{false};
        bool failed{false};

        net::ServiceStatus on_route(net::ByteView request,
                                    net::MutByteView response,
                                    util::usize* response_size) noexcept {
            static constexpr util::u8 ping[]{'p', 'i', 'n', 'g'};
            static constexpr util::u8 pong[]{'p', 'o', 'n', 'g'};

            saw_ping_route = true;
            if (!bytes_eq(request, net::ByteView{ping, 4})) {
                failed = true;
                return net::ServiceStatus::bad_request;
            }
            if (!copy_bytes(response, net::ByteView{pong, 4}, response_size)) {
                failed = true;
                return net::ServiceStatus::internal_error;
            }
            return net::ServiceStatus::ok;
        }

        net::ServiceStatus reject_route(net::ByteView,
                                        net::MutByteView response,
                                        util::usize* response_size) noexcept {
            static constexpr util::u8 reject[]{'b', 'a', 'd', '-', 'p', 'i', 'n', 'g'};

            saw_reject_route = true;
            if (!copy_bytes(response, net::ByteView{reject, 8}, response_size)) {
                failed = true;
                return net::ServiceStatus::internal_error;
            }
            return net::ServiceStatus::bad_request;
        }

        static void on_error(void* ctx, net::errc) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) return;
            self->failed = true;
        }
    };

    struct RejectRoute {
        ServerState& state;

        net::ServiceStatus on_route(net::ByteView request,
                                    net::MutByteView response,
                                    util::usize* response_size) noexcept {
            return state.reject_route(request, response, response_size);
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
    for (util::u16 candidate = 32100; candidate < 32200; ++candidate) {
        auto listening = net::TcpListener::listening_loopback(stack, candidate, 2);
        if (!listening) continue;
        listener = std::move(listening.value());
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor service listener failed\n", stderr);
        return 1;
    }

    net::SocketChannelBinding client_binding{};
    net::SocketChannelBinding server_binding{server_side.raw()};

    Session client_session{};
    Session server_session{};
    ClientState client_state{};
    ServerState server_state{};
    RejectRoute reject_route_handler{server_state};

    client_session.set_error_handler(&ClientState::on_error, &client_state);
    server_session.set_error_handler(&ServerState::on_error, &server_state);

    auto ping_route = server_session.set_route(0x10u, net::ServiceRouteHandlerRef::bind(server_state));
    auto reject_route = server_session.set_route(0x20u, net::ServiceRouteHandlerRef::bind(reject_route_handler));
    if (!ping_route || !reject_route) {
        std::fputs("reactor service route registration failed\n", stderr);
        return 2;
    }

    Driver client_driver{reactor, socket_poller, client_binding, client_session};
    Driver server_driver{reactor, socket_poller, server_binding, server_session};
    net::TcpSingleAcceptDriver<Driver, 8> listener_driver{
        reactor, socket_poller, listener, server_side, server_driver};

    auto listener_started = listener_driver.start(all_reactor_events());
    if (!listener_started) {
        std::fputs("reactor service listener driver start failed\n", stderr);
        return 3;
    }

    auto connected = net::TcpClient::connected_loopback(stack, port);
    if (!connected) {
        std::fputs("reactor service client connect failed\n", stderr);
        return 4;
    }
    client = std::move(connected.value());
    client_binding.bind(client.raw());

    auto client_started = client_driver.start();
    if (!client_started) {
        std::fputs("reactor service client driver start failed\n", stderr);
        return 5;
    }

    static constexpr util::u8 ping[]{'p', 'i', 'n', 'g'};
    static constexpr util::u8 noop[]{'n', 'o', 'o', 'p'};

    auto ping_request = client_session.send_request(
        0x10u,
        net::ByteView{ping, 4},
        0,
        200,
        net::ServiceResponseHandlerRef::bind(client_state),
        net::ServiceTimeoutHandlerRef::bind(client_state));
    if (!ping_request) {
        std::fputs("reactor service ping request failed\n", stderr);
        return 6;
    }
    client_state.ping_request_id = ping_request.value();

    auto reject_request = client_session.send_request(
        0x20u,
        net::ByteView{noop, 4},
        0,
        200,
        net::ServiceResponseHandlerRef::bind(client_state),
        net::ServiceTimeoutHandlerRef::bind(client_state));
    if (!reject_request) {
        std::fputs("reactor service reject request failed\n", stderr);
        return 7;
    }
    client_state.reject_request_id = reject_request.value();

    auto missing_request = client_session.send_request(
        0x30u,
        net::ByteView{noop, 4},
        0,
        200,
        net::ServiceResponseHandlerRef::bind(client_state),
        net::ServiceTimeoutHandlerRef::bind(client_state));
    if (!missing_request) {
        std::fputs("reactor service missing request failed\n", stderr);
        return 8;
    }
    client_state.missing_request_id = missing_request.value();

    bool done = false;
    for (int i = 0; i < 400; ++i) {
        const util::u32 now_ms = static_cast<util::u32>(i * 2);

        (void)socket_poller.poll();
        (void)reactor.drain(8);
        client_session.tick(now_ms);
        server_session.tick(now_ms);

        if (listener_driver.failed() || client_state.failed || server_state.failed) {
            std::fputs("reactor service protocol failed\n", stderr);
            return 9;
        }

        done = listener_driver.accepted()
            && server_state.saw_ping_route
            && server_state.saw_reject_route
            && client_state.got_ping_response
            && client_state.got_reject_response
            && client_state.got_missing_response;
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
        std::fputs("reactor service protocol timeout\n", stderr);
        return 10;
    }

    std::puts("net reactor service echo smoke: ok");
    return 0;
}
