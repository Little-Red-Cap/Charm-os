#include <cstdio>

import charm.net;
import net.backend.stub;
import util.core;

int main() {
    net::backend::StubProvider<16, 64, 64, 4> provider{};
    net::Stack stack{provider};

    net::TcpListener listener{};
    net::TcpClient client{};
    net::TcpClient server{};
    net::Endpoint peer{};
    util::u8 rx[8]{};
    const util::u8 tx[4]{1, 2, 3, 4};

    if (!listener.listen_loopback(stack, 30210, 2)) {
        std::fputs("api facade tcp listen_loopback failed\n", stderr);
        return 1;
    }
    if (!client.connect_loopback(stack, 30210)) {
        std::fputs("api facade tcp connect_loopback failed\n", stderr);
        return 2;
    }
    if (!listener.accept(server, &peer)) {
        std::fputs("api facade tcp accept failed\n", stderr);
        return 3;
    }
    if (peer.port == 0) {
        std::fputs("api facade tcp peer missing\n", stderr);
        return 4;
    }
    if (!client.send(net::ByteView{tx, 4})) {
        std::fputs("api facade tcp send failed\n", stderr);
        return 5;
    }
    auto received = server.recv(net::MutByteView{rx, sizeof(rx)});
    if (!received || received.value() != 4 || rx[0] != 1 || rx[3] != 4) {
        std::fputs("api facade tcp recv failed\n", stderr);
        return 6;
    }
    (void)client.close();
    (void)server.close();
    (void)listener.close();

    net::TcpListener listener_any{};
    net::TcpClient client_any{};
    net::TcpClient server_any{};
    if (!listener_any.listen_any(stack, 30211, 2)) {
        std::fputs("api facade tcp listen_any failed\n", stderr);
        return 7;
    }
    if (!client_any.connect_loopback(stack, 30211)) {
        std::fputs("api facade tcp connect to listen_any failed\n", stderr);
        return 8;
    }
    if (!listener_any.accept(server_any, nullptr)) {
        std::fputs("api facade tcp accept_any failed\n", stderr);
        return 9;
    }
    (void)client_any.close();
    (void)server_any.close();
    (void)listener_any.close();

    net::UdpSocket udp_any{};
    net::UdpSocket udp_connected{};
    if (!udp_any.bind_any(stack, 30212)) {
        std::fputs("api facade udp bind_any failed\n", stderr);
        return 10;
    }
    if (!udp_connected.connect_loopback(stack, 30212)) {
        std::fputs("api facade udp connect_loopback failed\n", stderr);
        return 11;
    }
    if (!udp_connected.send(net::ByteView{tx, 4})) {
        std::fputs("api facade udp send failed\n", stderr);
        return 12;
    }
    received = udp_any.recv(net::MutByteView{rx, sizeof(rx)});
    if (!received || received.value() != 4 || rx[0] != 1 || rx[3] != 4) {
        std::fputs("api facade udp recv_any failed\n", stderr);
        return 13;
    }
    (void)udp_connected.close();
    (void)udp_any.close();

    net::UdpSocket udp_loopback{};
    net::UdpSocket udp_sender_loopback{};
    if (!udp_loopback.bind_loopback(stack, 30213)) {
        std::fputs("api facade udp bind_loopback failed\n", stderr);
        return 14;
    }
    if (!udp_sender_loopback.connect_loopback(stack, 30213)) {
        std::fputs("api facade udp second connect_loopback failed\n", stderr);
        return 15;
    }
    if (!udp_sender_loopback.send(net::ByteView{tx, 4})) {
        std::fputs("api facade udp second send failed\n", stderr);
        return 16;
    }
    received = udp_loopback.recv(net::MutByteView{rx, sizeof(rx)});
    if (!received || received.value() != 4 || rx[0] != 1 || rx[3] != 4) {
        std::fputs("api facade udp recv_loopback failed\n", stderr);
        return 17;
    }
    (void)udp_sender_loopback.close();
    (void)udp_loopback.close();

    std::puts("net api facade smoke: ok");
    return 0;
}
