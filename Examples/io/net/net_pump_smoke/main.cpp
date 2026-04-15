#include <array>
#include <cstdio>

import charm.net;
import net.arp;
import net.driver;
import net.ether;
import net.ipv4;
import net.packet;
import net.udp;
import util.core;
import util.expected;

namespace {
    struct DatagramProbe {
        std::array<util::u8, 96> bytes{};
        util::usize size{0};
        util::usize calls{0};
        net::UdpDatagramInfo info{};

        [[nodiscard]] net::Result<void> consume(const net::UdpDatagramInfo& datagram,
                                                net::OwnedPacket packet) noexcept {
            const auto view = packet.view();
            if (view.size() > bytes.size()) {
                return util::unexpected(net::errc::buffer_overflow);
            }
            for (util::usize i = 0; i < view.size(); ++i) {
                bytes[i] = view[i];
            }
            size = view.size();
            info = datagram;
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
    constexpr auto inbound_peer_ip = net::IpAddress::ipv4(10, 0, 0, 7);
    constexpr auto timeout_peer_ip = net::IpAddress::ipv4(10, 0, 0, 19);
    constexpr auto local = net::Endpoint{net::IpAddress::ipv4_any(), 5000};
    constexpr auto peer = net::Endpoint{peer_ip, 7000};
    constexpr auto inbound_peer = net::Endpoint{inbound_peer_ip, 6100};
    constexpr auto timeout_peer = net::Endpoint{timeout_peer_ip, 7100};
    static constexpr util::u8 payload[]{'p', 'u', 'm', 'p'};
    static constexpr util::u8 inbound_payload[]{'p', 'i', 'n', 'g'};
    static constexpr util::u8 timeout_payload[]{'d', 'r', 'o', 'p'};

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
        std::fputs("net pump smoke netif configure failed\n", stderr);
        return 1;
    }

    StubLinkDriver link{};
    net::NetDriver driver{};
    auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
    if (!attached) {
        std::fputs("net pump smoke driver attach failed\n", stderr);
        return 2;
    }

    net::Stack stack{};
    auto registered = stack.register_driver(driver);
    if (!registered || stack.driver_count() != 1 || stack.netif_count() != 1) {
        std::fputs("net pump smoke stack register failed\n", stderr);
        return 3;
    }

    auto up = netif.bring_up();
    if (!up) {
        std::fputs("net pump smoke netif bring_up failed\n", stderr);
        return 4;
    }

    DatagramProbe probe{};
    net::UdpStackPump<128, 4, 128, 4, 16, 4> pump{
        stack,
        netif,
        net::UdpStackPumpConfig{
            .egress = net::UdpEgressPumpConfig{
                .retry_interval_ticks = 2,
                .max_attempts = 2,
            }
        }
    };
    auto bound_port = pump.bind_udp(5000, net::make_udp_datagram_sink_ref(probe));
    if (!bound_port || pump.udp_binding_count() != 1) {
        std::fputs("net pump smoke bind udp failed\n", stderr);
        return 5;
    }

    net::PacketBuffer<128> inbound_udp{};
    auto encoded_inbound_udp = net::write_udp_ipv4_datagram(
        inbound_udp,
        inbound_peer,
        net::Endpoint{local_ip, 5000},
        net::ByteView{inbound_payload, sizeof(inbound_payload)});
    if (!encoded_inbound_udp) {
        std::fputs("net pump smoke inbound datagram encode failed\n", stderr);
        return 6;
    }

    net::PacketBuffer<128> inbound_ipv4{};
    auto encoded_inbound_ipv4 = net::write_ipv4_packet(
        inbound_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x4321u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 52,
            .protocol = net::Ipv4Protocol::udp,
            .source = inbound_peer.address,
            .destination = local_ip,
        },
        inbound_udp.view().payload);
    if (!encoded_inbound_ipv4) {
        std::fputs("net pump smoke inbound ipv4 encode failed\n", stderr);
        return 7;
    }

    static constexpr util::u8 ether_padding[]{0x00u, 0x00u, 0x00u, 0x00u};
    net::PacketBuffer<128> inbound_frame{};
    auto wrote_inbound_frame = write_ether_frame(
        inbound_frame,
        net::MacAddress::broadcast(),
        local_mac,
        net::EtherType::ipv4,
        inbound_ipv4.view().payload,
        net::ByteView{ether_padding, sizeof(ether_padding)});
    if (!wrote_inbound_frame) {
        std::fputs("net pump smoke inbound ether encode failed\n", stderr);
        return 8;
    }

