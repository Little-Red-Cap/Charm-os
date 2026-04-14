#include <cstdio>

import charm.net;
import net.backend.stub;
import util.core;

namespace {
    struct CloseRequest {
        util::u8 text[4]{};
    };

    struct CloseReply {
        util::u8 text[4]{};
    };

    using CloseOp = net::TrivialServiceOp<0x71u, CloseRequest, CloseReply>;
    using Session = net::TypedServiceSession<64, 4, 4, 4>;
    using Driver = net::ReactorSocketDriver<Session, 4>;

    struct ClientState {
        util::u16 request_id{0};
        int response_count{0};
        int timeout_count{0};
        int error_count{0};
        net::errc last_error{net::errc::ok};

        static void on_response(void* ctx,
                                util::u16 request_id,
                                net::ServiceStatus,
                                const CloseReply&) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }
            ++self->response_count;
            if (request_id != self->request_id) {
                self->last_error = net::errc::format_error;
            }
        }

        static void on_timeout(void* ctx, util::u16 request_id) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }
            ++self->timeout_count;
            if (request_id != self->request_id) {
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
}

int main() {
    net::backend::StubProvider<8, 64, 64, 4> provider{};
    net::Stack stack{provider};
    io::Reactor reactor{};
    net::SocketPoller<4> poller{reactor};

    net::TcpListener listener{};
    net::TcpClient client{};
    net::TcpClient server{};

    if (!listener.listen(stack, net::Endpoint::ipv4_loopback(30203), 2)) {
        std::fputs("reactor service typed request close listen failed\n", stderr);
        return 1;
    }
    if (!client.connect(stack, net::Endpoint::ipv4_loopback(30203))) {
        std::fputs("reactor service typed request close connect failed\n", stderr);
        return 2;
    }
    if (!listener.accept(server, nullptr)) {
        std::fputs("reactor service typed request close accept failed\n", stderr);
        return 3;
    }

    net::SocketChannelBinding client_binding{client.raw()};
    Session client_session{};
    ClientState client_state{};
    client_session.set_error_handler(&ClientState::on_error, &client_state);

    Driver client_driver{reactor, poller, client_binding, client_session};
    if (!client_driver.start()) {
        std::fputs("reactor service typed request close driver start failed\n", stderr);
        return 4;
    }

    CloseRequest request{{'h', 'o', 'l', 'd'}};
    auto sent = client_session.send_request<CloseOp>(
        request,
        0,
        1000,
        &ClientState::on_response,
        &ClientState::on_timeout,
        &client_state);
    if (!sent) {
        std::fputs("reactor service typed request close send_request failed\n", stderr);
        return 5;
    }
    client_state.request_id = sent.value();

    (void)poller.poll();
    (void)reactor.drain(8);

    util::u8 rx[16]{};
    auto received = server.recv(net::MutByteView{rx, sizeof(rx)});
    if (!received || received.value() != 10) {
        std::fputs("reactor service typed request close server recv failed\n", stderr);
        return 6;
    }
    if (rx[0] != 0x00u || rx[1] != 0x08u || rx[4] != 0x71u || rx[6] != 'h') {
        std::fputs("reactor service typed request close wire mismatch\n", stderr);
        return 7;
    }
    if (!server.close()) {
        std::fputs("reactor service typed request close server close failed\n", stderr);
        return 8;
    }

    bool done = false;
    for (int i = 0; i < 16; ++i) {
        (void)poller.poll();
        (void)reactor.drain(8);
        done = client_state.error_count == 1
            && client_state.last_error == net::errc::closed;
        if (done) {
            break;
        }
    }

    client_driver.stop();
    (void)client.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor service typed request close timeout\n", stderr);
        return 9;
    }
    if (client_state.response_count != 0) {
        std::fputs("reactor service typed request close unexpected response\n", stderr);
        return 10;
    }
    if (client_state.timeout_count != 0) {
        std::fputs("reactor service typed request close unexpected timeout\n", stderr);
        return 11;
    }
    if (client_session.has_pending() || client_session.pending_count() != 0) {
        std::fputs("reactor service typed request close pending not cleared\n", stderr);
        return 12;
    }
    if (client_session.last_error() != net::errc::closed) {
        std::fputs("reactor service typed request close session error mismatch\n", stderr);
        return 13;
    }
    if (!client_driver.closed() || client_driver.last_error() != net::errc::closed) {
        std::fputs("reactor service typed request close driver state mismatch\n", stderr);
        return 14;
    }

    std::puts("net reactor service typed request close smoke: ok");
    return 0;
}
