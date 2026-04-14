#include <array>
#include <chrono>
#include <cstdio>
#include <thread>

import charm.net;
import net.backend.win;
import net.protocol.diagnostic;
import util.core;

namespace {
    using namespace std::chrono_literals;

    [[nodiscard]] constexpr util::u32 all_reactor_events() noexcept {
        return static_cast<util::u32>(io::Event::readable)
            | static_cast<util::u32>(io::Event::writable)
            | static_cast<util::u32>(io::Event::closed)
            | static_cast<util::u32>(io::Event::error);
    }

    using ProtocolClient = net::diag::Client<>;
    using ProtocolServer = net::diag::Server<>;
    using ClientDriver = net::ReactorSocketDriver<ProtocolClient, 8>;
    using ServerDriver = net::ReactorSocketDriver<ProtocolServer, 8>;

    bool bytes_eq(net::ByteView lhs, net::ByteView rhs) noexcept {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (util::usize i = 0; i < lhs.size(); ++i) {
            if (lhs[i] != rhs[i]) return false;
        }
        return true;
    }

    struct ClientState {
        util::u16 ping_request_id{0};
        util::u16 count_request_id{0};
        util::u16 slow_request_id{0};
        util::u16 meta_request_id{0};
        bool got_ping{false};
        bool got_count{false};
        bool got_slow{false};
        bool got_meta{false};
        bool failed{false};

        static void on_ping(void* ctx,
                            util::u16 request_id,
                            net::ServiceStatus status,
                            const net::diag::PingReply& response) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;

            static constexpr util::u8 want[]{'p', 'o', 'n', 'g'};
            self->got_ping = request_id == self->ping_request_id
                && status == net::ServiceStatus::ok
                && bytes_eq(net::ByteView{response.text, 4},
                            net::ByteView{want, 4});
            if (!self->got_ping) {
                self->failed = true;
            }
        }

        static void on_count(void* ctx,
                             util::u16 request_id,
                             net::ServiceStatus status,
                             const net::diag::CounterValue& response) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;

