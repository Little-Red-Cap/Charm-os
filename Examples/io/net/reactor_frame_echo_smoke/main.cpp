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

        void on_frame(net::ByteView payload) noexcept {
            static constexpr util::u8 expected[]{'p', 'o', 'n', 'g'};
            received_response = bytes_eq(payload, net::ByteView{expected, 4});
            if (!received_response) {
                failed = true;
            }
        }

        void on_error(net::errc) noexcept {
            failed = true;
        }
    };

    struct ServerState {
        net::FrameSession<64>* session{nullptr};
        bool received_request{false};
        bool sent_response{false};
        bool failed{false};

        void on_frame(net::ByteView payload) noexcept {
            if (!session) return;

            static constexpr util::u8 request[]{'p', 'i', 'n', 'g'};
            static constexpr util::u8 response[]{'p', 'o', 'n', 'g'};
            if (!bytes_eq(payload, net::ByteView{request, 4})) {
                failed = true;
                return;
            }

            received_request = true;
            auto sent = session->send_frame(net::ByteView{response, 4});
            if (!sent) {
                failed = true;
                return;
            }
            sent_response = true;
        }

        void on_error(net::errc) noexcept {
            failed = true;
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
        auto listening = net::TcpListener::listening_loopback(stack, candidate, 2);
        if (!listening) continue;
        listener = std::move(listening.value());
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor frame listener failed\n", stderr);
        return 1;
    }

    net::SocketChannelBinding client_binding{};
    net::SocketChannelBinding server_binding{server_side.raw()};

    net::FrameSession<64> client_session{};
    net::FrameSession<64> server_session{};
    ClientState client_state{};
    ServerState server_state{};

    server_state.session = &server_session;
    client_session.set_frame_handler(net::FrameHandlerRef::bind(client_state));
    client_session.set_error_handler(net::FrameErrorHandlerRef::bind(client_state));
    server_session.set_frame_handler(net::FrameHandlerRef::bind(server_state));
    server_session.set_error_handler(net::FrameErrorHandlerRef::bind(server_state));

    using DriverType = net::ReactorSocketDriver<net::FrameSession<64>, 8>;
    DriverType client_driver{reactor, socket_poller, client_binding, client_session};
    DriverType server_driver{reactor, socket_poller, server_binding, server_session};
    net::TcpSingleAcceptDriver<DriverType, 8> listener_driver{
        reactor, socket_poller, listener, server_side, server_driver};

    auto listener_started = listener_driver.start(all_reactor_events());
    if (!listener_started) {
        std::fputs("reactor frame listener driver start failed\n", stderr);
        return 2;
    }

    auto connected = net::TcpClient::connected_loopback(stack, port);
    if (!connected) {
        std::fputs("reactor frame client connect failed\n", stderr);
        return 3;
    }
    client = std::move(connected.value());
    client_binding.bind(client.raw());

    auto client_started = client_driver.start();
    if (!client_started) {
        std::fputs("reactor frame client driver start failed\n", stderr);
        return 4;
    }

    static constexpr util::u8 request[]{'p', 'i', 'n', 'g'};
    auto request_sent = client_session.send_frame(net::ByteView{request, 4});
    if (!request_sent) {
        std::fputs("reactor frame client send failed\n", stderr);
        return 5;
    }
    client_state.sent_request = true;

    bool done = false;
    for (int i = 0; i < 400; ++i) {
        (void)socket_poller.poll();
        (void)reactor.drain(8);

        if (listener_driver.failed() || client_state.failed || server_state.failed) {
            std::fputs("reactor frame protocol failed\n", stderr);
            return 6;
        }

        done = listener_driver.accepted()
            && client_state.sent_request
            && server_state.received_request
            && server_state.sent_response
            && client_state.received_response;
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
        std::fputs("reactor frame protocol timeout\n", stderr);
        return 7;
    }

    std::puts("net reactor frame echo smoke: ok");
    return 0;
}
