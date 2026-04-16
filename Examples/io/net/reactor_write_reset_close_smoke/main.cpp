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

    [[nodiscard]] bool set_abortive_close(SOCKET socket) noexcept {
        linger opt{};
        opt.l_onoff = 1;
        opt.l_linger = 0;
        return ::setsockopt(socket,
                            SOL_SOCKET,
                            SO_LINGER,
                            reinterpret_cast<const char*>(&opt),
                            static_cast<int>(sizeof(opt))) == 0;
    }

    struct ProbeSession {
        void set_sender(net::StreamSendFn fn, void* ctx) noexcept {
            send_ = fn;
            send_ctx_ = ctx;
        }

        void feed(net::ByteView) noexcept {}

        void notify_writable() noexcept {}

        void on_transport_closed() noexcept {
            ++closed_count_;
        }

        void on_transport_error(net::errc error) noexcept {
            ++error_count_;
            last_error_ = error;
        }

        net::StreamSendFn send_{nullptr};
        void* send_ctx_{nullptr};
        int closed_count_{0};
        int error_count_{0};
        net::errc last_error_{net::errc::ok};
    };
}

int main() {
    WinsockScope winsock{};
    if (!winsock.ok) {
        std::fputs("reactor write reset close winsock startup failed\n", stderr);
        return 1;
    }

    net::backend::WinProvider<16> provider{};
    net::Stack stack{provider};
    io::Reactor reactor{};
    net::SocketPoller<4> poller{reactor};

    net::TcpListener listener{};
    net::TcpClient server{};
    net::Endpoint peer{};

    util::u16 port = 0;
    for (util::u16 candidate = 33220; candidate < 33320; ++candidate) {
        auto listening = net::TcpListener::listening_loopback(stack, candidate, 2);
        if (listening) {
            listener = std::move(listening.value());
            port = candidate;
            break;
        }
    }
    if (port == 0) {
        std::fputs("reactor write reset close listen failed\n", stderr);
        return 2;
    }

    NativeSocket client{::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)};
    if (!client.valid()) {
        std::fputs("reactor write reset close native socket failed\n", stderr);
        return 3;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    if (::connect(client.value,
                  reinterpret_cast<const sockaddr*>(&addr),
                  static_cast<int>(sizeof(addr))) != 0) {
        std::fputs("reactor write reset close native connect failed\n", stderr);
        return 4;
    }

    net::errc accept_error = net::errc::ok;
    const bool accepted = wait_until([&]() {
        auto result = listener.accept(server, &peer);
        if (result) {
            return true;
        }
        accept_error = result.error();
        return accept_error != net::errc::would_block;
    });
    if (!accepted || accept_error != net::errc::ok) {
        std::fputs("reactor write reset close accept failed\n", stderr);
        return 5;
    }

    net::SocketChannelBinding server_binding{server.raw()};
    ProbeSession session{};
    using Driver = net::ReactorSocketDriver<ProbeSession, 4>;
    Driver driver{reactor, poller, server_binding, session};
    if (!driver.start()) {
        std::fputs("reactor write reset close driver start failed\n", stderr);
        return 6;
    }

    const util::u8 pong[4]{'p', 'o', 'n', 'g'};
    auto queued = session.send_(session.send_ctx_, net::ByteView{pong, 4});
    if (!queued || queued.value() != 4) {
        std::fputs("reactor write reset close queue send failed\n", stderr);
        return 7;
    }

    (void)poller.poll();
    if (!set_abortive_close(client.value)) {
        std::fputs("reactor write reset close linger setup failed\n", stderr);
        return 8;
    }
    client.close();
    (void)reactor.drain(8);

    auto after_close = session.send_(session.send_ctx_, net::ByteView{pong, 4});
    if (after_close || after_close.error() != net::errc::closed) {
        std::fputs("reactor write reset close sender terminal failed\n", stderr);
        return 9;
    }
    auto arm_after_close = driver.arm_writable();
    if (arm_after_close || arm_after_close.error() != net::errc::closed) {
        std::fputs("reactor write reset close arm terminal failed\n", stderr);
        return 10;
    }

    driver.stop();
    (void)server.close();
    (void)listener.close();

    if (session.closed_count_ != 1) {
        std::fputs("reactor write reset close missing closed callback\n", stderr);
        return 11;
    }
    if (session.error_count_ != 0 || session.last_error_ != net::errc::ok) {
        std::fputs("reactor write reset close unexpected error callback\n", stderr);
        return 12;
    }
    if (!driver.closed() || driver.last_error() != net::errc::closed) {
        std::fputs("reactor write reset close driver state mismatch\n", stderr);
        return 13;
    }

    std::puts("net reactor write reset close smoke: ok");
    return 0;
}
