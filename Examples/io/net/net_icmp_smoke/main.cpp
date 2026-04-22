#include <array>
#include <cstdio>

import charm.net;
import net.arp;
import net.driver;
import net.ether;
import net.ipv4;
import net.packet;
import util.core;
import util.expected;

namespace {
    struct EchoProbe {
        std::array<util::u8, 96> bytes{};
        util::usize size{0};
        util::usize calls{0};
        net::IcmpEchoInfo info{};

        [[nodiscard]] net::Result<void> consume(const net::IcmpEchoInfo& echo,
                                                net::OwnedPacket packet) noexcept {
            const auto view = packet.view();
            if (view.size() > bytes.size()) {
                return util::unexpected(net::errc::buffer_overflow);
            }
            for (util::usize i = 0; i < view.size(); ++i) {
                bytes[i] = view[i];
            }
            size = view.size();
            info = echo;
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
                                                      net::ByteView payload,
                                                      net::ByteView padding = {}) noexcept {
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

        auto appended_padding = packet.append(padding);
        if (!appended_padding) {
            return util::unexpected(appended_padding.error());
        }
        return {};
    }

    template <util::usize Capacity>
    [[nodiscard]] net::Result<void> write_ipv4_ether_frame(net::PacketBuffer<Capacity>& frame,
                                                           net::MacAddress destination_mac,
                                                           net::MacAddress source_mac,
                                                           const net::Ipv4PacketSpec& spec,
                                                           net::ByteView payload,
                                                           net::ByteView padding = {}) noexcept {
        net::PacketBuffer<Capacity> ipv4_packet{};
        auto encoded_ipv4 = net::write_ipv4_packet(ipv4_packet, spec, payload);
        if (!encoded_ipv4) {
            return util::unexpected(encoded_ipv4.error());
        }

        return write_ether_frame(
            frame,
            destination_mac,
            source_mac,
            net::EtherType::ipv4,
            ipv4_packet.view().payload,
            padding);
    }

