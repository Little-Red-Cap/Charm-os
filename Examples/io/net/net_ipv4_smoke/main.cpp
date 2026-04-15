#include <array>
#include <cstdio>

import net.driver;
import net.ether;
import net.ipv4;
import net.packet;
import net.stack;
import util.core;
import util.expected;

namespace {
    struct PayloadProbe {
        std::array<util::u8, 96> bytes{};
        util::usize size{0};
        util::usize calls{0};
        net::IpAddress source{};
        net::IpAddress destination{};
        net::Ipv4Protocol protocol{net::Ipv4Protocol::icmp};
        util::u8 ttl{0};

        [[nodiscard]] net::Result<void> consume(const net::Ipv4PacketView& header,
                                                net::OwnedPacket packet) noexcept {
            const auto view = packet.view();
            if (view.size() > bytes.size()) {
                return util::unexpected(net::errc::buffer_overflow);
            }
            for (util::usize i = 0; i < view.size(); ++i) {
                bytes[i] = view[i];
            }
            size = view.size();
            source = header.source;
            destination = header.destination;
            protocol = header.protocol;
            ttl = header.ttl;
            ++calls;
            return {};
        }
    };

    struct StubLinkDriver {
        net::OwnedPacketSinkRef input_sink{};
        net::PacketPool<2, 128> rx_pool{};
        std::array<util::u8, 128> rx_bytes{};
        util::usize rx_size{0};
        bool rx_ready{false};

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
            if (!rx_ready) {
                return {};
            }
            if (!input_sink.valid()) {
                return util::unexpected(net::errc::bad_state);
            }

            auto lease = rx_pool.acquire();
            if (!lease) {
                return util::unexpected(lease.error());
            }
            auto appended = lease.value()->append(net::ByteView{rx_bytes.data(), rx_size});
            if (!appended) {
                return util::unexpected(appended.error());
            }

            rx_ready = false;
            return input_sink.consume(net::OwnedPacket{
                static_cast<net::PacketPool<2, 128>::Lease&&>(lease.value())
            });
        }

        [[nodiscard]] net::Result<void> transmit(net::PacketView) noexcept {
            return {};
        }

        void queue_rx(net::ByteView packet) noexcept {
            rx_size = packet.size();
            rx_ready = true;
            for (util::usize i = 0; i < packet.size(); ++i) {
                rx_bytes[i] = packet[i];
            }
        }
    };

    template <util::usize Capacity>
    [[nodiscard]] net::Result<void> write_ether_frame(net::PacketBuffer<Capacity>& packet,
                                                      net::MacAddress destination,
                                                      net::MacAddress source,
                                                      net::EtherType type,
                                                      net::ByteView payload) noexcept {
        auto reset = packet.reset();
        if (!reset) {
            return util::unexpected(reset.error());
        }

        std::array<util::u8, 14> header{};
        for (util::usize i = 0; i < 6; ++i) {
            header[i] = destination.bytes[i];
            header[6 + i] = source.bytes[i];
        }
        const auto raw_type = net::ether_type_value(type);
        header[12] = static_cast<util::u8>((raw_type >> 8) & 0xFFu);
        header[13] = static_cast<util::u8>(raw_type & 0xFFu);

        auto appended_header = packet.append(net::ByteView{header.data(), header.size()});
        if (!appended_header) {
            return util::unexpected(appended_header.error());
        }

        auto appended_payload = packet.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }
        return {};
    }

    [[nodiscard]] bool bytes_eq(const std::array<util::u8, 96>& lhs,
                                util::usize lhs_size,
                                net::ByteView rhs) noexcept {
        if (lhs_size != rhs.size()) {
            return false;
        }
        for (util::usize i = 0; i < lhs_size; ++i) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool same_ipv4(const net::IpAddress& lhs, const net::IpAddress& rhs) noexcept {
        if (!lhs.is_ipv4() || !rhs.is_ipv4()) {
            return false;
        }
        for (util::usize i = 0; i < 4; ++i) {
            if (lhs.bytes[i] != rhs.bytes[i]) {
                return false;
            }
        }
        return true;
    }
}

