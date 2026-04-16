#include <chrono>
#include <cstdio>
#include <thread>

import charm.net;
import net.backend.win;
import util.core;

namespace {
    using namespace std::chrono_literals;

    template <class Fn>
    bool wait_until(Fn&& fn, int attempts = 200, std::chrono::milliseconds delay = 2ms) {
        for (int i = 0; i < attempts; ++i) {
            if (fn()) return true;
            std::this_thread::sleep_for(delay);
        }
        return false;
    }
}

int main() {
    net::backend::WinProvider<16> provider{};
    net::Stack stack{provider};

    net::TcpListener listener{};
    net::TcpClient server_side{};
    net::Endpoint peer{};

    util::u16 tcp_port = 0;
    for (util::u16 port = 25000; port < 25100; ++port) {
        auto candidate = net::TcpListener::listening_loopback(stack, port, 2);
        if (candidate) {
            listener = std::move(candidate.value());
            tcp_port = port;
            break;
        }
    }
    if (tcp_port == 0) {
        std::fputs("tcp listen failed\n", stderr);
        return 1;
    }

    auto client = net::TcpClient::connected_loopback(stack, tcp_port);
    if (!client) {
        std::fputs("tcp connect failed\n", stderr);
        return 2;
    }

    if (!wait_until([&]() {
        auto accepted = listener.accept(server_side, &peer);
        return static_cast<bool>(accepted);
    })) {
        std::fputs("tcp accept timeout\n", stderr);
        return 3;
    }

    util::u8 tx[4]{'p', 'i', 'n', 'g'};
    util::u8 rx[8]{};
    if (!wait_until([&]() {
        auto sent = client->send(tx);
        return static_cast<bool>(sent) || sent.error() != net::errc::would_block;
    })) {
        std::fputs("tcp send timeout\n", stderr);
        return 4;
    }

    bool tcp_recv_ok = false;
    if (!wait_until([&]() {
        auto received = server_side.recv(rx);
        if (received) {
            tcp_recv_ok = received.value() == 4
                && rx[0] == 'p'
                && rx[1] == 'i'
                && rx[2] == 'n'
                && rx[3] == 'g';
            return true;
        }
        return received.error() != net::errc::would_block;
    })) {
        std::fputs("tcp recv timeout\n", stderr);
        return 5;
    }
    if (!tcp_recv_ok) {
        std::fputs("tcp recv mismatch\n", stderr);
        return 6;
    }

    net::UdpSocket udp_a{};
    net::UdpSocket udp_b{};
    util::u16 udp_a_port = 0;
    util::u16 udp_b_port = 0;

    for (util::u16 port = 26000; port < 26100; ++port) {
        auto candidate = net::UdpSocket::bound_loopback(stack, port);
        if (!candidate) continue;
        udp_a = std::move(candidate.value());
        udp_a_port = port;
        break;
    }
    if (udp_a_port == 0) {
        std::fputs("udp a bind failed\n", stderr);
        return 7;
    }

    for (util::u16 port = static_cast<util::u16>(udp_a_port + 1); port < 26150; ++port) {
        auto candidate = net::UdpSocket::bound_loopback(stack, port);
        if (!candidate) continue;
        udp_b = std::move(candidate.value());
        udp_b_port = port;
        break;
    }
    if (udp_b_port == 0) {
        std::fputs("udp b bind failed\n", stderr);
        return 8;
    }

    auto udp_sent = udp_a.send_to(net::Endpoint::ipv4_loopback(udp_b_port), tx);
    if (!udp_sent) {
        std::fputs("udp send_to failed\n", stderr);
        return 9;
    }

    bool udp_recv_ok = false;
    net::Endpoint udp_peer{};
    if (!wait_until([&]() {
        auto received = udp_b.recv_from(rx, udp_peer);
        if (received) {
            udp_recv_ok = received.value() == 4
                && udp_peer.port == udp_a_port
                && rx[0] == 'p'
                && rx[1] == 'i'
                && rx[2] == 'n'
                && rx[3] == 'g';
            return true;
        }
        return received.error() != net::errc::would_block;
    })) {
        std::fputs("udp recv timeout\n", stderr);
        return 10;
    }
    if (!udp_recv_ok) {
        std::fputs("udp recv mismatch\n", stderr);
        return 11;
    }

    (void)client->close();
    (void)server_side.close();
    (void)listener.close();
    (void)udp_a.close();
    (void)udp_b.close();

    std::puts("net win loopback smoke: ok");
    return 0;
}