    link.queue_rx(inbound_frame.view().payload);
    auto inbound = pump.service(0);
    if (!inbound
        || !inbound.value().polled_links
        || inbound.value().ipv4_delivered != 1
        || inbound.value().ipv4_dropped != 0
        || inbound.value().udp_delivered != 1
        || inbound.value().udp_dropped != 0
        || inbound.value().arp_retried != 0
        || inbound.value().arp_timed_out != 0
        || inbound.value().egress_flushed != 0
        || inbound.value().egress_dropped != 0
        || probe.calls != 1
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("net pump smoke inbound dispatch failed\n", stderr);
        return 9;
    }
    if (!bytes_eq(probe.bytes, probe.size, net::ByteView{inbound_payload, sizeof(inbound_payload)})
        || !same_ipv4(probe.info.local.address, local_ip)
        || probe.info.local.port != 5000
        || !same_ipv4(probe.info.peer.address, inbound_peer_ip)
        || probe.info.peer.port != 6100) {
        std::fputs("net pump smoke inbound metadata failed\n", stderr);
        return 10;
    }

    auto seeded = pump.send(local, peer, net::ByteView{payload, sizeof(payload)}, 23, 0x1234u, 0x11u);
    if (!seeded
        || seeded.value() != net::UdpSendDisposition::queued
        || link.tx_calls != 1
        || pump.arp().request_count() != 1
        || pump.arp().pending_count() != 1
        || pump.pending_count() != 1) {
        std::fputs("net pump smoke initial queue failed\n", stderr);
        return 11;
    }

    net::PacketBuffer<128> reply{};
    auto wrote_reply = net::write_arp_ipv4_reply_frame(
        reply,
        local_mac,
        peer_mac,
        peer_ip,
        local_mac,
        local_ip);
    if (!wrote_reply) {
        std::fputs("net pump smoke arp reply encode failed\n", stderr);
        return 12;
    }

    link.queue_rx(reply.view().payload);
    auto flushed = pump.service(0);
    if (!flushed
        || !flushed.value().polled_links
        || flushed.value().ipv4_delivered != 0
        || flushed.value().udp_delivered != 0
        || flushed.value().arp_retried != 0
        || flushed.value().arp_timed_out != 0
        || flushed.value().egress_flushed != 1
        || flushed.value().egress_dropped != 0
        || link.tx_calls != 2
        || pump.arp().pending_count() != 0
        || pump.pending_count() != 0
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("net pump smoke poll and flush failed\n", stderr);
        return 13;
    }

    auto frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!frame
        || frame.value().type != net::EtherType::ipv4
        || !same_mac(frame.value().destination, peer_mac)) {
        std::fputs("net pump smoke ether parse failed\n", stderr);
        return 14;
    }

    auto ipv4 = net::parse_ipv4_packet(frame.value().payload);
    if (!ipv4
        || ipv4.value().ttl != 23
        || ipv4.value().identification != 0x1234u
        || ipv4.value().dscp_ecn != 0x11u
        || !same_ipv4(ipv4.value().destination, peer_ip)) {
        std::fputs("net pump smoke ipv4 payload failed\n", stderr);
        return 15;
    }

    auto udp = net::parse_udp_datagram(ipv4.value().payload);
    if (!udp
        || udp.value().destination_port != 7000
        || udp.value().payload.size() != sizeof(payload)) {
        std::fputs("net pump smoke udp payload failed\n", stderr);
        return 16;
    }
    for (util::usize i = 0; i < sizeof(payload); ++i) {
        if (udp.value().payload[i] != payload[i]) {
            std::fputs("net pump smoke udp payload bytes failed\n", stderr);
            return 17;
        }
    }

    auto timeout_seed = pump.send(local, timeout_peer, net::ByteView{timeout_payload, sizeof(timeout_payload)});
    if (!timeout_seed
        || timeout_seed.value() != net::UdpSendDisposition::queued
        || link.tx_calls != 3
        || pump.arp().request_count() != 2
        || pump.arp().pending_count() != 1
        || pump.pending_count() != 1) {
        std::fputs("net pump smoke timeout queue failed\n", stderr);
        return 18;
    }

    auto idle = pump.service(1);
    if (!idle
        || idle.value().arp_retried != 0
        || idle.value().arp_timed_out != 0
        || idle.value().egress_flushed != 0
        || idle.value().egress_dropped != 0
        || link.tx_calls != 3
        || pump.arp().pending_attempts(timeout_peer_ip) != 1) {
        std::fputs("net pump smoke idle service failed\n", stderr);
        return 19;
    }

    auto retried = pump.service(1);
    if (!retried
        || retried.value().arp_retried != 1
        || retried.value().arp_timed_out != 0
        || retried.value().egress_flushed != 0
        || retried.value().egress_dropped != 0
        || link.tx_calls != 4
        || pump.arp().pending_attempts(timeout_peer_ip) != 2) {
        std::fputs("net pump smoke retry service failed\n", stderr);
        return 20;
    }

    auto timed_out = pump.service(2);
    if (!timed_out
        || timed_out.value().arp_retried != 0
        || timed_out.value().arp_timed_out != 1
        || timed_out.value().egress_flushed != 0
        || timed_out.value().egress_dropped != 1
        || link.tx_calls != 4
        || pump.arp().pending_count() != 0
        || pump.arp().failed_count() != 1
        || pump.pending_count() != 0
        || pump.dropped_count() != 1) {
        std::fputs("net pump smoke timeout service failed\n", stderr);
        return 21;
    }

    std::puts("net pump smoke: ok");
    return 0;
}
