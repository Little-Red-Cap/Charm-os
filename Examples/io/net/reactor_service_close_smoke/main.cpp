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

    using Session = net::ServiceSession<64, 4, 4, 4>;
    using Driver = net::ReactorSocketDriver<Session, 4>;

    struct ServerState {
        net::ServiceReplyToken token{};
        int error_count{0};
        bool saw_request{false};
        bool saw_deferred_during_route{false};
        bool late_reply_rejected{false};
        net::errc last_error{net::errc::ok};

        static void on_close_probe(void* ctx,
                                   Session& session,
                                   net::ServiceReplyToken token,
                                   net::ByteView request) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return;
            }

            static constexpr util::u8 hold[]{'h', 'o', 'l', 'd'};
            self->saw_request = bytes_eq(request, net::ByteView{hold, 4});
            self->token = token;
            self->saw_deferred_during_route = session.has_deferred()
                && session.deferred_count() == 1;
        }

        static void on_error(void* ctx, net::errc error) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
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

    if (!listener.listen(stack, net::Endpoint::ipv4_loopback(30204), 2)) {
        std::fputs("reactor service close listen failed\n", stderr);
        return 1;
    }
    if (!client.connect(stack, net::Endpoint::ipv4_loopback(30204))) {
        std::fputs("reactor service close connect failed\n", stderr);
        return 2;
    }
    if (!listener.accept(server, nullptr)) {
        std::fputs("reactor service close accept failed\n", stderr);
        return 3;
    }

    net::SocketChannelBinding server_binding{server.raw()};
    Session server_session{};
    ServerState server_state{};
    server_session.set_error_handler(&ServerState::on_error, &server_state);

    auto route = server_session.set_deferred_route(0x72u,
                                                   &ServerState::on_close_probe,
                                                   &server_state);
    if (!route) {
        std::fputs("reactor service close route registration failed\n", stderr);
        return 4;
    }

    Driver server_driver{reactor, poller, server_binding, server_session};
    if (!server_driver.start()) {
        std::fputs("reactor service close driver start failed\n", stderr);
        return 5;
    }

    static constexpr util::u8 wire[]{
        0x00u, 0x08u,
        0x00u, 0x01u,
        0x72u, 0x00u,
        'h', 'o', 'l', 'd'
    };
    auto sent = client.send(net::ByteView{wire, sizeof(wire)});
    if (!sent || sent.value() != sizeof(wire)) {
        std::fputs("reactor service close client send failed\n", stderr);
        return 6;
    }
    if (!client.close()) {
        std::fputs("reactor service close client close failed\n", stderr);
        return 7;
    }

    bool done = false;
    for (int i = 0; i < 16; ++i) {
        (void)poller.poll();
        (void)reactor.drain(8);
        done = server_state.saw_request
            && server_state.saw_deferred_during_route
            && server_state.error_count == 1
            && server_state.last_error == net::errc::closed;
        if (done) {
            break;
        }
    }

    static constexpr util::u8 late_payload[]{'l', 'a', 't', 'e'};
    auto late_reply = server_session.send_deferred_response(server_state.token,
                                                            net::ServiceStatus::ok,
                                                            net::ByteView{late_payload, 4});
    server_state.late_reply_rejected = !late_reply
        && late_reply.error() == net::errc::noent;

    server_driver.stop();
    (void)server.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor service close timeout\n", stderr);
        return 8;
    }
    if (!server_state.token.valid()) {
        std::fputs("reactor service close missing deferred token\n", stderr);
        return 9;
    }
    if (!server_state.late_reply_rejected) {
        std::fputs("reactor service close late reply not rejected\n", stderr);
        return 10;
    }
    if (!server_driver.closed() || server_driver.last_error() != net::errc::closed) {
        std::fputs("reactor service close driver state mismatch\n", stderr);
        return 11;
    }
    if (server_session.has_deferred() || server_session.deferred_count() != 0) {
        std::fputs("reactor service close deferred not cleared\n", stderr);
        return 12;
    }
    if (server_session.last_error() != net::errc::closed) {
        std::fputs("reactor service close session error mismatch\n", stderr);
        return 13;
    }

    std::puts("net reactor service close smoke: ok");
    return 0;
}
