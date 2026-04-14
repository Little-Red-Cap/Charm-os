#include <cstdio>

import charm.net;
import net.backend.stub;
import util.core;

namespace {
    bool bytes_eq(const util::u8* lhs, const util::u8* rhs, util::usize count) noexcept {
        for (util::usize i = 0; i < count; ++i) {
            if (lhs[i] != rhs[i]) return false;
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
            return rx_len_ == 4 && bytes_eq(rx_, reinterpret_cast<const util::u8*>("ping"), 4);
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
    net::backend::StubProvider<8, 64, 64, 4> provider{};
    net::Stack stack{provider};
    io::Reactor reactor{};
    net::SocketPoller<4> poller{reactor};

    net::TcpListener listener{};
    net::TcpClient client{};
    net::TcpClient server{};

    if (!listener.listen(stack, net::Endpoint::ipv4_loopback(30200), 2)) {
        std::fputs("reactor close drain listen failed\n", stderr);
        return 1;
    }
    if (!client.connect(stack, net::Endpoint::ipv4_loopback(30200))) {
        std::fputs("reactor close drain connect failed\n", stderr);
        return 2;
    }
    if (!listener.accept(server, nullptr)) {
        std::fputs("reactor close drain accept failed\n", stderr);
        return 3;
    }

    net::SocketChannelBinding server_binding{server.raw()};
    ProbeSession session{};
    using Driver = net::ReactorSocketDriver<ProbeSession, 4>;
    Driver driver{reactor, poller, server_binding, session};
    if (!driver.start()) {
        std::fputs("reactor close drain driver start failed\n", stderr);
        return 4;
    }

    const util::u8 ping[4]{'p', 'i', 'n', 'g'};
    auto sent = client.send(net::ByteView{ping, 4});
    if (!sent || sent.value() != 4) {
        std::fputs("reactor close drain client send failed\n", stderr);
        return 5;
    }
    if (!client.close()) {
        std::fputs("reactor close drain client close failed\n", stderr);
        return 6;
    }

    bool done = false;
    for (int i = 0; i < 16; ++i) {
        (void)poller.poll();
        (void)reactor.drain(8);
        done = session.got_ping() && session.closed_count_ == 1;
        if (done) break;
    }

    driver.stop();
    (void)server.close();
    (void)listener.close();

    if (!done) {
        std::fputs("reactor close drain timeout\n", stderr);
        return 7;
    }
    if (session.error_count_ != 0 || session.last_error_ != net::errc::ok) {
        std::fputs("reactor close drain unexpected error\n", stderr);
        return 8;
    }
    if (!driver.closed()) {
        std::fputs("reactor close drain missing closed state\n", stderr);
        return 9;
    }

    std::puts("net reactor close drain smoke: ok");
    return 0;
}
