#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <thread>
#include <utility>

import charm.net;
import net.backend.win;
import util.core;

namespace {
    using namespace std::chrono_literals;

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

    [[nodiscard]] bool set_nonblocking(SOCKET socket) noexcept {
        u_long mode = 1;
        return ::ioctlsocket(socket, FIONBIO, &mode) == 0;
    }

    struct ClientState {
        util::u16 request_id{0};
        int response_count{0};
        int timeout_count{0};
        int error_count{0};
        net::errc last_error{net::errc::ok};

        static void on_response(void* ctx,
                                util::u16 request_id,
                                util::u8 opcode,
                                net::ServiceStatus,
                                net::ByteView) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }
            ++self->response_count;
            if (request_id != self->request_id || opcode != 0x73u) {
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
            if (request_id != self->request_id || opcode != 0x73u) {
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

    [[nodiscard]] bool recv_request_wire(SOCKET socket,
                                         std::array<util::u8, 10>& wire,
                                         int& filled,
                                         int& native_error) noexcept {
        while (filled < static_cast<int>(wire.size())) {
            const int rc = ::recv(socket,
                                  reinterpret_cast<char*>(wire.data() + filled),
                                  static_cast<int>(wire.size()) - filled,
                                  0);
            if (rc > 0) {
                filled += rc;
                continue;
            }
            if (rc == 0) {
                native_error = 0;
                return true;
            }

            native_error = ::WSAGetLastError();
            if (native_error == WSAEWOULDBLOCK) {
                native_error = 0;
                return false;
            }
            return true;
        }
        return true;
    }

    using Session = net::ServiceSession<64, 4, 4, 4>;
    using Driver = net::ReactorSocketDriver<Session, 4>;
}

int main() {
    WinsockScope winsock{};
    if (!winsock.ok) {
        std::fputs("reactor service request close win winsock startup failed\n", stderr);
        return 1;
    }

    NativeSocket listener{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    if (!listener.valid()) {
        std::fputs("reactor service request close win native listen socket failed\n", stderr);
        return 2;
    }

    if (!set_nonblocking(listener.value)) {
        std::fputs("reactor service request close win native listen nonblocking failed\n", stderr);
        return 3;
    }

    BOOL reuse = 1;
    (void)::setsockopt(listener.value,
                       SOL_SOCKET,
                       SO_REUSEADDR,
                       reinterpret_cast<const char*>(&reuse),
                       static_cast<int>(sizeof(reuse)));

    util::u16 port = 0;
    for (util::u16 candidate = 33620; candidate < 33720; ++candidate) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = ::htons(candidate);
        addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);

        if (::bind(listener.value,
                   reinterpret_cast<const sockaddr*>(&addr),
                   static_cast<int>(sizeof(addr))) != 0) {
            continue;
        }
        if (::listen(listener.value, 2) != 0) {
            std::fputs("reactor service request close win native listen failed\n", stderr);
            return 4;
        }
        port = candidate;
        break;
    }
    if (port == 0) {
        std::fputs("reactor service request close win native bind failed\n", stderr);
        return 5;
    }

    net::backend::WinProvider<16> provider{};
    net::Stack stack{provider};
    io::Reactor reactor{};
    net::SocketPoller<4> poller{reactor};

    net::TcpClient client{};
    auto connected = net::TcpClient::connected_loopback(stack, port);
    if (!connected) {
        std::fputs("reactor service request close win charm client connect failed\n", stderr);
        return 6;
    }
    client = std::move(connected.value());

    NativeSocket server{};
    int accept_error = 0;
    const bool accepted = wait_until([&]() {
        SOCKET accepted_socket = ::accept(listener.value, nullptr, nullptr);
        if (accepted_socket == INVALID_SOCKET) {
            const int wsa = ::WSAGetLastError();
            if (wsa == WSAEWOULDBLOCK) {
                return false;
            }
            accept_error = wsa;
            return true;
        }
        server = NativeSocket{accepted_socket};
        if (!set_nonblocking(server.value)) {
            accept_error = -1;
            return true;
        }
        return true;
    });
    if (!accepted || accept_error != 0 || !server.valid()) {
        std::fputs("reactor service request close win native accept failed\n", stderr);
        return 7;
    }

    net::SocketChannelBinding client_binding{};
    client_binding.bind(client.raw());
    Session client_session{};
    ClientState client_state{};
    client_session.set_error_handler(&ClientState::on_error, &client_state);

    Driver client_driver{reactor, poller, client_binding, client_session};
    if (!client_driver.start()) {
        std::fputs("reactor service request close win driver start failed\n", stderr);
        return 8;
    }

    static constexpr util::u8 hold[]{'h', 'o', 'l', 'd'};
    auto request = client_session.send_request(0x73u,
                                               net::ByteView{hold, 4},
                                               0,
                                               1000,
                                               &ClientState::on_response,
                                               &ClientState::on_timeout,
                                               &client_state);
    if (!request) {
        std::fputs("reactor service request close win send_request failed\n", stderr);
        return 9;
    }
    client_state.request_id = request.value();

    std::array<util::u8, 10> wire{};
    int wire_filled = 0;
    int native_error = 0;
    const bool got_wire = wait_until([&]() {
        (void)poller.poll();
        (void)reactor.drain(8);
        return recv_request_wire(server.value, wire, wire_filled, native_error);
    });
    if (!got_wire || native_error != 0 || wire_filled != static_cast<int>(wire.size())) {
        std::fputs("reactor service request close win native recv failed\n", stderr);
        return 10;
    }
    if (wire[0] != 0x00u
        || wire[1] != 0x08u
        || wire[2] != 0x00u
        || wire[3] != 0x01u
        || wire[4] != 0x73u
        || wire[5] != 0x00u
        || wire[6] != 'h'
        || wire[7] != 'o'
        || wire[8] != 'l'
        || wire[9] != 'd') {
        std::fputs("reactor service request close win wire mismatch\n", stderr);
        return 11;
    }

    server.close();

    const bool done = wait_until([&]() {
        (void)poller.poll();
        (void)reactor.drain(8);
        return client_state.error_count == 1
            && client_state.last_error == net::errc::closed;
    });

    auto retried = client_session.send_request(0x73u,
                                               net::ByteView{hold, 4},
                                               1,
                                               1000,
                                               &ClientState::on_response,
                                               &ClientState::on_timeout,
                                               &client_state);

    client_driver.stop();
    listener.close();
    (void)client.close();

    if (!done) {
        std::fputs("reactor service request close win timeout\n", stderr);
        return 12;
    }
    if (client_state.response_count != 0) {
        std::fputs("reactor service request close win unexpected response\n", stderr);
        return 13;
    }
    if (client_state.timeout_count != 0) {
        std::fputs("reactor service request close win unexpected timeout\n", stderr);
        return 14;
    }
    if (retried || retried.error() != net::errc::closed) {
        std::fputs("reactor service request close win retry not rejected\n", stderr);
        return 15;
    }
    if (client_session.has_pending() || client_session.pending_count() != 0) {
        std::fputs("reactor service request close win pending not cleared\n", stderr);
        return 16;
    }
    if (client_session.last_error() != net::errc::closed) {
        std::fputs("reactor service request close win session error mismatch\n", stderr);
        return 17;
    }
    if (!client_driver.closed() || client_driver.last_error() != net::errc::closed) {
        std::fputs("reactor service request close win driver state mismatch\n", stderr);
        return 18;
    }

    std::puts("net reactor service request close win smoke: ok");
    return 0;
}
