#include <array>
#include <cstdio>

import net.arp;
import net.driver;
import net.ether;
import net.packet;
import net.stack;
import util.core;
import util.expected;

namespace {
    struct StubLinkDriver {
        net::OwnedPacketSinkRef input_sink{};
        net::PacketPool<2, 96> rx_pool{};
        std::array<util::u8, 96> rx_bytes{};
        util::usize rx_size{0};
        bool rx_ready{false};

        std::array<util::u8, 96> tx_bytes{};
        util::usize tx_size{0};
        util::usize tx_calls{0};

        [[nodiscard]] net::NetDriverInfo info() const noexcept {
            return net::NetDriverInfo{
                .mtu = 96,
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
                static_cast<net::PacketPool<2, 96>::Lease&&>(lease.value())
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

    [[nodiscard]] bool same_mac(const net::MacAddress& lhs, const net::MacAddress& rhs) noexcept {
        return net::is_same_mac(lhs, rhs);
    }

    [[nodiscard]] bool same_ipv4(const net::IpAddress& lhs, const net::IpAddress& rhs) noexcept {
        return net::is_same_ipv4_address(lhs, rhs);
    }
}

int main() {
    constexpr auto local_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto local_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto peer_mac = net::MacAddress::from_bytes(0x02u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu);
    constexpr auto peer_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    constexpr auto other_mac = net::MacAddress::from_bytes(0x02u, 0x10u, 0x20u, 0x30u, 0x40u, 0x50u);
    constexpr auto other_ip = net::IpAddress::ipv4(10, 0, 0, 12);
    constexpr auto retry_mac = net::MacAddress::from_bytes(0x02u, 0x77u, 0x66u, 0x55u, 0x44u, 0x33u);
    constexpr auto retry_ip = net::IpAddress::ipv4(10, 0, 0, 21);
    constexpr auto auto_mac = net::MacAddress::from_bytes(0x02u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u);
    constexpr auto auto_ip = net::IpAddress::ipv4(10, 0, 0, 31);

    net::NetIf netif{};
    auto configured = netif.configure(net::NetIfConfig{
        .mtu = 96,
        .mac = local_mac,
        .address = local_ip,
        .capabilities = net::NetIfCapability::rx
            | net::NetIfCapability::tx
            | net::NetIfCapability::broadcast
    });
    if (!configured) {
        std::fputs("arp smoke netif configure failed\n", stderr);
        return 1;
    }

    StubLinkDriver link{};
    net::NetDriver driver{};
    auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
    if (!attached) {
        std::fputs("arp smoke driver attach failed\n", stderr);
        return 2;
    }

    net::Stack stack{};
    net::ArpService<4, 96> arp{netif};
    stack.set_arp_sink(net::make_owned_packet_sink_ref(arp));
    auto registered = stack.register_driver(driver);
    if (!registered || stack.driver_count() != 1 || stack.netif_count() != 1) {
        std::fputs("arp smoke stack register failed\n", stderr);
        return 3;
    }

    auto up = netif.bring_up();
    if (!up) {
        std::fputs("arp smoke netif bring_up failed\n", stderr);
        return 4;
    }

    net::PacketBuffer<96> request{};
    auto wrote_request = net::write_arp_ipv4_request_frame(request, peer_mac, peer_ip, local_ip);
    if (!wrote_request) {
        std::fputs("arp smoke request encode failed\n", stderr);
        return 5;
    }

    link.queue_rx(request.view().payload);
    auto polled = stack.poll_links();
    if (!polled || arp.reply_count() != 1 || link.tx_calls != 1 || link.rx_pool.in_use_count() != 0) {
        std::fputs("arp smoke request handling failed\n", stderr);
        return 6;
    }

    auto cached_peer = arp.table().lookup(peer_ip);
    if (!cached_peer || !same_mac(cached_peer.value(), peer_mac)) {
        std::fputs("arp smoke cache update from request failed\n", stderr);
        return 7;
    }

    auto reply_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!reply_frame || reply_frame.value().type != net::EtherType::arp) {
        std::fputs("arp smoke reply ether parse failed\n", stderr);
        return 8;
    }
    if (!same_mac(reply_frame.value().destination, peer_mac)
        || !same_mac(reply_frame.value().source, local_mac)) {
        std::fputs("arp smoke reply ether header mismatch\n", stderr);
        return 9;
    }

    auto reply_arp = net::parse_arp_ipv4_ethernet(reply_frame.value().payload);
    if (!reply_arp || reply_arp.value().operation != net::ArpOperation::reply) {
        std::fputs("arp smoke reply arp parse failed\n", stderr);
        return 10;
    }
    if (!same_mac(reply_arp.value().sender_mac, local_mac)
        || !same_ipv4(reply_arp.value().sender_ip, local_ip)
        || !same_mac(reply_arp.value().target_mac, peer_mac)
        || !same_ipv4(reply_arp.value().target_ip, peer_ip)) {
        std::fputs("arp smoke reply arp fields mismatch\n", stderr);
        return 11;
    }

    net::PacketBuffer<96> incoming_reply{};
    auto wrote_reply = net::write_arp_ipv4_reply_frame(
        incoming_reply,
        local_mac,
        other_mac,
        other_ip,
        local_mac,
        local_ip);
    if (!wrote_reply) {
        std::fputs("arp smoke reply encode failed\n", stderr);
        return 12;
    }

    link.queue_rx(incoming_reply.view().payload);
    polled = stack.poll_links();
    if (!polled || arp.reply_count() != 1 || link.tx_calls != 1) {
        std::fputs("arp smoke inbound reply handling failed\n", stderr);
        return 13;
    }

    auto cached_other = arp.table().lookup(other_ip);
    if (!cached_other || !same_mac(cached_other.value(), other_mac) || arp.table().entry_count() != 2) {
        std::fputs("arp smoke cache update from reply failed\n", stderr);
        return 14;
    }

    auto unresolved = arp.lookup_or_request(retry_ip);
    if (unresolved
        || unresolved.error() != net::errc::again
        || arp.request_count() != 1
        || arp.pending_count() != 1
        || arp.pending_attempts(retry_ip) != 1
        || link.tx_calls != 2) {
        std::fputs("arp smoke lookup_or_request failed\n", stderr);
        return 15;
    }

    auto retry_request_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!retry_request_frame || retry_request_frame.value().type != net::EtherType::arp) {
        std::fputs("arp smoke retry request ether parse failed\n", stderr);
        return 16;
    }

