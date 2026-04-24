#include <cstdio>
#include <utility>

import charm.net;
import net.backend.stub;
import util.core;

namespace {
    struct ErrorRequest {
        util::u8 text[4]{};
    };

    struct ErrorReply {
        util::u8 text[4]{};
    };

    using ErrorOp = net::TrivialServiceOp<0x77u, ErrorRequest, ErrorReply>;
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
                                const ErrorReply&) noexcept {
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

    auto listening = net::TcpListener::listening_loopback(stack, 30209, 2);
    if (!listening) {
        std::fputs("reactor service typed request error listen failed\n", stderr);
        return 1;
    }
    listener = std::move(listening.value());

    auto connected = net::TcpClient::connected_loopback(stack, 30209);
    if (!connected) {
        std::fputs("reactor service typed request error connect failed\n", stderr);
        return 2;
    }
    client = std::move(connected.value());
    if (!listener.accept(server, nullptr)) {
        std::fputs("reactor service typed request error accept failed\n", stderr);
        return 3;
    }

    net::SocketChannelBinding client_binding{};
    client_binding.bind(client.raw());
    Session client_session{};
    ClientState client_state{};
    client_session.set_error_handler(&ClientState::on_error, &client_state);

    Driver client_driver{reactor, poller, client_binding, client_session};
    if (!client_driver.start()) {
        std::fputs("reactor service typed request error driver start failed\n", stderr);
        return 4;
    }

    ErrorRequest request{{'h', 'o', 'l', 'd'}};
    auto sent = client_session.send_request<ErrorOp>(
        request,
        0,
        1000,
        &ClientState::on_response,
        &ClientState::on_timeout,
        &client_state);
    if (!sent) {
        std::fputs("reactor service typed request error send_request failed\n", stderr);
        return 5;
    }
    client_state.request_id = sent.value();

    (void)poller.poll();
    (void)reactor.drain(8);

    util::u8 rx[16]{};
    auto received = server.recv(net::MutByteView{rx, sizeof(rx)});
    if (!received || received.value() != 10) {
        std::fputs("reactor service typed request error server recv failed\n", stderr);
        return 6;
    }
    if (rx[0] != 0x00u || rx[1] != 0x08u || rx[4] != 0x77u || rx[6] != 'h') {
        std::fputs("reactor service typed request error wire mismatch\n", stderr);
        return 7;
    }

    auto injected = provider.inject_poll_error(client.raw().handle(), net::errc::io);
    if (!injected) {
        std::fputs("reactor service typed request error inject failed\n", stderr);
        return 8;
    }

    bool done = false;
    for (int i = 0; i < 16; ++i) {
        (void)poller.poll();
        (void)reactor.drain(8);
        done = client_state.error_count == 1
            && client_state.last_error == net::errc::io;
        if (done) {
            break;
        }
    }

    auto retried = client_session.send_request<ErrorOp>(
        request,
        1,
        1000,
        &ClientState::on_response,
        &ClientState::on_timeout,
        &client_state);

    client_driver.stop();
    (void)server.close();
    (void)client.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor service typed request error timeout\n", stderr);
        return 9;
    }
    if (client_state.response_count != 0) {
        std::fputs("reactor service typed request error unexpected response\n", stderr);
        return 10;
    }
    if (client_state.timeout_count != 0) {
        std::fputs("reactor service typed request error unexpected timeout\n", stderr);
        return 11;
    }
    if (retried || retried.error() != net::errc::io) {
        std::fputs("reactor service typed request error retry not rejected\n", stderr);
        return 12;
    }
    if (client_session.has_pending() || client_session.pending_count() != 0) {
        std::fputs("reactor service typed request error pending not cleared\n", stderr);
        return 13;
    }
    if (client_session.last_error() != net::errc::io) {
        std::fputs("reactor service typed request error session error mismatch\n", stderr);
        return 14;
    }
    if (client_driver.closed() || client_driver.last_error() != net::errc::io) {
        std::fputs("reactor service typed request error driver state mismatch\n", stderr);
        return 15;
    }

    std::puts("net reactor service typed request error smoke: ok");
    return 0;
}
