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
    constexpr auto queued_peer_mac = net::MacAddress::from_bytes(0x02u, 0x99u, 0x88u, 0x77u, 0x66u, 0x55u);
    constexpr auto queued_peer_ip = net::IpAddress::ipv4(10, 0, 0, 19);
    constexpr auto local = net::Endpoint{net::IpAddress::ipv4_any(), 5000};
    constexpr auto peer = net::Endpoint{peer_ip, 7000};
    constexpr auto queued_peer = net::Endpoint{queued_peer_ip, 7100};
    static constexpr util::u8 payload[]{'p', 'o', 'n', 'g'};
    static constexpr util::u8 queued_payload[]{'q', 'u', 'e', 'u', 'e'};

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
    net::ArpService<4, 128> arp{netif};
    stack.set_arp_sink(net::make_owned_packet_sink_ref(arp));
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
    auto unresolved_send = net::send_udp_ipv4<128>(
        netif,
        unresolved,
        local,
        peer,
        net::ByteView{payload, sizeof(payload)});
    if (unresolved_send || unresolved_send.error() != net::errc::noent || link.tx_calls != 0) {
        std::fputs("udp egress smoke unresolved arp table check failed\n", stderr);
        return 5;
    }

    auto missing = net::send_udp_ipv4<128>(netif, arp, local, peer, net::ByteView{payload, sizeof(payload)});
    if (missing
        || missing.error() != net::errc::again
        || link.tx_calls != 1
        || arp.request_count() != 1
        || arp.pending_count() != 1) {
        std::fputs("udp egress smoke arp request trigger failed\n", stderr);
        return 6;
    }

    auto duplicate = net::send_udp_ipv4<128>(netif, arp, local, peer, net::ByteView{payload, sizeof(payload)});
    if (duplicate
        || duplicate.error() != net::errc::again
        || link.tx_calls != 1
        || arp.request_count() != 1
        || arp.pending_count() != 1) {
        std::fputs("udp egress smoke arp pending dedup failed\n", stderr);
        return 7;
    }

    auto request_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!request_frame || request_frame.value().type != net::EtherType::arp) {
        std::fputs("udp egress smoke arp request ether parse failed\n", stderr);
        return 8;
    }
    if (!same_mac(request_frame.value().destination, net::MacAddress::broadcast())
        || !same_mac(request_frame.value().source, local_mac)) {
        std::fputs("udp egress smoke arp request ether mismatch\n", stderr);
        return 9;
    }

    auto request_arp = net::parse_arp_ipv4_ethernet(request_frame.value().payload);
    if (!request_arp || request_arp.value().operation != net::ArpOperation::request) {
        std::fputs("udp egress smoke arp request parse failed\n", stderr);
        return 10;
    }
    if (!same_mac(request_arp.value().sender_mac, local_mac)
        || !same_ipv4(request_arp.value().sender_ip, local_ip)
        || !request_arp.value().target_mac.is_zero()
        || !same_ipv4(request_arp.value().target_ip, peer_ip)) {
        std::fputs("udp egress smoke arp request fields mismatch\n", stderr);
        return 11;
    }

    net::PacketBuffer<128> incoming_reply{};
    auto wrote_reply = net::write_arp_ipv4_reply_frame(
        incoming_reply,
        local_mac,
        peer_mac,
        peer_ip,
        local_mac,
        local_ip);
    if (!wrote_reply) {
        std::fputs("udp egress smoke arp reply encode failed\n", stderr);
        return 12;
    }

    link.queue_rx(incoming_reply.view().payload);
    auto polled = stack.poll_links();
    if (!polled
        || link.tx_calls != 1
        || arp.reply_count() != 0
        || arp.pending_count() != 0
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("udp egress smoke arp reply handling failed\n", stderr);
        return 13;
    }

    auto resolved = arp.table().lookup(peer_ip);
    if (!resolved || !same_mac(resolved.value(), peer_mac)) {
        std::fputs("udp egress smoke arp cache fill failed\n", stderr);
        return 14;
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
    if (!sent || link.tx_calls != 2) {
        std::fputs("udp egress smoke send failed\n", stderr);
        return 15;
    }

    auto frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!frame || frame.value().type != net::EtherType::ipv4) {
        std::fputs("udp egress smoke ether parse failed\n", stderr);
        return 16;
    }
    if (!same_mac(frame.value().destination, peer_mac)
        || !same_mac(frame.value().source, local_mac)) {
        std::fputs("udp egress smoke ether header mismatch\n", stderr);
        return 17;
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
        return 18;
    }

    auto udp = net::parse_udp_datagram(ipv4.value().payload);
    if (!udp
        || udp.value().source_port != 5000
        || udp.value().destination_port != 7000
        || udp.value().length != net::udp_header_size() + sizeof(payload)
        || udp.value().checksum == 0u) {
        std::fputs("udp egress smoke udp fields mismatch\n", stderr);
        return 19;
    }

    for (util::usize i = 0; i < sizeof(payload); ++i) {
        if (udp.value().payload[i] != payload[i]) {
            std::fputs("udp egress smoke payload mismatch\n", stderr);
            return 20;
        }
    }

    net::UdpEgressQueue<4, 16> egress_queue{};
    auto queued = egress_queue.send<128>(
        netif,
        arp,
        local,
        queued_peer,
        net::ByteView{queued_payload, sizeof(queued_payload)},
        32,
        0x1357u,
        0x11u);
    if (!queued
        || queued.value() != net::UdpSendDisposition::queued
        || link.tx_calls != 3
        || arp.request_count() != 2
        || arp.pending_count() != 1
        || egress_queue.pending_count() != 1
        || egress_queue.queued_count() != 1) {
        std::fputs("udp egress smoke queue send failed\n", stderr);
        return 21;
    }

    auto queued_request_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!queued_request_frame || queued_request_frame.value().type != net::EtherType::arp) {
        std::fputs("udp egress smoke queued request ether parse failed\n", stderr);
        return 22;
    }

    auto queued_request_arp = net::parse_arp_ipv4_ethernet(queued_request_frame.value().payload);
    if (!queued_request_arp
        || queued_request_arp.value().operation != net::ArpOperation::request
        || !same_ipv4(queued_request_arp.value().target_ip, queued_peer_ip)) {
        std::fputs("udp egress smoke queued request arp mismatch\n", stderr);
        return 23;
    }

    auto flush_before_reply = egress_queue.flush<128>(netif, arp);
    if (!flush_before_reply
        || flush_before_reply.value() != 0
        || link.tx_calls != 3
        || arp.request_count() != 2
        || arp.pending_count() != 1
        || egress_queue.pending_count() != 1) {
        std::fputs("udp egress smoke flush before reply failed\n", stderr);
        return 24;
    }

    net::PacketBuffer<128> queued_reply{};
    auto wrote_queued_reply = net::write_arp_ipv4_reply_frame(
        queued_reply,
        local_mac,
        queued_peer_mac,
        queued_peer_ip,
        local_mac,
        local_ip);
    if (!wrote_queued_reply) {
        std::fputs("udp egress smoke queued reply encode failed\n", stderr);
        return 25;
    }

    link.queue_rx(queued_reply.view().payload);
    polled = stack.poll_links();
    if (!polled
        || link.tx_calls != 3
        || arp.pending_count() != 0
        || egress_queue.pending_count() != 1
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("udp egress smoke queued reply handling failed\n", stderr);
        return 26;
    }

    auto flushed = egress_queue.flush<128>(netif, arp);
    if (!flushed
        || flushed.value() != 1
        || link.tx_calls != 4
        || egress_queue.pending_count() != 0
        || egress_queue.flushed_count() != 1) {
        std::fputs("udp egress smoke queue flush failed\n", stderr);
        return 27;
    }

    auto queued_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!queued_frame || queued_frame.value().type != net::EtherType::ipv4) {
        std::fputs("udp egress smoke queued ether parse failed\n", stderr);
        return 28;
    }
    if (!same_mac(queued_frame.value().destination, queued_peer_mac)
        || !same_mac(queued_frame.value().source, local_mac)) {
        std::fputs("udp egress smoke queued ether header mismatch\n", stderr);
        return 29;
    }

    auto queued_ipv4 = net::parse_ipv4_packet(queued_frame.value().payload);
    if (!queued_ipv4
        || queued_ipv4.value().protocol != net::Ipv4Protocol::udp
        || queued_ipv4.value().ttl != 32
        || queued_ipv4.value().identification != 0x1357u
        || queued_ipv4.value().dscp_ecn != 0x11u
        || !same_ipv4(queued_ipv4.value().source, local_ip)
        || !same_ipv4(queued_ipv4.value().destination, queued_peer_ip)) {
        std::fputs("udp egress smoke queued ipv4 fields mismatch\n", stderr);
        return 30;
    }

    auto queued_udp = net::parse_udp_datagram(queued_ipv4.value().payload);
    if (!queued_udp
        || queued_udp.value().source_port != 5000
        || queued_udp.value().destination_port != 7100
        || queued_udp.value().length != net::udp_header_size() + sizeof(queued_payload)
        || queued_udp.value().checksum == 0u) {
        std::fputs("udp egress smoke queued udp fields mismatch\n", stderr);
        return 31;
    }

    for (util::usize i = 0; i < sizeof(queued_payload); ++i) {
        if (queued_udp.value().payload[i] != queued_payload[i]) {
            std::fputs("udp egress smoke queued payload mismatch\n", stderr);
            return 32;
        }
    }

    std::puts("net udp egress smoke: ok");
    return 0;
}
