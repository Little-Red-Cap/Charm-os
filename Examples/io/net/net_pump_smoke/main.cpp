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
    constexpr auto timeout_peer_ip = net::IpAddress::ipv4(10, 0, 0, 19);
    constexpr auto local = net::Endpoint{net::IpAddress::ipv4_any(), 5000};
    constexpr auto peer = net::Endpoint{peer_ip, 7000};
    constexpr auto timeout_peer = net::Endpoint{timeout_peer_ip, 7100};
    static constexpr util::u8 payload[]{'p', 'u', 'm', 'p'};
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
    net::ArpService<4, 128> arp{netif};
    stack.set_arp_sink(net::make_owned_packet_sink_ref(arp));
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

    net::UdpEgressPump<128, 4, 128, 4, 16> pump{
        stack,
        netif,
        arp,
        net::UdpEgressPumpConfig{
            .retry_interval_ticks = 2,
            .max_attempts = 2,
        }
    };

    auto seeded = pump.send(local, peer, net::ByteView{payload, sizeof(payload)}, 23, 0x1234u, 0x11u);
    if (!seeded
        || seeded.value() != net::UdpSendDisposition::queued
        || link.tx_calls != 1
        || arp.request_count() != 1
        || arp.pending_count() != 1
        || pump.pending_count() != 1) {
        std::fputs("net pump smoke initial queue failed\n", stderr);
        return 5;
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
        return 6;
    }

    link.queue_rx(reply.view().payload);
    auto flushed = pump.service(0);
    if (!flushed
        || !flushed.value().polled_links
        || flushed.value().arp_retried != 0
        || flushed.value().arp_timed_out != 0
        || flushed.value().flushed != 1
        || flushed.value().dropped != 0
        || link.tx_calls != 2
        || arp.pending_count() != 0
        || pump.pending_count() != 0
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("net pump smoke poll and flush failed\n", stderr);
        return 7;
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
        return 8;
    }

    auto ipv4 = net::parse_ipv4_packet(frame.value().payload);
    if (!ipv4
        || ipv4.value().ttl != 23
        || ipv4.value().identification != 0x1234u
        || ipv4.value().dscp_ecn != 0x11u
        || !same_ipv4(ipv4.value().destination, peer_ip)) {
        std::fputs("net pump smoke ipv4 payload failed\n", stderr);
        return 9;
    }

    auto udp = net::parse_udp_datagram(ipv4.value().payload);
    if (!udp
        || udp.value().destination_port != 7000
        || udp.value().payload.size() != sizeof(payload)) {
        std::fputs("net pump smoke udp payload failed\n", stderr);
        return 10;
    }
    for (util::usize i = 0; i < sizeof(payload); ++i) {
        if (udp.value().payload[i] != payload[i]) {
            std::fputs("net pump smoke udp payload bytes failed\n", stderr);
            return 11;
        }
    }

    auto timeout_seed = pump.send(local, timeout_peer, net::ByteView{timeout_payload, sizeof(timeout_payload)});
    if (!timeout_seed
        || timeout_seed.value() != net::UdpSendDisposition::queued
        || link.tx_calls != 3
        || arp.request_count() != 2
        || arp.pending_count() != 1
        || pump.pending_count() != 1) {
        std::fputs("net pump smoke timeout queue failed\n", stderr);
        return 12;
    }

    auto idle = pump.service(1);
    if (!idle
        || idle.value().arp_retried != 0
        || idle.value().arp_timed_out != 0
        || idle.value().flushed != 0
        || idle.value().dropped != 0
        || link.tx_calls != 3
        || arp.pending_attempts(timeout_peer_ip) != 1) {
        std::fputs("net pump smoke idle service failed\n", stderr);
        return 13;
    }

    auto retried = pump.service(1);
    if (!retried
        || retried.value().arp_retried != 1
        || retried.value().arp_timed_out != 0
        || retried.value().flushed != 0
        || retried.value().dropped != 0
        || link.tx_calls != 4
        || arp.pending_attempts(timeout_peer_ip) != 2) {
        std::fputs("net pump smoke retry service failed\n", stderr);
        return 14;
    }

    auto timed_out = pump.service(2);
    if (!timed_out
        || timed_out.value().arp_retried != 0
        || timed_out.value().arp_timed_out != 1
        || timed_out.value().flushed != 0
        || timed_out.value().dropped != 1
        || link.tx_calls != 4
        || arp.pending_count() != 0
        || arp.failed_count() != 1
        || pump.pending_count() != 0
        || pump.dropped_count() != 1) {
        std::fputs("net pump smoke timeout service failed\n", stderr);
        return 15;
    }

    std::puts("net pump smoke: ok");
    return 0;
}
