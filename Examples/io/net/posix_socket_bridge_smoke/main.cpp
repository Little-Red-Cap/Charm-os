#include <chrono>
#include <cstdio>
#include <thread>

import net.backend.win;
import net.common;
import net.posix;
import net.stack;
import posix.api;
import posix.fd_table;
import posix.file;
import posix.pipe;
import posix.proc;
import util.core;

namespace {
    using namespace std::chrono_literals;

    template <class Fn>
    bool wait_until(Fn&& fn, int attempts = 300, std::chrono::milliseconds delay = 2ms) {
        for (int i = 0; i < attempts; ++i) {
            if (fn()) return true;
            std::this_thread::sleep_for(delay);
        }
        return false;
    }

    bool bytes_eq(const char* lhs, const char* rhs, util::usize count) noexcept {
        for (util::usize i = 0; i < count; ++i) {
            if (lhs[i] != rhs[i]) return false;
        }
        return true;
    }
}

int main() {
    using FdTableType = posix::FdTable<32>;
    using FileServiceType = posix::FileService<4>;
    using PipeServiceType = posix::PipeService<4, 64>;
    using ProcServiceType = posix::ProcService<4, 4, 32, 4>;
    using SocketServiceType = posix::SocketService<16>;
    using ApiType = posix::Api<32, 4, 64, 4, 4, 4, 128, 16, 16, 256, 4096, 4096, 4, 32, 16>;

    net::backend::WinProvider<32> provider{};
    net::Stack stack{provider};

    FdTableType fds{};
    FileServiceType files{};
    PipeServiceType pipes{};
    ProcServiceType procs{};
    SocketServiceType sockets{};
    fds.init();
    files.init();
    pipes.init();
    procs.init();
    sockets.init();
    sockets.bind_stack(stack);

    ApiType api{fds, files, pipes, procs, &sockets};

    int listener = api.socket(posix::AF_INET, posix::SOCK_STREAM);
    if (listener < 0) {
        std::fputs("posix tcp socket failed\n", stderr);
        return 1;
    }

    util::u16 tcp_port = 0;
    for (util::u16 port = 27000; port < 27100; ++port) {
        if (api.bind(listener, net::Endpoint::ipv4_loopback(port)) != 0) continue;
        if (api.listen(listener, 2) != 0) {
            std::fputs("posix tcp listen failed\n", stderr);
            return 2;
        }
        tcp_port = port;
        break;
    }
    if (tcp_port == 0) {
        std::fputs("posix tcp bind failed\n", stderr);
        return 3;
    }

    posix::PosixStat socket_stat{};
    if (api.fstat(listener, &socket_stat) != 0) {
        std::fputs("posix socket fstat failed\n", stderr);
        return 4;
    }
    constexpr util::u32 kSocketTypeMask = 0170000u;
    constexpr util::u32 kSocketTypeBits = 0140000u;
    if ((socket_stat.mode & kSocketTypeMask) != kSocketTypeBits) {
        std::fputs("posix socket fstat type mismatch\n", stderr);
        return 5;
    }

    constexpr int duplicated_listener_fd = 15;
    if (api.dup2(listener, duplicated_listener_fd) != duplicated_listener_fd) {
        std::fputs("posix socket dup2 failed\n", stderr);
        return 6;
    }
    if (api.close(listener) != 0) {
        std::fputs("posix original listener close failed\n", stderr);
        return 7;
    }

    int client = api.socket(posix::AF_INET, posix::SOCK_STREAM);
    if (client < 0) {
        std::fputs("posix client socket failed\n", stderr);
        return 8;
    }
    if (api.connect(client, net::Endpoint::ipv4_loopback(tcp_port)) != 0) {
        std::fputs("posix tcp connect failed\n", stderr);
        return 9;
    }

    net::Endpoint accepted_peer{};
    int accepted = -1;
    if (!wait_until([&]() {
        accepted = api.accept(duplicated_listener_fd, &accepted_peer);
        return accepted >= 0;
    })) {
        std::fputs("posix tcp accept timeout\n", stderr);
        return 10;
    }
    if (accepted < 0) {
        std::fputs("posix tcp accept failed\n", stderr);
        return 11;
    }

    const char ping[4]{'p', 'i', 'n', 'g'};
    char tcp_rx[8]{};
    if (!wait_until([&]() {
        const auto written = api.write(client, ping, 4);
        return written == 4;
    })) {
        std::fputs("posix tcp write timeout\n", stderr);
        return 12;
    }

    bool tcp_recv_ok = false;
    if (!wait_until([&]() {
        const auto received = api.recv(accepted, tcp_rx, sizeof(tcp_rx));
        if (received >= 0) {
            tcp_recv_ok = received == 4 && bytes_eq(tcp_rx, ping, 4);
            return true;
        }
        return false;
    })) {
        std::fputs("posix tcp recv timeout\n", stderr);
        return 13;
    }
    if (!tcp_recv_ok) {
        std::fputs("posix tcp recv mismatch\n", stderr);
        return 14;
    }

    const char pong[4]{'p', 'o', 'n', 'g'};
    if (!wait_until([&]() {
        const auto sent = api.send(accepted, pong, 4);
        return sent == 4;
    })) {
        std::fputs("posix tcp send timeout\n", stderr);
        return 15;
    }

    bool tcp_read_ok = false;
    if (!wait_until([&]() {
        const auto read = api.read(client, tcp_rx, sizeof(tcp_rx));
        if (read >= 0) {
            tcp_read_ok = read == 4 && bytes_eq(tcp_rx, pong, 4);
            return true;
        }
        return false;
    })) {
        std::fputs("posix tcp read timeout\n", stderr);
        return 16;
    }
    if (!tcp_read_ok) {
        std::fputs("posix tcp read mismatch\n", stderr);
        return 17;
    }

    if (api.shutdown(client, net::ShutdownMode::both) != 0) {
        std::fputs("posix tcp shutdown failed\n", stderr);
        return 18;
    }

    int udp_a = api.socket(posix::AF_INET, posix::SOCK_DGRAM);
    int udp_b = api.socket(posix::AF_INET, posix::SOCK_DGRAM);
    if (udp_a < 0 || udp_b < 0) {
        std::fputs("posix udp socket failed\n", stderr);
        return 19;
    }

    util::u16 udp_a_port = 0;
    util::u16 udp_b_port = 0;
    for (util::u16 port = 28000; port < 28100; ++port) {
        if (api.bind(udp_a, net::Endpoint::ipv4_loopback(port)) != 0) continue;
        udp_a_port = port;
        break;
    }
    if (udp_a_port == 0) {
        std::fputs("posix udp a bind failed\n", stderr);
        return 20;
    }

    for (util::u16 port = static_cast<util::u16>(udp_a_port + 1); port < 28150; ++port) {
        if (api.bind(udp_b, net::Endpoint::ipv4_loopback(port)) != 0) continue;
        udp_b_port = port;
        break;
    }
    if (udp_b_port == 0) {
        std::fputs("posix udp b bind failed\n", stderr);
        return 21;
    }

    const auto udp_sent = api.sendto(udp_a, ping, 4, net::Endpoint::ipv4_loopback(udp_b_port));
    if (udp_sent != 4) {
        std::fputs("posix udp sendto failed\n", stderr);
        return 22;
    }

    bool udp_recv_ok = false;
    net::Endpoint udp_peer{};
    if (!wait_until([&]() {
        const auto received = api.recvfrom(udp_b, tcp_rx, sizeof(tcp_rx), &udp_peer);
        if (received >= 0) {
            udp_recv_ok = received == 4 && udp_peer.port == udp_a_port && bytes_eq(tcp_rx, ping, 4);
            return true;
        }
        return false;
    })) {
        std::fputs("posix udp recvfrom timeout\n", stderr);
        return 23;
    }
    if (!udp_recv_ok) {
        std::fputs("posix udp recvfrom mismatch\n", stderr);
        return 24;
    }

    if (api.close(client) != 0
        || api.close(accepted) != 0
        || api.close(duplicated_listener_fd) != 0
        || api.close(udp_a) != 0
        || api.close(udp_b) != 0) {
        std::fputs("posix socket close failed\n", stderr);
        return 25;
    }

    std::puts("net posix socket bridge smoke: ok");
    return 0;
}
