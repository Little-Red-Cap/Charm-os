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

    [[nodiscard]] bool bytes_eq(const util::u8* lhs,
                                const util::u8* rhs,
                                util::usize count) noexcept {
        for (util::usize i = 0; i < count; ++i) {
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

    struct ProbeSession {
        void set_sender(net::StreamSendFn fn, void* ctx) noexcept {
            send_ = fn;
            send_ctx_ = ctx;
        }

        void feed(net::ByteView data) noexcept {
            const auto remaining = sizeof(rx_) - rx_len_;
            const auto copy = data.size() < remaining ? data.size() : remaining;
            for (util::usize i = 0; i < copy; ++i) {
                rx_[rx_len_ + i] = data[i];
            }
            rx_len_ += copy;
        }

        void notify_writable() noexcept {}

        void on_transport_closed() noexcept {
            ++closed_count_;
        }

        void on_transport_error(net::errc error) noexcept {
            ++error_count_;
            last_error_ = error;
        }

        [[nodiscard]] bool got_ping() const noexcept {
            return rx_len_ == 4
                && bytes_eq(rx_, reinterpret_cast<const util::u8*>("ping"), 4);
        }

        net::StreamSendFn send_{nullptr};
        void* send_ctx_{nullptr};
        util::u8 rx_[8]{};
        util::usize rx_len_{0};
        int closed_count_{0};
        int error_count_{0};
        net::errc last_error_{net::errc::ok};
    };
}

int main() {
    WinsockScope winsock{};
    if (!winsock.ok) {
        std::fputs("reactor close drain win winsock startup failed\n", stderr);
        return 1;
    }

    net::backend::WinProvider<16> provider{};
    net::Stack stack{provider};
    io::Reactor reactor{};
    net::SocketPoller<4> poller{reactor};

    net::TcpListener listener{};
    net::TcpClient server{};

    util::u16 port = 0;
    for (util::u16 candidate = 33320; candidate < 33420; ++candidate) {
        if (listener.listen(stack, net::Endpoint::ipv4_loopback(candidate), 2)) {
            port = candidate;
            break;
        }
    }
    if (port == 0) {
        std::fputs("reactor close drain win listen failed\n", stderr);
        return 2;
    }

    NativeSocket client{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    if (!client.valid()) {
        std::fputs("reactor close drain win native socket failed\n", stderr);
        return 3;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    if (::connect(client.value,
                  reinterpret_cast<const sockaddr*>(&addr),
                  static_cast<int>(sizeof(addr))) != 0) {
        std::fputs("reactor close drain win native connect failed\n", stderr);
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
        std::fputs("reactor close drain win accept failed\n", stderr);
        return 5;
    }

    net::SocketChannelBinding server_binding{server.raw()};
    ProbeSession session{};
    using Driver = net::ReactorSocketDriver<ProbeSession, 4>;
    Driver driver{reactor, poller, server_binding, session};
    if (!driver.start()) {
        std::fputs("reactor close drain win driver start failed\n", stderr);
        return 6;
    }

    const util::u8 ping[4]{'p', 'i', 'n', 'g'};
    if (!send_all(client.value, ping, 4)) {
        std::fputs("reactor close drain win native send failed\n", stderr);
        return 7;
    }
    client.close();

    const bool done = wait_until([&]() {
        (void)poller.poll();
        (void)reactor.drain(8);
        return session.got_ping() && session.closed_count_ == 1;
    });

    driver.stop();
    (void)server.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor close drain win timeout\n", stderr);
        return 8;
    }
    if (session.error_count_ != 0 || session.last_error_ != net::errc::ok) {
        std::fputs("reactor close drain win unexpected error\n", stderr);
        return 9;
    }
    if (!driver.closed() || driver.last_error() != net::errc::closed) {
        std::fputs("reactor close drain win driver state mismatch\n", stderr);
        return 10;
    }

    std::puts("net reactor close drain win smoke: ok");
    return 0;
}