    auto retry_request = net::parse_arp_ipv4_ethernet(retry_request_frame.value().payload);
    if (!retry_request
        || retry_request.value().operation != net::ArpOperation::request
        || !same_ipv4(retry_request.value().target_ip, retry_ip)) {
        std::fputs("arp smoke retry request fields mismatch\n", stderr);
        return 17;
    }

    auto retried = arp.retry_pending_requests();
    if (!retried
        || retried.value() != 1
        || arp.request_count() != 2
        || arp.pending_count() != 1
        || arp.pending_attempts(retry_ip) != 2
        || link.tx_calls != 3) {
        std::fputs("arp smoke retry_pending_requests failed\n", stderr);
        return 18;
    }

    retry_request_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    retry_request = net::parse_arp_ipv4_ethernet(retry_request_frame.value().payload);
    if (!retry_request
        || retry_request.value().operation != net::ArpOperation::request
        || !same_ipv4(retry_request.value().target_ip, retry_ip)) {
        std::fputs("arp smoke retry resend mismatch\n", stderr);
        return 19;
    }

    auto exhausted = arp.retry_pending_requests(2);
    if (!exhausted
        || exhausted.value() != 0
        || arp.request_count() != 2
        || arp.pending_count() != 0
        || arp.failed_count() != 1
        || arp.pending_attempts(retry_ip) != 2
        || link.tx_calls != 3) {
        std::fputs("arp smoke retry exhaustion failed\n", stderr);
        return 20;
    }

