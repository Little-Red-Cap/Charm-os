#include <cstdio>

import charm.net;
import net.backend.stub;
import util.core;

namespace {
    [[nodiscard]] constexpr util::u32 readable_event() noexcept {
        return static_cast<util::u32>(io::Event::readable);
    }

    [[nodiscard]] constexpr util::u32 writable_event() noexcept {
        return static_cast<util::u32>(io::Event::writable);
    }

    [[nodiscard]] constexpr util::u32 closed_event() noexcept {
        return static_cast<util::u32>(io::Event::closed);
    }

    [[nodiscard]] constexpr util::u32 error_event() noexcept {
        return static_cast<util::u32>(io::Event::error);
    }

    [[nodiscard]] constexpr util::u32 all_events() noexcept {
        return readable_event() | writable_event() | closed_event() | error_event();
    }

    struct AcceptedProbe {
        bool saw_writable{false};
        bool failed{false};

        static void on_event(void* ctx, io::Channel&, util::u32 events) noexcept {
            auto* self = static_cast<AcceptedProbe*>(ctx);
            if (!self) {
                return;
            }
            if ((events & error_event()) != 0u) {
                self->failed = true;
                return;
            }
            if ((events & writable_event()) != 0u) {
                self->saw_writable = true;
            }
        }
    };
}

int main() {
    {
        net::backend::StubProvider<8, 64, 64, 4> provider{};
        net::Stack stack{provider};
        io::Reactor reactor{};
        net::SocketPoller<4> poller{reactor};

        net::TcpListener listener{};
        net::TcpClient client{};
        net::TcpClient accepted{};
        net::SocketWatch accepted_watch{};
        net::SocketChannelBinding accepted_binding{accepted.raw()};
        net::SocketWatchDriver<4> accepted_driver{poller, accepted_binding, accepted_watch};
        net::TcpSingleAcceptDriver<net::SocketWatchDriver<4>, 4> listener_driver{
            reactor, poller, listener, accepted, accepted_driver};
        AcceptedProbe probe{};

        auto accepted_sub = reactor.subscribe(
            accepted_binding.channel(),
            all_events(),
            &AcceptedProbe::on_event,
            &probe);
        if (!accepted_sub) {
            std::fputs("reactor listener close accepted subscribe failed\n", stderr);
            return 1;
        }

        if (!listener.listen(stack, net::Endpoint::ipv4_loopback(30205), 2)) {
            std::fputs("reactor listener close listen failed\n", stderr);
            return 2;
        }
        if (!listener_driver.start(all_events())) {
            std::fputs("reactor listener close driver start failed\n", stderr);
            return 3;
        }
        if (!client.connect(stack, net::Endpoint::ipv4_loopback(30205))) {
            std::fputs("reactor listener close client connect failed\n", stderr);
            return 4;
        }

        bool done = false;
        for (int i = 0; i < 8; ++i) {
            (void)poller.poll();
            (void)reactor.drain(8);
            done = listener_driver.accepted()
                && accepted.valid()
                && accepted_watch
                && probe.saw_writable
                && !probe.failed;
            if (done) {
                break;
            }
        }

        reactor.unsubscribe(accepted_sub.value());
        accepted_driver.stop();
        listener_driver.stop();
        (void)accepted.close();
        (void)client.close();
        (void)listener.close();

        if (!done) {
            std::fputs("reactor listener close accepted event inherit failed\n", stderr);
            return 5;
        }
    }

    {
        net::backend::StubProvider<8, 64, 64, 4> provider{};
        net::Stack stack{provider};
        io::Reactor reactor{};
        net::SocketPoller<4> poller{reactor};

        net::TcpListener listener{};
        net::TcpClient accepted{};
        net::SocketWatch accepted_watch{};
        net::SocketChannelBinding accepted_binding{accepted.raw()};
        net::SocketWatchDriver<4> accepted_driver{poller, accepted_binding, accepted_watch};
        net::TcpSingleAcceptDriver<net::SocketWatchDriver<4>, 4> listener_driver{
            reactor, poller, listener, accepted, accepted_driver};

        if (!listener.listen(stack, net::Endpoint::ipv4_loopback(30206), 2)) {
            std::fputs("reactor listener close listen failed\n", stderr);
            return 6;
        }
        if (!listener_driver.start()) {
            std::fputs("reactor listener close driver start failed\n", stderr);
            return 7;
        }
        if (!listener.close()) {
            std::fputs("reactor listener close local close failed\n", stderr);
            return 8;
        }

        bool done = false;
        for (int i = 0; i < 8; ++i) {
            (void)poller.poll();
            (void)reactor.drain(4);
            done = listener_driver.failed()
                && !listener_driver.accepted()
                && listener_driver.last_error() == net::errc::closed;
            if (done) {
                break;
            }
        }

        accepted_driver.stop();
        listener_driver.stop();
        (void)accepted.close();

        if (!done) {
            std::fputs("reactor listener close timeout\n", stderr);
            return 9;
        }
        if (accepted.valid() || accepted_watch) {
            std::fputs("reactor listener close accepted state mismatch\n", stderr);
            return 10;
        }
    }

    std::puts("net reactor listener close smoke: ok");
    return 0;
}
