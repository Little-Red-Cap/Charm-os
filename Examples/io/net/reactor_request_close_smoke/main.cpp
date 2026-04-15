#include <cstdio>

import charm.net;
import net.backend.stub;
import util.core;

namespace {
    bool bytes_eq(net::ByteView lhs, net::ByteView rhs) noexcept {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (util::usize i = 0; i < lhs.size(); ++i) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        return true;
    }

    using Session = net::RequestSession<64, 4>;
    using Driver = net::ReactorSocketDriver<Session, 4>;

    struct ClientState {
        util::u16 request_id{0};
        int response_count{0};
        int timeout_count{0};
        int error_count{0};
        net::errc last_error{net::errc::ok};

        static void on_response(void* ctx,
                                util::u16 request_id,
                                util::u8 opcode,
                                bool,
                                net::ByteView payload) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }
            ++self->response_count;
            if (request_id != self->request_id
                || opcode != 0x33u
                || !bytes_eq(payload, {})) {
                self->last_error = net::errc::format_error;
            }
        }

        static void on_timeout(void* ctx,
                               util::u16 request_id,
                               util::u8 opcode) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }
            ++self->timeout_count;
            if (request_id != self->request_id || opcode != 0x33u) {
                self->last_error = net::errc::format_error;
            }
        }

        static void on_error(void* ctx, net::errc error) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }
            ++self->error_count;
            self->last_error = error;
        }
    };

    struct ServerState {
        net::TcpClient* socket{nullptr};
        bool saw_request{false};
        bool close_ok{false};

        static void on_request(void* ctx,
                               Session&,
                               util::u16 request_id,
                               util::u8 opcode,
                               net::ByteView payload) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return;
            }

            static constexpr util::u8 hold[]{'h', 'o', 'l', 'd'};
            self->saw_request = request_id != 0
                && opcode == 0x33u
                && bytes_eq(payload, net::ByteView{hold, 4});
            if (!self->saw_request || !self->socket) {
                return;
            }
            auto closed = self->socket->close();
            self->close_ok = static_cast<bool>(closed);
        }
    };
}

int main() {
    net::backend::StubProvider<8, 64, 64, 4> provider{};
    net::Stack stack{provider};
    io::Reactor reactor{};
    net::SocketPoller<4> poller{reactor};

    net::TcpListener listener{};
    net::TcpClient client{};
    net::TcpClient server{};

    if (!listener.listen(stack, net::Endpoint::ipv4_loopback(30201), 2)) {
        std::fputs("reactor request close listen failed\n", stderr);
        return 1;
    }
    if (!client.connect(stack, net::Endpoint::ipv4_loopback(30201))) {
        std::fputs("reactor request close connect failed\n", stderr);
        return 2;
    }
    if (!listener.accept(server, nullptr)) {
        std::fputs("reactor request close accept failed\n", stderr);
        return 3;
    }

    net::SocketChannelBinding client_binding{client.raw()};
    net::SocketChannelBinding server_binding{server.raw()};

    Session client_session{};
    Session server_session{};
    ClientState client_state{};
    ServerState server_state{};
    server_state.socket = &server;

    client_session.set_error_handler(&ClientState::on_error, &client_state);
    server_session.set_request_handler(&ServerState::on_request, &server_state);

    Driver client_driver{reactor, poller, client_binding, client_session};
    Driver server_driver{reactor, poller, server_binding, server_session};
    if (!client_driver.start()) {
        std::fputs("reactor request close client driver start failed\n", stderr);
        return 4;
    }
    if (!server_driver.start()) {
        std::fputs("reactor request close server driver start failed\n", stderr);
        return 5;
    }

    static constexpr util::u8 hold[]{'h', 'o', 'l', 'd'};
    auto request = client_session.send_request(
        0x33u,
        net::ByteView{hold, 4},
        0,
        1000,
        &ClientState::on_response,
        &ClientState::on_timeout,
        &client_state);
    if (!request) {
        std::fputs("reactor request close send_request failed\n", stderr);
        return 6;
    }
    client_state.request_id = request.value();

    bool done = false;
    for (int i = 0; i < 16; ++i) {
        (void)poller.poll();
        (void)reactor.drain(8);
        done = server_state.saw_request
            && server_state.close_ok
            && client_state.error_count == 1
            && client_state.last_error == net::errc::closed;
        if (done) {
            break;
        }
    }

    auto late_request = client_session.send_request(
        0x44u,
        net::ByteView{hold, 4},
        1,
        1000,
        &ClientState::on_response,
        &ClientState::on_timeout,
        &client_state);
    if (late_request || late_request.error() != net::errc::closed) {
        std::fputs("reactor request close late send_request not rejected\n", stderr);
        return 7;
    }

    client_driver.stop();
    server_driver.stop();
    (void)client.close();
    (void)server.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor request close timeout\n", stderr);
        return 8;
    }
    if (client_state.response_count != 0) {
        std::fputs("reactor request close unexpected response\n", stderr);
        return 9;
    }
    if (client_state.timeout_count != 0) {
        std::fputs("reactor request close unexpected timeout\n", stderr);
        return 10;
    }
    if (client_state.error_count != 1 || client_state.last_error != net::errc::closed) {
        std::fputs("reactor request close unexpected error state\n", stderr);
        return 11;
    }
    if (client_session.pending_count() != 0) {
        std::fputs("reactor request close pending not cleared\n", stderr);
        return 12;
    }
    if (!client_driver.closed() || client_driver.last_error() != net::errc::closed) {
        std::fputs("reactor request close driver state mismatch\n", stderr);
        return 13;
    }

    std::puts("net reactor request close smoke: ok");
    return 0;
}
