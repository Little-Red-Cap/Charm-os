#include <array>
#include <cstdio>

import net.arp;
import net.driver;
import net.ether;
import net.ipv4;
import net.packet;
import net.stack;
import net.udp;
import util.core;
import util.expected;

namespace {
    struct StubLinkDriver {
        net::OwnedPacketSinkRef input_sink{};
        std::array<util::u8, 128> tx_bytes{};
        util::usize tx_size{0};
        util::usize tx_calls{0};

        [[nodiscard]] net::NetDriverInfo info() const noexcept {
            return net::NetDriverInfo{
                .mtu = 128,
                .mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u),
                .capabilities = net::NetIfCapability::rx
                    | net::NetIfCapability::tx
                    | net::NetIfCapability::broadcast
            };
        }

        [[nodiscard]] net::Result<void> set_input_sink(net::OwnedPacketSinkRef sink) noexcept {
            input_sink = sink;
            return {};
        }

        [[nodiscard]] net::Result<void> poll() noexcept {
            return {};
        }

        [[nodiscard]] net::Result<void> transmit(net::PacketView packet) noexcept {
            if (packet.size() > tx_bytes.size()) {
                return util::unexpected(net::errc::buffer_overflow);
            }
            for (util::usize i = 0; i < packet.size(); ++i) {
                tx_bytes[i] = packet[i];
            }
            tx_size = packet.size();
            ++tx_calls;
            return {};
        }
    };

    [[nodiscard]] bool same_ipv4(const net::IpAddress& lhs, const net::IpAddress& rhs) noexcept {
        return net::is_same_ipv4_address(lhs, rhs);
    }

    [[nodiscard]] bool same_mac(const net::MacAddress& lhs, const net::MacAddress& rhs) noexcept {
        return net::is_same_mac(lhs, rhs);
    }
}

int main() {
    constexpr auto local_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto local_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto peer_mac = net::MacAddress::from_bytes(0x02u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu);
    constexpr auto peer_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    constexpr auto local = net::Endpoint{net::IpAddress::ipv4_any(), 5000};
    constexpr auto peer = net::Endpoint{peer_ip, 7000};
    static constexpr util::u8 payload[]{'p', 'o', 'n', 'g'};

    net::NetIf netif{};
    auto configured = netif.configure(net::NetIfConfig{
        .mtu = 128,
        .mac = local_mac,
        .address = local_ip,
        .capabilities = net::NetIfCapability::rx
            | net::NetIfCapability::tx
            | net::NetIfCapability::broadcast
    });
    if (!configured) {
        std::fputs("udp egress smoke netif configure failed\n", stderr);
        return 1;
    }

    StubLinkDriver link{};
    net::NetDriver driver{};
    auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
    if (!attached) {
        std::fputs("udp egress smoke driver attach failed\n", stderr);
        return 2;
    }

    net::Stack stack{};
    auto registered = stack.register_driver(driver);
    if (!registered || stack.driver_count() != 1 || stack.netif_count() != 1) {
        std::fputs("udp egress smoke stack register failed\n", stderr);
        return 3;
    }

    auto up = netif.bring_up();
    if (!up) {
        std::fputs("udp egress smoke netif bring_up failed\n", stderr);
        return 4;
    }

    net::ArpTable<4> unresolved{};
    auto missing = net::send_udp_ipv4<128>(netif, unresolved, local, peer, net::ByteView{payload, sizeof(payload)});
    if (missing || missing.error() != net::errc::noent || link.tx_calls != 0) {
        std::fputs("udp egress smoke unresolved arp check failed\n", stderr);
        return 5;
    }

    net::ArpTable<4> arp{};
    auto remembered = arp.remember(peer_ip, peer_mac);
    if (!remembered) {
        std::fputs("udp egress smoke arp remember failed\n", stderr);
        return 6;
    }

    auto sent = net::send_udp_ipv4<128>(
        netif,
        arp,
        local,
        peer,
        net::ByteView{payload, sizeof(payload)},
        48,
        0x2468u,
        0x2Eu);
    if (!sent || link.tx_calls != 1) {
        std::fputs("udp egress smoke send failed\n", stderr);
        return 7;
    }

    auto frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!frame || frame.value().type != net::EtherType::ipv4) {
        std::fputs("udp egress smoke ether parse failed\n", stderr);
        return 8;
    }
    if (!same_mac(frame.value().destination, peer_mac)
        || !same_mac(frame.value().source, local_mac)) {
        std::fputs("udp egress smoke ether header mismatch\n", stderr);
        return 9;
    }

    auto ipv4 = net::parse_ipv4_packet(frame.value().payload);
    if (!ipv4
        || ipv4.value().protocol != net::Ipv4Protocol::udp
        || ipv4.value().ttl != 48
        || ipv4.value().identification != 0x2468u
        || ipv4.value().dscp_ecn != 0x2Eu
        || !same_ipv4(ipv4.value().source, local_ip)
        || !same_ipv4(ipv4.value().destination, peer_ip)) {
        std::fputs("udp egress smoke ipv4 fields mismatch\n", stderr);
        return 10;
    }

    auto udp = net::parse_udp_datagram(ipv4.value().payload);
    if (!udp
        || udp.value().source_port != 5000
        || udp.value().destination_port != 7000
        || udp.value().length != net::udp_header_size() + sizeof(payload)
        || udp.value().checksum == 0u) {
        std::fputs("udp egress smoke udp fields mismatch\n", stderr);
        return 11;
    }

    for (util::usize i = 0; i < sizeof(payload); ++i) {
        if (udp.value().payload[i] != payload[i]) {
            std::fputs("udp egress smoke payload mismatch\n", stderr);
            return 12;
        }
    }

    std::puts("net udp egress smoke: ok");
    return 0;
}