int main() {
    constexpr auto local_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto local_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto peer_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    constexpr auto other_ip = net::IpAddress::ipv4(10, 0, 0, 12);

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
        std::fputs("ipv4 smoke netif configure failed\n", stderr);
        return 1;
    }

    StubLinkDriver link{};
    net::NetDriver driver{};
    auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
    if (!attached) {
        std::fputs("ipv4 smoke driver attach failed\n", stderr);
        return 2;
    }

    net::Stack stack{};
    PayloadProbe icmp{};
    PayloadProbe udp{};
    net::Ipv4Service ipv4{netif};
    ipv4.set_icmp_sink(net::make_ipv4_packet_sink_ref(icmp));
    ipv4.set_udp_sink(net::make_ipv4_packet_sink_ref(udp));
    stack.set_ipv4_sink(net::make_owned_packet_sink_ref(ipv4));

    auto registered = stack.register_driver(driver);
    if (!registered || stack.driver_count() != 1 || stack.netif_count() != 1) {
        std::fputs("ipv4 smoke stack register failed\n", stderr);
        return 3;
    }

    auto up = netif.bring_up();
    if (!up) {
        std::fputs("ipv4 smoke netif bring_up failed\n", stderr);
        return 4;
    }

    static constexpr util::u8 icmp_payload[]{0x08u, 0x00u, 0x12u, 0x34u};
    net::PacketBuffer<128> icmp_packet{};
    auto encoded_icmp = net::write_ipv4_packet(
        icmp_packet,
        net::Ipv4PacketSpec{
            .dscp_ecn = 0,
            .identification = 0x1234u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 64,
            .protocol = net::Ipv4Protocol::icmp,
            .source = peer_ip,
            .destination = local_ip,
        },
        net::ByteView{icmp_payload, sizeof(icmp_payload)});
    if (!encoded_icmp) {
        std::fputs("ipv4 smoke icmp encode failed\n", stderr);
        return 5;
    }

    auto parsed_icmp = net::parse_ipv4_packet(icmp_packet.view());
    if (!parsed_icmp
        || parsed_icmp.value().header_length != net::ipv4_min_header_size()
        || parsed_icmp.value().total_length != net::ipv4_min_header_size() + sizeof(icmp_payload)
        || parsed_icmp.value().protocol != net::Ipv4Protocol::icmp
        || parsed_icmp.value().ttl != 64
        || !same_ipv4(parsed_icmp.value().source, peer_ip)
        || !same_ipv4(parsed_icmp.value().destination, local_ip)) {
        std::fputs("ipv4 smoke icmp parse mismatch\n", stderr);
        return 6;
    }

    net::PacketBuffer<128> icmp_frame{};
    auto wrote_icmp_frame = write_ether_frame(
        icmp_frame,
        net::MacAddress::broadcast(),
        local_mac,
        net::EtherType::ipv4,
        icmp_packet.view().payload);
    if (!wrote_icmp_frame) {
        std::fputs("ipv4 smoke icmp ether encode failed\n", stderr);
        return 7;
    }

    link.queue_rx(icmp_frame.view().payload);
    auto polled = stack.poll_links();
    if (!polled || ipv4.packet_count() != 1 || ipv4.drop_count() != 0 || icmp.calls != 1 || udp.calls != 0 || link.rx_pool.in_use_count() != 0) {
        std::fputs("ipv4 smoke icmp dispatch failed\n", stderr);
        return 8;
    }
    if (!bytes_eq(icmp.bytes, icmp.size, net::ByteView{icmp_payload, sizeof(icmp_payload)})) {
        std::fputs("ipv4 smoke icmp payload mismatch\n", stderr);
        return 9;
    }
    if (!same_ipv4(icmp.source, peer_ip)
        || !same_ipv4(icmp.destination, local_ip)
        || icmp.protocol != net::Ipv4Protocol::icmp
        || icmp.ttl != 64) {
        std::fputs("ipv4 smoke icmp header forwarding mismatch\n", stderr);
        return 10;
    }

    static constexpr util::u8 udp_payload[]{0x13u, 0x88u, 0x17u, 0x70u, 0x00u, 0x08u, 0x00u, 0x00u};
    net::PacketBuffer<128> udp_packet{};
    auto encoded_udp = net::write_ipv4_packet(
        udp_packet,
        net::Ipv4PacketSpec{
            .identification = 0x3456u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 32,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer_ip,
            .destination = local_ip,
        },
        net::ByteView{udp_payload, sizeof(udp_payload)});
    if (!encoded_udp) {
        std::fputs("ipv4 smoke udp encode failed\n", stderr);
        return 11;
    }

    net::PacketBuffer<128> udp_frame{};
    auto wrote_udp_frame = write_ether_frame(
        udp_frame,
        net::MacAddress::broadcast(),
        local_mac,
        net::EtherType::ipv4,
        udp_packet.view().payload);
    if (!wrote_udp_frame) {
        std::fputs("ipv4 smoke udp ether encode failed\n", stderr);
        return 12;
    }

    link.queue_rx(udp_frame.view().payload);
    polled = stack.poll_links();
    if (!polled || ipv4.packet_count() != 2 || udp.calls != 1 || link.rx_pool.in_use_count() != 0) {
        std::fputs("ipv4 smoke udp dispatch failed\n", stderr);
        return 13;
    }
    if (!bytes_eq(udp.bytes, udp.size, net::ByteView{udp_payload, sizeof(udp_payload)})) {
        std::fputs("ipv4 smoke udp payload mismatch\n", stderr);
        return 14;
    }
    if (!same_ipv4(udp.source, peer_ip)
        || !same_ipv4(udp.destination, local_ip)
        || udp.protocol != net::Ipv4Protocol::udp
        || udp.ttl != 32) {
        std::fputs("ipv4 smoke udp header forwarding mismatch\n", stderr);
        return 15;
    }

    net::PacketBuffer<128> dropped_packet{};
    auto encoded_dropped = net::write_ipv4_packet(
        dropped_packet,
        net::Ipv4PacketSpec{
            .identification = 0x789Au,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 16,
            .protocol = net::Ipv4Protocol::icmp,
            .source = peer_ip,
            .destination = other_ip,
        },
        net::ByteView{icmp_payload, sizeof(icmp_payload)});
    if (!encoded_dropped) {
        std::fputs("ipv4 smoke drop encode failed\n", stderr);
        return 16;
    }

    net::PacketBuffer<128> dropped_frame{};
    auto wrote_dropped_frame = write_ether_frame(
        dropped_frame,
        net::MacAddress::broadcast(),
        local_mac,
        net::EtherType::ipv4,
        dropped_packet.view().payload);
    if (!wrote_dropped_frame) {
        std::fputs("ipv4 smoke drop ether encode failed\n", stderr);
        return 17;
    }

    link.queue_rx(dropped_frame.view().payload);
    polled = stack.poll_links();
    if (!polled || ipv4.packet_count() != 2 || ipv4.drop_count() != 1 || icmp.calls != 1 || udp.calls != 1) {
        std::fputs("ipv4 smoke drop filter failed\n", stderr);
        return 18;
    }

    net::PacketBuffer<128> invalid_packet{};
    auto encoded_invalid = net::write_ipv4_packet(
        invalid_packet,
        net::Ipv4PacketSpec{
            .identification = 0xBCDEu,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 8,
            .protocol = net::Ipv4Protocol::icmp,
            .source = peer_ip,
            .destination = local_ip,
        },
        net::ByteView{icmp_payload, sizeof(icmp_payload)});
    if (!encoded_invalid) {
        std::fputs("ipv4 smoke invalid encode failed\n", stderr);
        return 19;
    }
    invalid_packet.mut_view()[10] ^= 0x5Au;

    net::PacketBuffer<128> invalid_frame{};
    auto wrote_invalid_frame = write_ether_frame(
        invalid_frame,
        net::MacAddress::broadcast(),
        local_mac,
        net::EtherType::ipv4,
        invalid_packet.view().payload);
    if (!wrote_invalid_frame) {
        std::fputs("ipv4 smoke invalid ether encode failed\n", stderr);
        return 20;
    }

    link.queue_rx(invalid_frame.view().payload);
    polled = stack.poll_links();
    if (polled || polled.error() != net::errc::invalid_format) {
        std::fputs("ipv4 smoke checksum validation failed\n", stderr);
        return 21;
    }

    std::puts("net ipv4 smoke: ok");
    return 0;
}