    auto timed_out = arp.lookup_or_request(retry_ip);
    if (timed_out || timed_out.error() != net::errc::timeout || link.tx_calls != 3) {
        std::fputs("arp smoke timeout state failed\n", stderr);
        return 21;
    }

    net::PacketBuffer<96> retry_reply{};
    auto wrote_retry_reply = net::write_arp_ipv4_reply_frame(
        retry_reply,
        local_mac,
        retry_mac,
        retry_ip,
        local_mac,
        local_ip);
    if (!wrote_retry_reply) {
        std::fputs("arp smoke retry reply encode failed\n", stderr);
        return 22;
    }

    link.queue_rx(retry_reply.view().payload);
    polled = stack.poll_links();
    if (!polled
        || arp.pending_count() != 0
        || arp.failed_count() != 0
        || link.tx_calls != 3
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("arp smoke retry reply handling failed\n", stderr);
        return 23;
    }

    auto resolved_retry = arp.lookup_or_request(retry_ip);
    if (!resolved_retry || !same_mac(resolved_retry.value(), retry_mac) || arp.request_count() != 2) {
        std::fputs("arp smoke retry cache resolve failed\n", stderr);
        return 24;
    }

    auto auto_pending = arp.lookup_or_request(auto_ip);
    if (auto_pending
        || auto_pending.error() != net::errc::again
        || arp.request_count() != 3
        || arp.pending_count() != 1
        || arp.pending_attempts(auto_ip) != 1
        || link.tx_calls != 4) {
        std::fputs("arp smoke auto request seed failed\n", stderr);
        return 25;
    }

    arp.advance_ticks(1);
    auto idle = arp.service_pending(2, 2);
    if (!idle
        || idle.value().retried != 0
        || idle.value().timed_out != 0
        || arp.request_count() != 3
        || arp.pending_count() != 1
        || link.tx_calls != 4) {
        std::fputs("arp smoke auto idle tick failed\n", stderr);
        return 26;
    }

    arp.advance_ticks(1);
    auto auto_retry = arp.service_pending(2, 2);
    if (!auto_retry
        || auto_retry.value().retried != 1
        || auto_retry.value().timed_out != 0
        || arp.request_count() != 4
        || arp.pending_attempts(auto_ip) != 2
        || link.tx_calls != 5) {
        std::fputs("arp smoke auto retry tick failed\n", stderr);
        return 27;
    }

    arp.advance_ticks(2);
    auto auto_timeout = arp.service_pending(2, 2);
    if (!auto_timeout
        || auto_timeout.value().retried != 0
        || auto_timeout.value().timed_out != 1
        || arp.pending_count() != 0
        || arp.failed_count() != 1
        || link.tx_calls != 5) {
        std::fputs("arp smoke auto timeout tick failed\n", stderr);
        return 28;
    }

    net::PacketBuffer<96> auto_reply{};
    auto wrote_auto_reply = net::write_arp_ipv4_reply_frame(
        auto_reply,
        local_mac,
        auto_mac,
        auto_ip,
        local_mac,
        local_ip);
    if (!wrote_auto_reply) {
        std::fputs("arp smoke auto reply encode failed\n", stderr);
        return 29;
    }

    link.queue_rx(auto_reply.view().payload);
    polled = stack.poll_links();
    if (!polled || arp.failed_count() != 0 || link.tx_calls != 5 || link.rx_pool.in_use_count() != 0) {
        std::fputs("arp smoke auto reply handling failed\n", stderr);
        return 30;
    }

    auto auto_resolved = arp.lookup_or_request(auto_ip);
    if (!auto_resolved || !same_mac(auto_resolved.value(), auto_mac) || arp.request_count() != 4) {
        std::fputs("arp smoke auto cache resolve failed\n", stderr);
        return 31;
    }

    std::puts("net arp smoke: ok");
    return 0;
}
