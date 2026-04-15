#include <cstdio>

import charm.net;
import net.backend.stub;
import util.core;

int main() {
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

    if (!listener.listen(stack, net::Endpoint::ipv4_loopback(30205), 2)) {
        std::fputs("reactor listener close listen failed\n", stderr);
        return 1;
    }
    if (!listener_driver.start()) {
        std::fputs("reactor listener close driver start failed\n", stderr);
        return 2;
    }
    if (!listener.close()) {
        std::fputs("reactor listener close local close failed\n", stderr);
        return 3;
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
        return 4;
    }
    if (accepted.valid() || accepted_watch) {
        std::fputs("reactor listener close accepted state mismatch\n", stderr);
        return 5;
    }

    std::puts("net reactor listener close smoke: ok");
    return 0;
}
