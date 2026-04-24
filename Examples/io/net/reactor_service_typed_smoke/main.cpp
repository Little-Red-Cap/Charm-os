#include <array>
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

    struct PingRequest {
        util::u8 text[4]{};
    };

    struct PingReply {
        util::u8 text[4]{};
    };

    struct CounterValue {
        util::u16 value{0};
    };

    struct MetaRequest {
        util::u16 code{0};
        util::u8 flags{0};
        std::array<util::u8, 2> tag{};
    };

    struct MetaReply {
        util::u8 status{0};
        util::u16 reflected_code{0};
        std::array<util::u8, 2> tag{};
    };

    using PingOp = net::TrivialServiceOp<0x60u, PingRequest, PingReply>;

    using CountOp = net::WireServiceOp<
        0x61u,
        net::EmptyMessage,
        CounterValue,
        net::WireMembers<>,
        net::WireMembers<&CounterValue::value>>;

    using SlowCountOp = net::WireServiceOp<
        0x62u,
        CounterValue,
        CounterValue,
        net::WireMembers<&CounterValue::value>,
        net::WireMembers<&CounterValue::value>>;

    using MetaOp = net::WireServiceOp<
        0x63u,
        MetaRequest,
        MetaReply,
        net::WireMembers<
            &MetaRequest::code,
            &MetaRequest::flags,
            &MetaRequest::tag>,
        net::WireMembers<
            &MetaReply::status,
            &MetaReply::reflected_code,
            &MetaReply::tag>>;

    using Session = net::TypedServiceSession<64, 4, 8>;
    using Driver = net::ReactorSocketDriver<Session, 8>;

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
                            const PingReply& response) noexcept {
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
                             const CounterValue& response) noexcept {
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
                            const CounterValue& response) noexcept {
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
                            const MetaReply& response) noexcept {
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
                                          const PingRequest& request,
                                          PingReply& response) noexcept {
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
                                           CounterValue& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return net::ServiceStatus::internal_error;
            }

            self->saw_count = true;
            response.value = 7;
            return net::ServiceStatus::ok;
        }

        static void on_slow(void* ctx,
                            Session&,
                            net::ServiceReplyToken token,
                            const CounterValue& request) noexcept {
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
                                          const MetaRequest& request,
                                          MetaReply& response) noexcept {
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
    for (util::u16 candidate = 32300; candidate < 32400; ++candidate) {
        auto listening = net::TcpListener::listening_loopback(stack, candidate, 2);
        if (!listening) continue;
        listener = std::move(listening.value());
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor service typed listener failed\n", stderr);
        return 1;
    }

    net::SocketChannelBinding client_binding{};
    net::SocketChannelBinding server_binding{server_side.raw()};

    Session client_session{};
    Session server_session{};
    ClientState client_state{};
    ServerState server_state{};

    client_session.set_error_handler(&ClientState::on_error, &client_state);
    server_session.set_error_handler(&ServerState::on_error, &server_state);

    auto ping_route = server_session.set_route<PingOp>(&ServerState::on_ping, &server_state);
    auto count_route = server_session.set_route<CountOp>(&ServerState::on_count, &server_state);
    auto slow_route = server_session.set_deferred_route<SlowCountOp>(&ServerState::on_slow, &server_state);
    auto meta_route = server_session.set_route<MetaOp>(&ServerState::on_meta, &server_state);
    if (!ping_route || !count_route || !slow_route || !meta_route) {
        std::fputs("reactor service typed route registration failed\n", stderr);
        return 2;
    }

    Driver client_driver{reactor, socket_poller, client_binding, client_session};
    Driver server_driver{reactor, socket_poller, server_binding, server_session};
    net::TcpSingleAcceptDriver<Driver, 8> listener_driver{
        reactor, socket_poller, listener, server_side, server_driver};

    auto listener_started = listener_driver.start(all_reactor_events());
    if (!listener_started) {
        std::fputs("reactor service typed listener driver start failed\n", stderr);
        return 3;
    }

    auto connected = net::TcpClient::connected_loopback(stack, port);
    if (!connected) {
        std::fputs("reactor service typed client connect failed\n", stderr);
        return 4;
    }
    client = std::move(connected.value());
    client_binding.bind(client.raw());

    auto client_started = client_driver.start();
    if (!client_started) {
        std::fputs("reactor service typed client driver start failed\n", stderr);
        return 5;
    }

    PingRequest ping_request{{'p', 'i', 'n', 'g'}};
    auto ping = client_session.send_request<PingOp>(
        ping_request,
        0,
        200,
        &ClientState::on_ping,
        &ClientState::on_timeout,
        &client_state);
    if (!ping) {
        std::fputs("reactor service typed ping request failed\n", stderr);
        return 6;
    }
    client_state.ping_request_id = ping.value();

    auto count = client_session.send_request<CountOp>(
        net::EmptyMessage{},
        0,
        200,
        &ClientState::on_count,
        &ClientState::on_timeout,
        &client_state);
    if (!count) {
        std::fputs("reactor service typed count request failed\n", stderr);
        return 7;
    }
    client_state.count_request_id = count.value();

    auto slow = client_session.send_request<SlowCountOp>(
        CounterValue{41},
        0,
        200,
        &ClientState::on_slow,
        &ClientState::on_timeout,
        &client_state);
    if (!slow) {
        std::fputs("reactor service typed slow request failed\n", stderr);
        return 8;
    }
    client_state.slow_request_id = slow.value();

    auto meta = client_session.send_request<MetaOp>(
        MetaRequest{0x1234u, 0x5au, {'o', 'k'}},
        0,
        200,
        &ClientState::on_meta,
        &ClientState::on_timeout,
        &client_state);
    if (!meta) {
        std::fputs("reactor service typed meta request failed\n", stderr);
        return 9;
    }
    client_state.meta_request_id = meta.value();

    bool done = false;
    for (int i = 0; i < 400; ++i) {
        const util::u32 now_ms = static_cast<util::u32>(i * 2);

        (void)socket_poller.poll();
        (void)reactor.drain(8);
        client_session.tick(now_ms);
        server_session.tick(now_ms);

        if (server_state.slow_pending && now_ms >= server_state.slow_due_ms) {
            auto sent = server_session.send_deferred_response<SlowCountOp>(
                server_state.slow_token,
                CounterValue{42});
            if (!sent) {
                std::fputs("reactor service typed slow response failed\n", stderr);
                return 10;
            }
            server_state.slow_pending = false;
        }

        if (listener_driver.failed() || client_state.failed || server_state.failed) {
            std::fputs("reactor service typed protocol failed\n", stderr);
            return 11;
        }

        done = listener_driver.accepted()
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
    listener_driver.stop();

    (void)client.close();
    (void)server_side.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor service typed protocol timeout\n", stderr);
        return 12;
    }

    std::puts("net reactor service typed smoke: ok");
    return 0;
}
