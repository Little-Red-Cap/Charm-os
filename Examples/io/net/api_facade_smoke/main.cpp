#include <cstdio>

import charm.net;
import net.backend.stub;
import util.core;

int main() {
    net::backend::StubProvider<16, 64, 64, 4> provider{};
    net::Stack stack{provider};

    net::Endpoint peer{};
    util::u8 rx[8]{};
    const util::u8 tx[4]{1, 2, 3, 4};

    auto listener = net::TcpListener::listening_loopback(stack, 30210, 2);
    if (!listener) {
        std::fputs("api facade tcp listening_loopback failed\n", stderr);
        return 1;
    }
    auto client = net::TcpClient::connected_loopback(stack, 30210);
    if (!client) {
        std::fputs("api facade tcp connected_loopback failed\n", stderr);
        return 2;
    }
    auto accepted = listener->accept(peer);
    if (!accepted) {
        std::fputs("api facade tcp accept failed\n", stderr);
        return 3;
    }
    if (peer.port == 0) {
        std::fputs("api facade tcp peer missing\n", stderr);
        return 4;
    }
    if (!client->send(net::ByteView{tx, 4})) {
        std::fputs("api facade tcp send failed\n", stderr);
        return 5;
    }
    auto received = accepted->recv(net::MutByteView{rx, sizeof(rx)});
    if (!received || received.value() != 4 || rx[0] != 1 || rx[3] != 4) {
        std::fputs("api facade tcp recv failed\n", stderr);
        return 6;
    }
    (void)client->close();
    (void)accepted->close();
    (void)listener->close();

    auto listener_any = net::TcpListener::listening_any(stack, 30211, 2);
    if (!listener_any) {
        std::fputs("api facade tcp listening_any failed\n", stderr);
        return 7;
    }
    auto client_any = net::TcpClient::connected_loopback(stack, 30211);
    if (!client_any) {
        std::fputs("api facade tcp connected to listening_any failed\n", stderr);
        return 8;
    }
    auto accepted_any = listener_any->accept();
    if (!accepted_any) {
        std::fputs("api facade tcp accept_any failed\n", stderr);
        return 9;
    }
    (void)client_any->close();
    (void)accepted_any->close();
    (void)listener_any->close();

    auto udp_any = net::UdpSocket::bound_any(stack, 30212);
    if (!udp_any) {
        std::fputs("api facade udp bound_any failed\n", stderr);
        return 10;
    }
    auto udp_connected = net::UdpSocket::connected_loopback(stack, 30212);
    if (!udp_connected) {
        std::fputs("api facade udp connected_loopback failed\n", stderr);
        return 11;
    }
    if (!udp_connected->send(net::ByteView{tx, 4})) {
        std::fputs("api facade udp send failed\n", stderr);
        return 12;
    }
    received = udp_any->recv(net::MutByteView{rx, sizeof(rx)});
    if (!received || received.value() != 4 || rx[0] != 1 || rx[3] != 4) {
        std::fputs("api facade udp recv_any failed\n", stderr);
        return 13;
    }
    (void)udp_connected->close();
    (void)udp_any->close();

    auto udp_loopback = net::UdpSocket::bound_loopback(stack, 30213);
    if (!udp_loopback) {
        std::fputs("api facade udp bound_loopback failed\n", stderr);
        return 14;
    }
    auto udp_sender_loopback = net::UdpSocket::connected_loopback(stack, 30213);
    if (!udp_sender_loopback) {
        std::fputs("api facade udp second connected_loopback failed\n", stderr);
        return 15;
    }
    if (!udp_sender_loopback->send(net::ByteView{tx, 4})) {
        std::fputs("api facade udp second send failed\n", stderr);
        return 16;
    }
    received = udp_loopback->recv(net::MutByteView{rx, sizeof(rx)});
    if (!received || received.value() != 4 || rx[0] != 1 || rx[3] != 4) {
        std::fputs("api facade udp recv_loopback failed\n", stderr);
        return 17;
    }
    (void)udp_sender_loopback->close();
    (void)udp_loopback->close();

    std::puts("net api facade smoke: ok");
    return 0;
}