            self->got_count = request_id == self->count_request_id
                && status == net::ServiceStatus::ok
                && response.value == 7;
            if (!self->got_count) {
                self->failed = true;
            }
        }

        static void on_slow(void* ctx,
                            util::u16 request_id,
                            net::ServiceStatus status,
                            const net::diag::CounterValue& response) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;

            self->got_slow = request_id == self->slow_request_id
                && status == net::ServiceStatus::ok
                && response.value == 42;
            if (!self->got_slow) {
                self->failed = true;
            }
        }

        static void on_meta(void* ctx,
                            util::u16 request_id,
                            net::ServiceStatus status,
                            const net::diag::MetaReply& response) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;

            static constexpr std::array<util::u8, 2> want{'o', 'k'};
            self->got_meta = request_id == self->meta_request_id
                && status == net::ServiceStatus::ok
                && response.status == 0xa5u
                && response.reflected_code == 0x1235u
                && response.tag == want;
            if (!self->got_meta) {
                self->failed = true;
            }
        }

        static void on_timeout(void* ctx, util::u16) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;
            self->failed = true;
        }

        static void on_error(void* ctx, net::errc) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;
            self->failed = true;
        }
    };

    struct ServerState {
        net::ServiceReplyToken slow_token{};
        util::u32 slow_due_ms{0};
        bool slow_pending{false};
        bool saw_ping{false};
        bool saw_count{false};
        bool saw_slow{false};
        bool saw_meta{false};
        bool failed{false};

        static net::ServiceStatus on_ping(void* ctx,
                                          const net::diag::PingRequest& request,
                                          net::diag::PingReply& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return net::ServiceStatus::internal_error;
            }

            static constexpr util::u8 ping[]{'p', 'i', 'n', 'g'};
            static constexpr util::u8 pong[]{'p', 'o', 'n', 'g'};

            self->saw_ping = bytes_eq(net::ByteView{request.text, 4},
                                      net::ByteView{ping, 4});
            if (!self->saw_ping) {
                self->failed = true;
                return net::ServiceStatus::bad_request;
            }

            for (util::usize i = 0; i < 4; ++i) {
                response.text[i] = pong[i];
            }
            return net::ServiceStatus::ok;
        }

        static net::ServiceStatus on_count(void* ctx,
                                           const net::EmptyMessage&,
                                           net::diag::CounterValue& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return net::ServiceStatus::internal_error;
            }

            self->saw_count = true;
            response.value = 7;
            return net::ServiceStatus::ok;
        }

        static void on_slow(void* ctx,
                            ProtocolServer&,
                            net::ServiceReplyToken token,
                            const net::diag::CounterValue& request) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) return;

            self->saw_slow = request.value == 41;
            if (!self->saw_slow) {
                self->failed = true;
                return;
            }

            self->slow_token = token;
            self->slow_due_ms = 30;
            self->slow_pending = true;
        }

        static net::ServiceStatus on_meta(void* ctx,
                                          const net::diag::MetaRequest& request,
                                          net::diag::MetaReply& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return net::ServiceStatus::internal_error;
            }

            static constexpr std::array<util::u8, 2> want{'o', 'k'};
            self->saw_meta = request.code == 0x1234u
                && request.flags == 0x5au
                && request.tag == want;
            if (!self->saw_meta) {
                self->failed = true;
                return net::ServiceStatus::bad_request;
            }

            response.status = 0xa5u;
            response.reflected_code = static_cast<util::u16>(request.code + 1u);
            response.tag = request.tag;
            return net::ServiceStatus::ok;
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
    for (util::u16 candidate = 32400; candidate < 32500; ++candidate) {
        if (!listener.listen_loopback(stack, candidate, 2)) continue;
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor protocol diag listener failed\n", stderr);
        return 1;
    }

    net::SocketEventChannelBinding listener_binding{listener.raw()};
    net::SocketChannelBinding client_binding{client.raw()};
    net::SocketChannelBinding server_binding{server_side.raw()};

    ProtocolClient client_proto{};
    ProtocolServer server_proto{};
    ClientState client_state{};
    ServerState server_state{};

    client_proto.set_error_handler(&ClientState::on_error, &client_state);
    server_proto.set_error_handler(&ServerState::on_error, &server_state);

    auto ping_route = server_proto.on_ping(&ServerState::on_ping, &server_state);
    auto count_route = server_proto.on_count(&ServerState::on_count, &server_state);
    auto slow_route = server_proto.on_slow_count(&ServerState::on_slow, &server_state);
    auto meta_route = server_proto.on_meta(&ServerState::on_meta, &server_state);
    if (!ping_route || !count_route || !slow_route || !meta_route) {
        std::fputs("reactor protocol diag route registration failed\n", stderr);
        return 2;
    }

    ClientDriver client_driver{reactor, socket_poller, client_binding, client_proto};
    ServerDriver server_driver{reactor, socket_poller, server_binding, server_proto};

    ListenerState<ServerDriver> listener_state{};
    listener_state.listener = &listener;
    listener_state.server_socket = &server_side;
    listener_state.server_binding = &server_binding;
    listener_state.server_driver = &server_driver;

    auto listener_sub = reactor.subscribe(listener_binding.channel(),
                                          all_reactor_events(),
                                          &ListenerState<ServerDriver>::on_event,
                                          &listener_state);
    if (!listener_sub) {
        std::fputs("reactor protocol diag listener subscribe failed\n", stderr);
        return 3;
    }

    auto listener_watch = socket_poller.watch(listener.raw(), listener_binding.channel());
    if (!listener_watch) {
        std::fputs("reactor protocol diag listener watch failed\n", stderr);
        return 4;
    }

    if (!client.connect_loopback(stack, port)) {
        std::fputs("reactor protocol diag client connect failed\n", stderr);
        return 5;
    }

    auto client_started = client_driver.start();
    if (!client_started) {
        std::fputs("reactor protocol diag client driver start failed\n", stderr);
        return 6;
    }

    auto ping = client_proto.ping(
        net::diag::PingRequest{{'p', 'i', 'n', 'g'}},
        0,
        200,
        &ClientState::on_ping,
        &ClientState::on_timeout,
        &client_state);
    if (!ping) {
        std::fputs("reactor protocol diag ping request failed\n", stderr);
        return 7;
    }
    client_state.ping_request_id = ping.value();

    auto count = client_proto.query_count(
        0,
        200,
        &ClientState::on_count,
        &ClientState::on_timeout,
        &client_state);
    if (!count) {
        std::fputs("reactor protocol diag count request failed\n", stderr);
        return 8;
    }
    client_state.count_request_id = count.value();

    auto slow = client_proto.query_slow_count(
        net::diag::CounterValue{41},
        0,
        200,
        &ClientState::on_slow,
        &ClientState::on_timeout,
        &client_state);
    if (!slow) {
        std::fputs("reactor protocol diag slow request failed\n", stderr);
        return 9;
    }
    client_state.slow_request_id = slow.value();

    auto meta = client_proto.query_meta(
        net::diag::MetaRequest{0x1234u, 0x5au, {'o', 'k'}},
        0,
        200,
        &ClientState::on_meta,
        &ClientState::on_timeout,
        &client_state);
    if (!meta) {
        std::fputs("reactor protocol diag meta request failed\n", stderr);
        return 10;
    }
    client_state.meta_request_id = meta.value();

    bool done = false;
    for (int i = 0; i < 400; ++i) {
        const util::u32 now_ms = static_cast<util::u32>(i * 2);

        (void)socket_poller.poll();
        (void)reactor.drain(8);
        client_proto.tick(now_ms);
        server_proto.tick(now_ms);

        if (server_state.slow_pending && now_ms >= server_state.slow_due_ms) {
            auto sent = server_proto.reply_slow_count(
                server_state.slow_token,
                net::diag::CounterValue{42});
            if (!sent) {
                std::fputs("reactor protocol diag slow response failed\n", stderr);
                return 11;
            }
            server_state.slow_pending = false;
        }

        if (listener_state.failed || client_state.failed || server_state.failed) {
            std::fputs("reactor protocol diag failed\n", stderr);
            return 12;
        }

        done = listener_state.accepted
            && server_state.saw_ping
            && server_state.saw_count
            && server_state.saw_slow
            && server_state.saw_meta
            && client_state.got_ping
            && client_state.got_count
            && client_state.got_slow
            && client_state.got_meta;
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
        std::fputs("reactor protocol diag timeout\n", stderr);
        return 13;
    }

    std::puts("net reactor protocol diag smoke: ok");
    return 0;
}
