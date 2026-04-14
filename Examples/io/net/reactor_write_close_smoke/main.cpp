#include <cstdio>

import charm.net;
import net.backend.stub;
import util.core;

namespace {
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
    net::backend::StubProvider<8, 64, 64, 4> provider{};
    net::Stack stack{provider};
    io::Reactor reactor{};
    net::SocketPoller<4> poller{reactor};

    net::TcpListener listener{};
    net::TcpClient client{};
    net::TcpClient server{};

    if (!listener.listen(stack, net::Endpoint::ipv4_loopback(30204), 2)) {
        std::fputs("reactor write close listen failed\n", stderr);
        return 1;
    }
    if (!client.connect(stack, net::Endpoint::ipv4_loopback(30204))) {
        std::fputs("reactor write close connect failed\n", stderr);
        return 2;
    }
    if (!listener.accept(server, nullptr)) {
        std::fputs("reactor write close accept failed\n", stderr);
        return 3;
    }

    net::SocketChannelBinding server_binding{server.raw()};
    ProbeSession session{};
    using Driver = net::ReactorSocketDriver<ProbeSession, 4>;
    Driver driver{reactor, poller, server_binding, session};
    if (!driver.start()) {
        std::fputs("reactor write close driver start failed\n", stderr);
        return 4;
    }

    const util::u8 pong[4]{'p', 'o', 'n', 'g'};
    auto queued = session.send_(session.send_ctx_, net::ByteView{pong, 4});
    if (!queued || queued.value() != 4) {
        std::fputs("reactor write close queue send failed\n", stderr);
        return 5;
    }

    (void)poller.poll();
    if (!client.close()) {
        std::fputs("reactor write close client close failed\n", stderr);
        return 6;
    }
    (void)reactor.drain(8);

    driver.stop();
    (void)server.close();
    (void)listener.close();

    if (session.closed_count_ != 1) {
        std::fputs("reactor write close missing closed callback\n", stderr);
        return 7;
    }
    if (session.error_count_ != 0 || session.last_error_ != net::errc::ok) {
        std::fputs("reactor write close unexpected error callback\n", stderr);
        return 8;
    }
    if (!driver.closed() || driver.last_error() != net::errc::closed) {
        std::fputs("reactor write close driver state mismatch\n", stderr);
        return 9;
    }

    std::puts("net reactor write close smoke: ok");
    return 0;
}
