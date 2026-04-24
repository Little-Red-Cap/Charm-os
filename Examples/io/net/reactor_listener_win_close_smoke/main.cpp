#include <chrono>
#include <cstdio>
#include <thread>
#include <utility>

import charm.net;
import net.backend.win;
import util.core;

namespace {
    using namespace std::chrono_literals;

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

    [[nodiscard]] bool listen_in_range(net::TcpListener& listener,
                                       net::Stack& stack,
                                       util::u16 begin_port,
                                       util::u16 end_port,
                                       util::u16& port_out) noexcept {
        for (util::u16 port = begin_port; port < end_port; ++port) {
            auto listening = net::TcpListener::listening_loopback(stack, port, 2);
            if (listening) {
                listener = std::move(listening.value());
                port_out = port;
                return true;
            }
        }
        return false;
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
        net::backend::WinProvider<16> provider{};
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
            std::fputs("reactor listener win close accepted subscribe failed\n", stderr);
            return 1;
        }

        util::u16 port = 0;
        if (!listen_in_range(listener, stack, 33020, 33120, port)) {
            std::fputs("reactor listener win close listen failed\n", stderr);
            return 2;
        }
        if (!listener_driver.start(all_events())) {
            std::fputs("reactor listener win close driver start failed\n", stderr);
            return 3;
        }
        auto connected = net::TcpClient::connected_loopback(stack, port);
        if (!connected) {
            std::fputs("reactor listener win close client connect failed\n", stderr);
            return 4;
        }
        client = std::move(connected.value());

        const bool done = wait_until([&]() {
            (void)poller.poll();
            (void)reactor.drain(8);
            return listener_driver.accepted()
                && accepted.valid()
                && accepted_watch
                && listener_driver.peer().port != 0
                && probe.saw_writable
                && !probe.failed;
        });

        reactor.unsubscribe(accepted_sub.value());
        accepted_driver.stop();
        listener_driver.stop();
        (void)accepted.close();
        (void)client.close();
        (void)listener.close();

        if (!done) {
            std::fputs("reactor listener win close accepted event inherit failed\n", stderr);
            return 5;
        }
    }

    {
        net::backend::WinProvider<16> provider{};
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

        util::u16 port = 0;
        if (!listen_in_range(listener, stack, 33120, 33220, port)) {
            std::fputs("reactor listener win close listen failed\n", stderr);
            return 6;
        }
        if (!listener_driver.start()) {
            std::fputs("reactor listener win close driver start failed\n", stderr);
            return 7;
        }
        if (!listener.close()) {
            std::fputs("reactor listener win close local close failed\n", stderr);
            return 8;
        }

        const bool done = wait_until([&]() {
            (void)poller.poll();
            (void)reactor.drain(4);
            return listener_driver.failed()
                && !listener_driver.accepted()
                && listener_driver.last_error() == net::errc::closed;
        });

        accepted_driver.stop();
        listener_driver.stop();
        (void)accepted.close();

        if (!done) {
            std::fputs("reactor listener win close timeout\n", stderr);
            return 9;
        }
        if (accepted.valid() || accepted_watch) {
            std::fputs("reactor listener win close accepted state mismatch\n", stderr);
            return 10;
        }
    }

    std::puts("net reactor listener win close smoke: ok");
    return 0;
}