    template <util::usize N>
    [[nodiscard]] bool bytes_eq(const std::array<util::u8, N>& lhs,
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

    template <util::usize N>
    [[nodiscard]] bool bytes_eq(const util::u8 (&lhs)[N],
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

    template <util::usize N>
    [[nodiscard]] bool bytes_eq(const std::array<util::u8, N>& lhs,
                                util::usize lhs_size,
                                net::PacketView rhs) noexcept {
        return bytes_eq(lhs, lhs_size, rhs.payload);
    }

    template <util::usize N>
    [[nodiscard]] bool bytes_eq(const util::u8 (&lhs)[N],
                                util::usize lhs_size,
                                net::PacketView rhs) noexcept {
        return bytes_eq(lhs, lhs_size, rhs.payload);
    }

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
    static constexpr util::u8 request_payload[]{'p', 'i', 'n', 'g'};
    static constexpr util::u8 reply_payload[]{'p', 'o', 'n', 'g'};
    static constexpr util::u8 egress_payload[]{'e', 'c', 'h', 'o'};
    static constexpr util::u8 ether_padding[]{0x00u, 0x00u, 0x00u, 0x00u};

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
        std::fputs("icmp smoke netif configure failed\n", stderr);
        return 1;
    }

    StubLinkDriver link{};
    net::NetDriver driver{};
    auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
    if (!attached) {
        std::fputs("icmp smoke driver attach failed\n", stderr);
        return 2;
    }

    net::Stack stack{};
    net::ArpService<4, 128> arp{netif};
    net::Ipv4Service ipv4{netif};
    net::IcmpEchoService icmp{};
    EchoProbe probe{};
    icmp.set_sink(probe);
    ipv4.set_icmp_sink(net::make_ipv4_packet_sink_ref(icmp));
    stack.set_arp_sink(net::make_owned_packet_sink_ref(arp));
    stack.set_ipv4_sink(net::make_owned_packet_sink_ref(ipv4));

    auto registered = stack.register_driver(driver);
    if (!registered || stack.driver_count() != 1 || stack.netif_count() != 1) {
        std::fputs("icmp smoke stack register failed\n", stderr);
        return 3;
    }

    auto up = netif.bring_up();
    if (!up) {
        std::fputs("icmp smoke netif bring_up failed\n", stderr);
        return 4;
    }

    net::PacketBuffer<128> request_packet{};
    auto encoded_request = net::write_icmp_echo_request(
        request_packet,
        0x1357u,
        0x0002u,
        net::ByteView{request_payload, sizeof(request_payload)});
    if (!encoded_request) {
        std::fputs("icmp smoke request encode failed\n", stderr);
        return 5;
    }

    auto parsed_request = net::parse_icmp_echo_packet(request_packet.view());
    if (!parsed_request
        || parsed_request.value().type != net::IcmpType::echo_request
        || parsed_request.value().identifier != 0x1357u
        || parsed_request.value().sequence != 0x0002u
        || parsed_request.value().checksum == 0u
        || !bytes_eq(request_payload, sizeof(request_payload), parsed_request.value().payload)) {
        std::fputs("icmp smoke request parse mismatch\n", stderr);
        return 6;
    }

    net::PacketBuffer<128> request_frame{};
    auto wrote_request_frame = write_ipv4_ether_frame(
        request_frame,
        local_mac,
        peer_mac,
        net::Ipv4PacketSpec{
            .identification = 0x1111u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 64,
            .protocol = net::Ipv4Protocol::icmp,
            .source = peer_ip,
            .destination = local_ip,
        },
        request_packet.view().payload,
        net::ByteView{ether_padding, sizeof(ether_padding)});
    if (!wrote_request_frame) {
        std::fputs("icmp smoke request frame encode failed\n", stderr);
        return 7;
    }

    link.queue_rx(request_frame.view().payload);
    auto polled = stack.poll_links();
    if (!polled
        || ipv4.packet_count() != 1
        || ipv4.drop_count() != 0
        || icmp.packet_count() != 1
        || icmp.request_count() != 1
        || icmp.reply_count() != 0
        || icmp.drop_count() != 0
        || probe.calls != 1
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("icmp smoke request ingress failed\n", stderr);
        return 8;
    }
    if (probe.info.type != net::IcmpType::echo_request
        || !same_ipv4(probe.info.local, local_ip)
        || !same_ipv4(probe.info.peer, peer_ip)
        || probe.info.identifier != 0x1357u
        || probe.info.sequence != 0x0002u
        || !bytes_eq(probe.bytes, probe.size, net::ByteView{request_payload, sizeof(request_payload)})) {
        std::fputs("icmp smoke request ingress metadata failed\n", stderr);
        return 9;
    }

    net::PacketBuffer<128> reply_packet{};
    auto encoded_reply = net::write_icmp_echo_reply(
        reply_packet,
        0x2468u,
        0x0003u,
        net::ByteView{reply_payload, sizeof(reply_payload)});
    if (!encoded_reply) {
        std::fputs("icmp smoke reply encode failed\n", stderr);
        return 10;
    }

    net::PacketBuffer<128> reply_frame{};
    auto wrote_reply_frame = write_ipv4_ether_frame(
        reply_frame,
        local_mac,
        peer_mac,
        net::Ipv4PacketSpec{
            .identification = 0x2222u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 48,
            .protocol = net::Ipv4Protocol::icmp,
            .source = peer_ip,
            .destination = local_ip,
        },
        reply_packet.view().payload);
    if (!wrote_reply_frame) {
        std::fputs("icmp smoke reply frame encode failed\n", stderr);
        return 11;
    }

    link.queue_rx(reply_frame.view().payload);
    polled = stack.poll_links();
    if (!polled
        || ipv4.packet_count() != 2
        || icmp.packet_count() != 2
        || icmp.request_count() != 1
        || icmp.reply_count() != 1
        || icmp.drop_count() != 0
        || probe.calls != 2
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("icmp smoke reply ingress failed\n", stderr);
        return 12;
    }
    if (probe.info.type != net::IcmpType::echo_reply
        || !same_ipv4(probe.info.local, local_ip)
        || !same_ipv4(probe.info.peer, peer_ip)
        || probe.info.identifier != 0x2468u
        || probe.info.sequence != 0x0003u
        || !bytes_eq(probe.bytes, probe.size, net::ByteView{reply_payload, sizeof(reply_payload)})) {
        std::fputs("icmp smoke reply ingress metadata failed\n", stderr);
        return 13;
    }

    net::PacketBuffer<128> quoted_ipv4_packet{};
    auto encoded_quoted_ipv4 = net::write_ipv4_packet(
        quoted_ipv4_packet,
        net::Ipv4PacketSpec{
            .identification = 0x9999u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 1,
            .protocol = net::Ipv4Protocol::icmp,
            .source = local_ip,
            .destination = peer_ip,
        },
        request_packet.view().payload);
    if (!encoded_quoted_ipv4) {
        std::fputs("icmp smoke quoted ipv4 encode failed\n", stderr);
        return 14;
    }

    const auto quoted_prefix = quoted_ipv4_packet.view().payload.subspan(
        0,
        net::ipv4_min_header_size() + 8u);

    net::PacketBuffer<128> time_exceeded_packet{};
    auto encoded_time_exceeded = net::write_icmp_time_exceeded_packet(
        time_exceeded_packet,
        0u,
        quoted_prefix);
    if (!encoded_time_exceeded) {
        std::fputs("icmp smoke time exceeded encode failed\n", stderr);
        return 15;
    }

    auto parsed_time_exceeded = net::parse_icmp_error_quote_packet(time_exceeded_packet.view());
    if (!parsed_time_exceeded
        || parsed_time_exceeded.value().type != net::IcmpType::time_exceeded
        || parsed_time_exceeded.value().code != 0u
        || parsed_time_exceeded.value().reserved != 0u
        || parsed_time_exceeded.value().quoted_ipv4.identification != 0x9999u
        || parsed_time_exceeded.value().quoted_ipv4.ttl != 1u
        || parsed_time_exceeded.value().quoted_ipv4.protocol != net::Ipv4Protocol::icmp
        || !same_ipv4(parsed_time_exceeded.value().quoted_ipv4.source, local_ip)
        || !same_ipv4(parsed_time_exceeded.value().quoted_ipv4.destination, peer_ip)
        || parsed_time_exceeded.value().quoted_ipv4.total_length != quoted_ipv4_packet.view().size()
        || parsed_time_exceeded.value().quoted_ipv4.payload.size() != 8u
        || parsed_time_exceeded.value().quoted_ipv4.payload[0] != request_packet.view()[0]
        || parsed_time_exceeded.value().quoted_ipv4.payload[1] != request_packet.view()[1]
        || parsed_time_exceeded.value().quoted_ipv4.payload[2] != request_packet.view()[2]
        || parsed_time_exceeded.value().quoted_ipv4.payload[3] != request_packet.view()[3]
        || parsed_time_exceeded.value().quoted_ipv4.payload[4] != request_packet.view()[4]
        || parsed_time_exceeded.value().quoted_ipv4.payload[5] != request_packet.view()[5]
        || parsed_time_exceeded.value().quoted_ipv4.payload[6] != request_packet.view()[6]
        || parsed_time_exceeded.value().quoted_ipv4.payload[7] != request_packet.view()[7]) {
        std::fputs("icmp smoke time exceeded parse mismatch\n", stderr);
        return 16;
    }

    net::PacketBuffer<128> time_exceeded_frame{};
    auto wrote_time_exceeded_frame = write_ipv4_ether_frame(
        time_exceeded_frame,
        local_mac,
        peer_mac,
        net::Ipv4PacketSpec{
            .identification = 0x3333u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 32,
            .protocol = net::Ipv4Protocol::icmp,
            .source = peer_ip,
            .destination = local_ip,
        },
        time_exceeded_packet.view().payload);
    if (!wrote_time_exceeded_frame) {
        std::fputs("icmp smoke time exceeded frame encode failed\n", stderr);
        return 17;
    }

    link.queue_rx(time_exceeded_frame.view().payload);
    polled = stack.poll_links();
    if (!polled
        || ipv4.packet_count() != 3
        || icmp.packet_count() != 2
        || icmp.request_count() != 1
        || icmp.reply_count() != 1
        || icmp.drop_count() != 1
        || probe.calls != 2
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("icmp smoke time exceeded drop failed\n", stderr);
        return 18;
    }

    net::PacketBuffer<128> invalid_packet{};
    auto encoded_invalid = net::write_icmp_echo_request(
        invalid_packet,
        0xAAAAu,
        0x0004u,
        net::ByteView{request_payload, sizeof(request_payload)});
    if (!encoded_invalid) {
        std::fputs("icmp smoke invalid encode failed\n", stderr);
        return 17;
    }
    invalid_packet.mut_view()[2] ^= 0x5Au;

    net::PacketBuffer<128> invalid_frame{};
    auto wrote_invalid_frame = write_ipv4_ether_frame(
        invalid_frame,
        local_mac,
        peer_mac,
        net::Ipv4PacketSpec{
            .identification = 0x4444u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 28,
            .protocol = net::Ipv4Protocol::icmp,
            .source = peer_ip,
            .destination = local_ip,
        },
        invalid_packet.view().payload);
    if (!wrote_invalid_frame) {
        std::fputs("icmp smoke invalid frame encode failed\n", stderr);
        return 19;
    }

    link.queue_rx(invalid_frame.view().payload);
    polled = stack.poll_links();
    if (polled
        || polled.error() != net::errc::invalid_format
        || ipv4.packet_count() != 4
        || icmp.packet_count() != 2
        || icmp.drop_count() != 1
        || probe.calls != 2
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("icmp smoke invalid checksum failed\n", stderr);
        return 20;
    }

    auto unresolved = net::send_icmp_echo_request<128>(
        netif,
        arp,
        net::IpAddress::ipv4_any(),
        peer_ip,
        0xBEEFu,
        0x0007u,
        net::ByteView{egress_payload, sizeof(egress_payload)},
        23,
        0x4567u,
        0x11u);
    if (unresolved
        || unresolved.error() != net::errc::again
        || link.tx_calls != 1
        || arp.request_count() != 1
        || arp.pending_count() != 1) {
        std::fputs("icmp smoke unresolved egress failed\n", stderr);
        return 21;
    }

    auto arp_request_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!arp_request_frame
        || arp_request_frame.value().type != net::EtherType::arp
        || !same_mac(arp_request_frame.value().destination, net::MacAddress::broadcast())
        || !same_mac(arp_request_frame.value().source, local_mac)) {
        std::fputs("icmp smoke arp request ether parse failed\n", stderr);
        return 22;
    }

