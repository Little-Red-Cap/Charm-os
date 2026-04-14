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

    using Session = net::ServiceSession<64, 4, 8>;
    using Driver = net::ReactorSocketDriver<Session, 8>;

    struct ClientState {
        util::u16 slow_request_id{0};
        util::u16 cancel_request_id{0};
        util::u16 abort_request_id{0};
        bool cancel_issued{false};
        bool got_slow_response{false};
        bool got_cancel_response{false};
        bool got_abort_timeout{false};
        bool failed{false};

        static void on_response(void* ctx,
                                util::u16 request_id,
                                util::u8 opcode,
                                net::ServiceStatus status,
                                net::ByteView payload) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;

            static constexpr util::u8 done[]{'d', 'o', 'n', 'e'};

            if (request_id == self->slow_request_id) {
                self->got_slow_response = opcode == 0x40u
                    && status == net::ServiceStatus::ok
                    && bytes_eq(payload, net::ByteView{done, 4});
                if (!self->got_slow_response) {
                    self->failed = true;
                }
                return;
            }

            if (request_id == self->cancel_request_id) {
                self->got_cancel_response = true;
                self->failed = true;
                return;
            }

            if (request_id == self->abort_request_id) {
                self->failed = true;
                return;
            }

            self->failed = true;
        }

        static void on_timeout(void* ctx,
                               util::u16 request_id,
                               util::u8 opcode) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;
            self->got_abort_timeout = request_id == self->abort_request_id && opcode == 0x42u;
            if (!self->got_abort_timeout) {
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
        net::ServiceReplyToken slow_token{};
        net::ServiceReplyToken cancel_token{};
        net::ServiceReplyToken abort_token{};
        util::u32 slow_due_ms{0};
        util::u32 cancel_due_ms{0};
        util::u32 abort_due_ms{0};
        bool slow_pending{false};
        bool cancel_pending{false};
        bool abort_pending{false};
        bool slow_sent{false};
        bool cancel_sent{false};
        bool abort_canceled{false};
        bool saw_slow_route{false};
        bool saw_cancel_route{false};
        bool saw_abort_route{false};
        bool failed{false};

        static void on_slow(void* ctx,
                            Session& session,
                            net::ServiceReplyToken token,
                            net::ByteView request) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) return;

            static constexpr util::u8 slow[]{'s', 'l', 'o', 'w'};

            self->saw_slow_route = true;
            if (!bytes_eq(request, net::ByteView{slow, 4})) {
                self->failed = true;
                (void)session.send_deferred_error(token, net::ServiceStatus::bad_request);
                return;
            }

            self->slow_token = token;
            self->slow_due_ms = 30;
            self->slow_pending = true;
        }

        static void on_cancel(void* ctx,
                              Session& session,
                              net::ServiceReplyToken token,
                              net::ByteView request) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) return;

            static constexpr util::u8 hold[]{'h', 'o', 'l', 'd'};

            self->saw_cancel_route = true;
            if (!bytes_eq(request, net::ByteView{hold, 4})) {
                self->failed = true;
                (void)session.send_deferred_error(token, net::ServiceStatus::bad_request);
                return;
            }

            self->cancel_token = token;
            self->cancel_due_ms = 60;
            self->cancel_pending = true;
        }

        static void on_abort(void* ctx,
                             Session& session,
                             net::ServiceReplyToken token,
                             net::ByteView request) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) return;

            static constexpr util::u8 wait[]{'w', 'a', 'i', 't'};

            self->saw_abort_route = true;
            if (!bytes_eq(request, net::ByteView{wait, 4})) {
                self->failed = true;
                (void)session.send_deferred_error(token, net::ServiceStatus::bad_request);
                return;
            }

            self->abort_token = token;
            self->abort_due_ms = 26;
            self->abort_pending = true;
        }

        static void on_error(void* ctx, net::errc) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) return;
            self->failed = true;
        }
    };

    template <class DriverType>
    struct ListenerState {
        net::TcpListener* listener{nullptr};
        net::TcpClient* server_socket{nullptr};
        net::SocketChannelBinding* server_binding{nullptr};
        DriverType* server_driver{nullptr};
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
    for (util::u16 candidate = 32200; candidate < 32300; ++candidate) {
        if (!listener.listen(stack, net::Endpoint::ipv4_loopback(candidate), 2)) continue;
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor service deferred listener failed\n", stderr);
        return 1;
    }

    net::SocketEventChannelBinding listener_binding{listener.raw()};
    net::SocketChannelBinding client_binding{client.raw()};
    net::SocketChannelBinding server_binding{server_side.raw()};

    Session client_session{};
    Session server_session{};
    ClientState client_state{};
    ServerState server_state{};

    client_session.set_error_handler(&ClientState::on_error, &client_state);
    server_session.set_error_handler(&ServerState::on_error, &server_state);

    auto slow_route = server_session.set_deferred_route(0x40u, &ServerState::on_slow, &server_state);
    auto cancel_route = server_session.set_deferred_route(0x41u, &ServerState::on_cancel, &server_state);
    auto abort_route = server_session.set_deferred_route(0x42u, &ServerState::on_abort, &server_state);
    if (!slow_route || !cancel_route || !abort_route) {
        std::fputs("reactor service deferred route registration failed\n", stderr);
        return 2;
    }

    Driver client_driver{reactor, socket_poller, client_binding, client_session};
    Driver server_driver{reactor, socket_poller, server_binding, server_session};

    ListenerState<Driver> listener_state{};
    listener_state.listener = &listener;
    listener_state.server_socket = &server_side;
    listener_state.server_binding = &server_binding;
    listener_state.server_driver = &server_driver;

    auto listener_sub = reactor.subscribe(listener_binding.channel(), all_reactor_events(), &ListenerState<Driver>::on_event, &listener_state);
    if (!listener_sub) {
        std::fputs("reactor service deferred listener subscribe failed\n", stderr);
        return 3;
    }

    auto listener_watch = socket_poller.watch(listener.raw(), listener_binding.channel());
    if (!listener_watch) {
        std::fputs("reactor service deferred listener watch failed\n", stderr);
        return 4;
    }

    if (!client.connect(stack, net::Endpoint::ipv4_loopback(port))) {
        std::fputs("reactor service deferred client connect failed\n", stderr);
        return 5;
    }

    auto client_started = client_driver.start();
    if (!client_started) {
        std::fputs("reactor service deferred client driver start failed\n", stderr);
        return 6;
    }

    static constexpr util::u8 slow[]{'s', 'l', 'o', 'w'};
    static constexpr util::u8 hold[]{'h', 'o', 'l', 'd'};
    static constexpr util::u8 wait[]{'w', 'a', 'i', 't'};
    static constexpr util::u8 done[]{'d', 'o', 'n', 'e'};
    static constexpr util::u8 late[]{'l', 'a', 't', 'e'};

    auto slow_request = client_session.send_request(
        0x40u,
        net::ByteView{slow, 4},
        0,
        200,
        &ClientState::on_response,
        &ClientState::on_timeout,
        &client_state);
    if (!slow_request) {
        std::fputs("reactor service deferred slow request failed\n", stderr);
        return 7;
    }
    client_state.slow_request_id = slow_request.value();

    auto cancel_request = client_session.send_request(
        0x41u,
        net::ByteView{hold, 4},
        0,
        200,
        &ClientState::on_response,
        &ClientState::on_timeout,
        &client_state);
    if (!cancel_request) {
        std::fputs("reactor service deferred cancel request failed\n", stderr);
        return 8;
    }
    client_state.cancel_request_id = cancel_request.value();

    auto abort_request = client_session.send_request(
        0x42u,
        net::ByteView{wait, 4},
        0,
        60,
        &ClientState::on_response,
        &ClientState::on_timeout,
        &client_state);
    if (!abort_request) {
        std::fputs("reactor service deferred abort request failed\n", stderr);
        return 9;
    }
    client_state.abort_request_id = abort_request.value();

    int settle_left = -1;
    bool done_ok = false;
    for (int i = 0; i < 500; ++i) {
        const util::u32 now_ms = static_cast<util::u32>(i * 2);

        (void)socket_poller.poll();
        (void)reactor.drain(8);
        client_session.tick(now_ms);
        server_session.tick(now_ms);

        if (!client_state.cancel_issued && now_ms >= 20) {
            if (!client_session.cancel_request(client_state.cancel_request_id)) {
                std::fputs("reactor service deferred cancel_request failed\n", stderr);
                return 10;
            }
            client_state.cancel_issued = true;
        }

        if (server_state.abort_pending && now_ms >= server_state.abort_due_ms) {
            if (!server_session.cancel_deferred(server_state.abort_token)) {
                std::fputs("reactor service deferred cancel_deferred failed\n", stderr);
                return 11;
            }
            server_state.abort_pending = false;
            server_state.abort_canceled = true;
        }

        if (server_state.slow_pending && now_ms >= server_state.slow_due_ms) {
            auto sent = server_session.send_deferred_response(server_state.slow_token,
                                                              net::ServiceStatus::ok,
                                                              net::ByteView{done, 4});
            if (!sent) {
                std::fputs("reactor service deferred slow response failed\n", stderr);
                return 12;
            }
            server_state.slow_pending = false;
            server_state.slow_sent = true;
        }

        if (server_state.cancel_pending
            && client_state.cancel_issued
            && now_ms >= server_state.cancel_due_ms) {
            auto sent = server_session.send_deferred_response(server_state.cancel_token,
                                                              net::ServiceStatus::ok,
                                                              net::ByteView{late, 4});
            if (!sent) {
                std::fputs("reactor service deferred late response failed\n", stderr);
                return 13;
            }
            server_state.cancel_pending = false;
            server_state.cancel_sent = true;
        }

        if (listener_state.failed || client_state.failed || server_state.failed) {
            std::fputs("reactor service deferred protocol failed\n", stderr);
            return 14;
        }

        const bool ready_to_settle = listener_state.accepted
            && client_state.cancel_issued
            && server_state.saw_slow_route
            && server_state.saw_cancel_route
            && server_state.saw_abort_route
            && server_state.abort_canceled
            && server_state.slow_sent
            && server_state.cancel_sent
            && client_state.got_slow_response
            && client_state.got_abort_timeout
            && !client_state.got_cancel_response
            && client_session.pending_count() == 0
            && !server_session.has_deferred();

        if (ready_to_settle && settle_left < 0) {
            settle_left = 12;
        }

        if (settle_left >= 0) {
            --settle_left;
            if (settle_left == 0) {
                done_ok = true;
                break;
            }
        }

        std::this_thread::sleep_for(2ms);
    }

    client_driver.stop();
    server_driver.stop();
    socket_poller.unwatch(listener_watch.value());
    reactor.unsubscribe(listener_sub.value());

    (void)client.close();
    (void)server_side.close();
    (void)listener.close();

    if (!done_ok) {
        std::fputs("reactor service deferred protocol timeout\n", stderr);
        return 15;
    }

    std::puts("net reactor service deferred smoke: ok");
    return 0;
}
