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

    struct ClientState {
        bool sent_request{false};
        bool received_response{false};
        bool failed{false};

        static void on_frame(void* ctx, net::ByteView payload) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) return;
            static constexpr util::u8 expected[]{'p', 'o', 'n', 'g'};
            self->received_response = bytes_eq(payload, net::ByteView{expected, 4});
            if (!self->received_response) {
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
        net::FrameSession<64>* session{nullptr};
        bool received_request{false};
        bool sent_response{false};
        bool failed{false};

        static void on_frame(void* ctx, net::ByteView payload) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self || !self->session) return;

            static constexpr util::u8 request[]{'p', 'i', 'n', 'g'};
            static constexpr util::u8 response[]{'p', 'o', 'n', 'g'};
            if (!bytes_eq(payload, net::ByteView{request, 4})) {
                self->failed = true;
                return;
            }

            self->received_request = true;
            auto sent = self->session->send_frame(net::ByteView{response, 4});
            if (!sent) {
                self->failed = true;
                return;
            }
            self->sent_response = true;
        }

        static void on_error(void* ctx, net::errc) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) return;
            self->failed = true;
        }
    };

    template <class Driver>
    struct ListenerState {
        net::TcpListener* listener{nullptr};
        net::TcpClient* server_socket{nullptr};
        net::SocketChannelBinding* server_binding{nullptr};
        Driver* server_driver{nullptr};
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
    for (util::u16 candidate = 31000; candidate < 31100; ++candidate) {
        if (!listener.listen(stack, net::Endpoint::ipv4_loopback(candidate), 2)) continue;
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor frame listener failed\n", stderr);
        return 1;
    }

    net::SocketEventChannelBinding listener_binding{listener.raw()};
    net::SocketChannelBinding client_binding{client.raw()};
    net::SocketChannelBinding server_binding{server_side.raw()};

    net::FrameSession<64> client_session{};
    net::FrameSession<64> server_session{};
    ClientState client_state{};
    ServerState server_state{};

    server_state.session = &server_session;
    client_session.set_frame_handler(&ClientState::on_frame, &client_state);
    client_session.set_error_handler(&ClientState::on_error, &client_state);
    server_session.set_frame_handler(&ServerState::on_frame, &server_state);
    server_session.set_error_handler(&ServerState::on_error, &server_state);

    using DriverType = net::ReactorSocketDriver<net::FrameSession<64>, 8>;
    DriverType client_driver{reactor, socket_poller, client_binding, client_session};
    DriverType server_driver{reactor, socket_poller, server_binding, server_session};

    ListenerState<DriverType> listener_state{};
    listener_state.listener = &listener;
    listener_state.server_socket = &server_side;
    listener_state.server_binding = &server_binding;
    listener_state.server_driver = &server_driver;

    auto listener_sub = reactor.subscribe(listener_binding.channel(), all_reactor_events(), &ListenerState<DriverType>::on_event, &listener_state);
    if (!listener_sub) {
        std::fputs("reactor frame listener subscribe failed\n", stderr);
        return 2;
    }

    auto listener_watch = socket_poller.watch(listener.raw(), listener_binding.channel());
    if (!listener_watch) {
        std::fputs("reactor frame listener watch failed\n", stderr);
        return 3;
    }

    if (!client.connect(stack, net::Endpoint::ipv4_loopback(port))) {
        std::fputs("reactor frame client connect failed\n", stderr);
        return 4;
    }

    auto client_started = client_driver.start();
    if (!client_started) {
        std::fputs("reactor frame client driver start failed\n", stderr);
        return 5;
    }

    static constexpr util::u8 request[]{'p', 'i', 'n', 'g'};
    auto request_sent = client_session.send_frame(net::ByteView{request, 4});
    if (!request_sent) {
        std::fputs("reactor frame client send failed\n", stderr);
        return 6;
    }
    client_state.sent_request = true;

    bool done = false;
    for (int i = 0; i < 400; ++i) {
        (void)socket_poller.poll();
        (void)reactor.drain(8);

        if (listener_state.failed || client_state.failed || server_state.failed) {
            std::fputs("reactor frame protocol failed\n", stderr);
            return 7;
        }

        done = listener_state.accepted
            && client_state.sent_request
            && server_state.received_request
            && server_state.sent_response
            && client_state.received_response;
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
        std::fputs("reactor frame protocol timeout\n", stderr);
        return 8;
    }

    std::puts("net reactor frame echo smoke: ok");
    return 0;
}
