#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <cstdio>
#include <thread>
#include <utility>

import charm.net;
import net.backend.win;
import util.core;

namespace {
    using namespace std::chrono_literals;

    [[nodiscard]] bool bytes_eq(net::ByteView lhs, net::ByteView rhs) noexcept {
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

    struct WinsockScope {
        WinsockScope() noexcept {
            WSADATA data{};
            ok = ::WSAStartup(MAKEWORD(2, 2), &data) == 0;
        }

        ~WinsockScope() {
            if (ok) {
                ::WSACleanup();
            }
        }

        bool ok{false};
    };

    struct NativeSocket {
        NativeSocket() noexcept = default;

        explicit NativeSocket(SOCKET socket) noexcept
            : value(socket) {}

        NativeSocket(const NativeSocket&) = delete;
        NativeSocket& operator=(const NativeSocket&) = delete;

        NativeSocket(NativeSocket&& other) noexcept
            : value(other.value) {
            other.value = INVALID_SOCKET;
        }

        NativeSocket& operator=(NativeSocket&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            close();
            value = other.value;
            other.value = INVALID_SOCKET;
            return *this;
        }

        ~NativeSocket() {
            close();
        }

        [[nodiscard]] bool valid() const noexcept {
            return value != INVALID_SOCKET;
        }

        void close() noexcept {
            if (!valid()) {
                return;
            }
            (void)::closesocket(value);
            value = INVALID_SOCKET;
        }

        SOCKET value{INVALID_SOCKET};
    };

    template <class Fn>
    bool wait_until(Fn&& fn, int attempts = 200, std::chrono::milliseconds delay = 2ms) {
        for (int i = 0; i < attempts; ++i) {
            if (fn()) {
                return true;
            }
            std::this_thread::sleep_for(delay);
        }
        return false;
    }

    [[nodiscard]] bool send_all(SOCKET socket,
                                const util::u8* data,
                                util::usize size) noexcept {
        util::usize sent = 0;
        while (sent < size) {
            const int rc = ::send(socket,
                                  reinterpret_cast<const char*>(data + sent),
                                  static_cast<int>(size - sent),
                                  0);
            if (rc <= 0) {
                return false;
            }
            sent += static_cast<util::usize>(rc);
        }
        return true;
    }

    struct CloseRequest {
        util::u8 text[4]{};
    };

    struct CloseReply {
        util::u8 text[4]{};
    };

    using CloseOp = net::TrivialServiceOp<0x70u, CloseRequest, CloseReply>;
    using Session = net::TypedServiceSession<64, 4, 4, 4>;
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
                                   const CloseRequest& request) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return;
            }

            static constexpr util::u8 hold[]{'h', 'o', 'l', 'd'};
            self->saw_request = bytes_eq(net::ByteView{request.text, 4},
                                         net::ByteView{hold, 4});
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
    WinsockScope winsock{};
    if (!winsock.ok) {
        std::fputs("reactor service typed close win winsock startup failed\n", stderr);
        return 1;
    }

    net::backend::WinProvider<16> provider{};
    net::Stack stack{provider};
    io::Reactor reactor{};
    net::SocketPoller<4> poller{reactor};

    net::TcpListener listener{};
    net::TcpClient server{};

    util::u16 port = 0;
    for (util::u16 candidate = 33820; candidate < 33920; ++candidate) {
        if (listener.listen(stack, net::Endpoint::ipv4_loopback(candidate), 2)) {
            port = candidate;
            break;
        }
    }
    if (port == 0) {
        std::fputs("reactor service typed close win listen failed\n", stderr);
        return 2;
    }

    NativeSocket client{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    if (!client.valid()) {
        std::fputs("reactor service typed close win native socket failed\n", stderr);
        return 3;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    if (::connect(client.value,
                  reinterpret_cast<const sockaddr*>(&addr),
                  static_cast<int>(sizeof(addr))) != 0) {
        std::fputs("reactor service typed close win native connect failed\n", stderr);
        return 4;
    }

    net::errc accept_error = net::errc::ok;
    const bool accepted = wait_until([&]() {
        auto result = listener.accept(server, nullptr);
        if (result) {
            return true;
        }
        accept_error = result.error();
        return accept_error != net::errc::would_block;
    });
    if (!accepted || accept_error != net::errc::ok) {
        std::fputs("reactor service typed close win accept failed\n", stderr);
        return 5;
    }

    net::SocketChannelBinding server_binding{server.raw()};
    Session server_session{};
    ServerState server_state{};
    server_session.set_error_handler(&ServerState::on_error, &server_state);

    auto route = server_session.set_deferred_route<CloseOp>(&ServerState::on_close_probe,
                                                            &server_state);
    if (!route) {
        std::fputs("reactor service typed close win route registration failed\n", stderr);
        return 6;
    }

    Driver server_driver{reactor, poller, server_binding, server_session};
    if (!server_driver.start()) {
        std::fputs("reactor service typed close win driver start failed\n", stderr);
        return 7;
    }

    static constexpr util::u8 wire[]{
        0x00u, 0x08u,
        0x00u, 0x01u,
        0x70u, 0x00u,
        'h', 'o', 'l', 'd'
    };
    if (!send_all(client.value, wire, sizeof(wire))) {
        std::fputs("reactor service typed close win native send failed\n", stderr);
        return 8;
    }
    client.close();

    const bool done = wait_until([&]() {
        (void)poller.poll();
        (void)reactor.drain(8);
        return server_state.saw_request
            && server_state.saw_deferred_during_route
            && server_state.token.valid()
            && server_state.error_count == 1
            && server_state.last_error == net::errc::closed;
    });

    CloseReply late_reply{{'l', 'a', 't', 'e'}};
    auto late = server_session.send_deferred_response<CloseOp>(server_state.token, late_reply);
    server_state.late_reply_rejected = !late && late.error() == net::errc::noent;

    server_driver.stop();
    (void)server.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor service typed close win timeout\n", stderr);
        return 9;
    }
    if (!server_state.late_reply_rejected) {
        std::fputs("reactor service typed close win late reply not rejected\n", stderr);
        return 10;
    }
    if (!server_driver.closed() || server_driver.last_error() != net::errc::closed) {
        std::fputs("reactor service typed close win driver state mismatch\n", stderr);
        return 11;
    }
    if (server_session.has_deferred() || server_session.deferred_count() != 0) {
        std::fputs("reactor service typed close win deferred not cleared\n", stderr);
        return 12;
    }
    if (server_session.last_error() != net::errc::closed) {
        std::fputs("reactor service typed close win session error mismatch\n", stderr);
        return 13;
    }

    std::puts("net reactor service typed close win smoke: ok");
    return 0;
}