    auto arp_request = net::parse_arp_ipv4_ethernet(arp_request_frame.value().payload);
    if (!arp_request
        || arp_request.value().operation != net::ArpOperation::request
        || !same_mac(arp_request.value().sender_mac, local_mac)
        || !same_ipv4(arp_request.value().sender_ip, local_ip)
        || !same_ipv4(arp_request.value().target_ip, peer_ip)) {
        std::fputs("icmp smoke arp request payload failed\n", stderr);
        return 23;
    }

    net::PacketBuffer<128> arp_reply{};
    auto wrote_arp_reply = net::write_arp_ipv4_reply_frame(
        arp_reply,
        local_mac,
        peer_mac,
        peer_ip,
        local_mac,
        local_ip);
    if (!wrote_arp_reply) {
        std::fputs("icmp smoke arp reply encode failed\n", stderr);
        return 24;
    }

    link.queue_rx(arp_reply.view().payload);
    polled = stack.poll_links();
    auto resolved = arp.table().lookup(peer_ip);
    if (!polled
        || !resolved
        || !same_mac(resolved.value(), peer_mac)
        || arp.pending_count() != 0
        || arp.request_count() != 1
        || link.tx_calls != 1
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("icmp smoke arp resolution failed\n", stderr);
        return 25;
    }

    auto sent = net::send_icmp_echo_request<128>(
        netif,
        arp,
        net::IpAddress::ipv4_any(),
        peer_ip,
        0xBEEFu,
        0x0007u,
        net::ByteView{egress_payload, sizeof(egress_payload)},
        23,
        0x4567u,
        0x11u);
    if (!sent
        || link.tx_calls != 2
        || arp.request_count() != 1
        || arp.pending_count() != 0) {
        std::fputs("icmp smoke resolved egress failed\n", stderr);
        return 26;
    }

    auto egress_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!egress_frame
        || egress_frame.value().type != net::EtherType::ipv4
        || !same_mac(egress_frame.value().destination, peer_mac)
        || !same_mac(egress_frame.value().source, local_mac)) {
        std::fputs("icmp smoke egress ether parse failed\n", stderr);
        return 27;
    }

    auto egress_ipv4 = net::parse_ipv4_packet(egress_frame.value().payload);
    if (!egress_ipv4
        || egress_ipv4.value().protocol != net::Ipv4Protocol::icmp
        || egress_ipv4.value().ttl != 23
        || egress_ipv4.value().identification != 0x4567u
        || egress_ipv4.value().dscp_ecn != 0x11u
        || !same_ipv4(egress_ipv4.value().source, local_ip)
        || !same_ipv4(egress_ipv4.value().destination, peer_ip)) {
        std::fputs("icmp smoke egress ipv4 payload failed\n", stderr);
        return 28;
    }

    auto egress_icmp = net::parse_icmp_echo_packet(egress_ipv4.value().payload);
    if (!egress_icmp
        || egress_icmp.value().type != net::IcmpType::echo_request
        || egress_icmp.value().identifier != 0xBEEFu
        || egress_icmp.value().sequence != 0x0007u
        || !bytes_eq(egress_payload, sizeof(egress_payload), egress_icmp.value().payload)) {
        std::fputs("icmp smoke egress icmp payload failed\n", stderr);
        return 29;
    }

    std::puts("net icmp smoke: ok");
    return 0;
}
