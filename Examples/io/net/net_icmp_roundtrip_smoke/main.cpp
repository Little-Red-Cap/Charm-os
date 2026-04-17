#include <array>
#include <cstdio>

import charm.net;
import net.driver;
import net.ether;
import net.packet;
import util.core;
import util.expected;

namespace {
    template <util::usize Capacity, util::usize QueueDepth = 4>
    struct StubLinkDriver {
        net::OwnedPacketSinkRef input_sink{};
        net::PacketPool<QueueDepth, Capacity> rx_pool{};
        std::array<std::array<util::u8, Capacity>, QueueDepth> rx_frames{};
        std::array<util::usize, QueueDepth> rx_sizes{};
        util::usize rx_head{0};
        util::usize rx_tail{0};
        util::usize rx_count{0};

        std::array<util::u8, Capacity> tx_bytes{};
        util::usize tx_size{0};
        util::usize tx_calls{0};

        net::MacAddress mac{};
        StubLinkDriver* peer{nullptr};

        [[nodiscard]] net::NetDriverInfo info() const noexcept {
            return net::NetDriverInfo{
                .mtu = Capacity,
                .mac = mac,
                .capabilities = net::NetIfCapability::rx
                    | net::NetIfCapability::tx
                    | net::NetIfCapability::broadcast
            };
        }

        [[nodiscard]] net::Result<void> set_input_sink(net::OwnedPacketSinkRef sink) noexcept {
            input_sink = sink;
            return {};
        }

        [[nodiscard]] bool has_rx() const noexcept {
            return rx_count != 0;
        }

        [[nodiscard]] net::Result<void> enqueue_rx(net::PacketView packet) noexcept {
            if (rx_count == QueueDepth || packet.size() > Capacity) {
                return util::unexpected(net::errc::buffer_overflow);
            }

            rx_sizes[rx_tail] = packet.size();
            for (util::usize i = 0; i < packet.size(); ++i) {
                rx_frames[rx_tail][i] = packet[i];
            }
            rx_tail = (rx_tail + 1) % QueueDepth;
            ++rx_count;
            return {};
        }

        [[nodiscard]] net::Result<void> poll() noexcept {
            if (rx_count == 0) {
                return {};
            }
            if (!input_sink.valid()) {
                return util::unexpected(net::errc::bad_state);
            }

            auto lease = rx_pool.acquire();
            if (!lease) {
                return util::unexpected(lease.error());
            }

            auto appended = lease.value()->append(net::ByteView{
                rx_frames[rx_head].data(),
                rx_sizes[rx_head]
            });
            if (!appended) {
                return util::unexpected(appended.error());
            }

            rx_head = (rx_head + 1) % QueueDepth;
            --rx_count;

            return input_sink.consume(net::OwnedPacket{
                static_cast<typename net::PacketPool<QueueDepth, Capacity>::Lease&&>(lease.value())
            });
        }

        [[nodiscard]] net::Result<void> transmit(net::PacketView packet) noexcept {
            if (packet.size() > Capacity) {
                return util::unexpected(net::errc::buffer_overflow);
            }

            tx_size = packet.size();
            for (util::usize i = 0; i < packet.size(); ++i) {
                tx_bytes[i] = packet[i];
            }
            ++tx_calls;

            if (!peer) {
                return {};
            }
            return peer->enqueue_rx(packet);
        }
    };

    template <util::usize Capacity = 192>
    struct Node {
        StubLinkDriver<Capacity> link{};
        net::NetIf netif{};
        net::NetDriver driver{};
        net::Stack stack{};
        net::IcmpStackPump<Capacity, 4, Capacity, 4, 64> pump{};

        [[nodiscard]] net::Result<void> init(net::MacAddress mac, net::IpAddress address) noexcept {
            link.mac = mac;

            auto configured = netif.configure(net::NetIfConfig{
                .mtu = Capacity,
                .mac = mac,
                .address = address,
                .capabilities = net::NetIfCapability::rx
                    | net::NetIfCapability::tx
                    | net::NetIfCapability::broadcast
            });
            if (!configured) {
                return util::unexpected(configured.error());
            }

            auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
            if (!attached) {
                return util::unexpected(attached.error());
            }

            auto registered = stack.register_driver(driver);
            if (!registered) {
                return util::unexpected(registered.error());
            }

            auto up = netif.bring_up();
            if (!up) {
                return util::unexpected(up.error());
            }

            pump.bind(stack, netif);
            pump.configure(net::IcmpStackPumpConfig{
                .egress = net::IcmpEgressPumpConfig{
                    .retry_interval_ticks = 2,
                    .max_attempts = 4,
                }
            });
            return {};
        }
    };

    struct EchoReplyProbe {
        std::array<util::u8, 64> bytes{};
        util::usize size{0};
        util::usize calls{0};
        bool failed{false};
        net::IcmpEchoInfo info{};

        [[nodiscard]] net::Result<void> consume(const net::IcmpEchoInfo& echo,
                                                net::OwnedPacket packet) noexcept {
            if (echo.type != net::IcmpType::echo_reply) {
                failed = true;
                return util::unexpected(net::errc::invalid_arg);
            }

            const auto view = packet.view();
            if (view.size() > bytes.size()) {
                failed = true;
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

    template <class Pump>
    struct EchoResponder {
        Pump* pump{nullptr};
        util::usize calls{0};
        util::usize transmitted{0};
        util::usize queued{0};
        bool failed{false};
        net::errc last_error{net::errc::ok};
        net::IcmpEchoInfo last_info{};
        std::array<util::u8, 64> last_payload{};
        util::usize last_payload_size{0};

        [[nodiscard]] net::Result<void> consume(const net::IcmpEchoInfo& echo,
                                                net::OwnedPacket packet) noexcept {
            if (pump == nullptr || echo.type != net::IcmpType::echo_request) {
                failed = true;
                last_error = net::errc::bad_state;
                return util::unexpected(last_error);
            }

            const auto view = packet.view();
            if (view.size() > last_payload.size()) {
                failed = true;
                last_error = net::errc::buffer_overflow;
                return util::unexpected(last_error);
            }
            for (util::usize i = 0; i < view.size(); ++i) {
                last_payload[i] = view[i];
            }
            last_payload_size = view.size();
            last_info = echo;

            auto sent = pump->send_reply(
                echo.local,
                echo.peer,
                echo.identifier,
                echo.sequence,
                view.payload,
                47,
                0x4321u,
                0x22u);
            if (!sent) {
                failed = true;
                last_error = sent.error();
                return util::unexpected(sent.error());
            }

            ++calls;
            if (sent.value() == net::IcmpSendDisposition::transmitted) {
                ++transmitted;
            } else {
                ++queued;
            }
            return {};
        }
    };

    template <typename ClientNode, typename ServerNode>
    [[nodiscard]] bool drive_until_idle(ClientNode& client_node,
                                        ServerNode& server_node,
                                        util::usize max_steps = 16) noexcept {
        for (util::usize step = 0; step < max_steps; ++step) {
            auto client_progress = client_node.pump.service(1);
            if (!client_progress) {
                return false;
            }

            auto server_progress = server_node.pump.service(1);
            if (!server_progress) {
                return false;
            }

            if (client_node.pump.pending_count() == 0
                && server_node.pump.pending_count() == 0
                && !client_node.link.has_rx()
                && !server_node.link.has_rx()) {
                return true;
            }
        }
        return false;
    }

    template <util::usize N>
    [[nodiscard]] bool bytes_eq(const util::u8 (&lhs)[N],
                                net::ByteView rhs) noexcept {
        if (rhs.size() != N) {
            return false;
        }
        for (util::usize i = 0; i < N; ++i) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        return true;
    }

    template <util::usize N>
    [[nodiscard]] bool bytes_eq(const util::u8 (&lhs)[N],
                                const std::array<util::u8, 64>& rhs,
                                util::usize rhs_size) noexcept {
        if (rhs_size != N) {
            return false;
        }
        for (util::usize i = 0; i < N; ++i) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool same_mac(const net::MacAddress& lhs, const net::MacAddress& rhs) noexcept {
        for (util::usize i = 0; i < lhs.bytes.size(); ++i) {
            if (lhs.bytes[i] != rhs.bytes[i]) {
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
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    static constexpr util::u8 payload[]{'p', 'i', 'n', 'g'};

    auto fail = [](const char* message, int code) noexcept {
        std::fputs(message, stderr);
        return code;
    };

    Node<> client_node{};
    auto client_init = client_node.init(client_mac, client_ip);
    if (!client_init) {
        return fail("icmp roundtrip smoke client init failed\n", 1);
    }

    Node<> server_node{};
    auto server_init = server_node.init(server_mac, server_ip);
    if (!server_init) {
        return fail("icmp roundtrip smoke server init failed\n", 2);
    }

    client_node.link.peer = &server_node.link;
    server_node.link.peer = &client_node.link;

    EchoReplyProbe client_probe{};
    EchoResponder<decltype(server_node.pump)> server_responder{
        .pump = &server_node.pump
    };
    client_node.pump.set_echo_sink(client_probe);
    server_node.pump.set_echo_sink(server_responder);
    if (!client_node.pump.has_echo_sink() || !server_node.pump.has_echo_sink()) {
        return fail("icmp roundtrip smoke sink bind failed\n", 3);
    }

    auto seeded = client_node.pump.send_request(
        net::IpAddress::ipv4_any(),
        server_ip,
        0x1357u,
        0x0009u,
        net::ByteView{payload, sizeof(payload)},
        29,
        0x2468u,
        0x11u);
    if (!seeded
        || seeded.value() != net::IcmpSendDisposition::queued
        || client_node.link.tx_calls != 1
        || client_node.pump.arp().request_count() != 1
        || client_node.pump.arp().pending_count() != 1
        || client_node.pump.pending_count() != 1
        || server_node.link.tx_calls != 0) {
        return fail("icmp roundtrip smoke initial queue failed\n", 4);
    }

    if (!drive_until_idle(client_node, server_node)) {
        return fail("icmp roundtrip smoke exchange stalled\n", 5);
    }

    auto client_peer_mac = client_node.pump.arp().table().lookup(server_ip);
    auto server_peer_mac = server_node.pump.arp().table().lookup(client_ip);
    if (!client_peer_mac
        || !server_peer_mac
        || !same_mac(client_peer_mac.value(), server_mac)
        || !same_mac(server_peer_mac.value(), client_mac)) {
        return fail("icmp roundtrip smoke arp cache mismatch\n", 6);
    }

    if (client_probe.failed
        || client_probe.calls != 1
        || client_probe.info.type != net::IcmpType::echo_reply
        || !same_ipv4(client_probe.info.local, client_ip)
        || !same_ipv4(client_probe.info.peer, server_ip)
        || client_probe.info.identifier != 0x1357u
        || client_probe.info.sequence != 0x0009u
        || !bytes_eq(payload, client_probe.bytes, client_probe.size)) {
        return fail("icmp roundtrip smoke client reply mismatch\n", 7);
    }

    if (server_responder.failed
        || server_responder.calls != 1
        || server_responder.transmitted != 1
        || server_responder.queued != 0
        || server_responder.last_error != net::errc::ok
        || server_responder.last_info.type != net::IcmpType::echo_request
        || !same_ipv4(server_responder.last_info.local, server_ip)
        || !same_ipv4(server_responder.last_info.peer, client_ip)
        || server_responder.last_info.identifier != 0x1357u
        || server_responder.last_info.sequence != 0x0009u
        || !bytes_eq(payload, server_responder.last_payload, server_responder.last_payload_size)) {
        return fail("icmp roundtrip smoke server responder mismatch\n", 8);
    }

    if (client_node.pump.pending_count() != 0
        || server_node.pump.pending_count() != 0
        || client_node.pump.dropped_count() != 0
        || server_node.pump.dropped_count() != 0
        || client_node.link.has_rx()
        || server_node.link.has_rx()) {
        return fail("icmp roundtrip smoke left pending work\n", 9);
    }

    if (client_node.pump.queued_count() != 1
        || client_node.pump.flushed_count() != 1
        || client_node.pump.icmp().packet_count() != 1
        || client_node.pump.icmp().request_count() != 0
        || client_node.pump.icmp().reply_count() != 1
        || client_node.pump.icmp().drop_count() != 0
        || server_node.pump.icmp().packet_count() != 1
        || server_node.pump.icmp().request_count() != 1
        || server_node.pump.icmp().reply_count() != 0
        || server_node.pump.icmp().drop_count() != 0) {
        return fail("icmp roundtrip smoke service counters mismatch\n", 10);
    }

    if (client_node.link.tx_calls != 2 || server_node.link.tx_calls != 2) {
        return fail("icmp roundtrip smoke tx count mismatch\n", 11);
    }

    auto request_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{client_node.link.tx_bytes.data(), client_node.link.tx_size},
        0,
        0
    });
    if (!request_frame
        || request_frame.value().type != net::EtherType::ipv4
        || !same_mac(request_frame.value().destination, server_mac)
        || !same_mac(request_frame.value().source, client_mac)) {
        return fail("icmp roundtrip smoke request ether parse failed\n", 12);
    }

    auto request_ipv4 = net::parse_ipv4_packet(request_frame.value().payload);
    if (!request_ipv4
        || request_ipv4.value().protocol != net::Ipv4Protocol::icmp
        || request_ipv4.value().ttl != 29
        || request_ipv4.value().identification != 0x2468u
        || request_ipv4.value().dscp_ecn != 0x11u
        || !same_ipv4(request_ipv4.value().source, client_ip)
        || !same_ipv4(request_ipv4.value().destination, server_ip)) {
        return fail("icmp roundtrip smoke request ipv4 mismatch\n", 13);
    }

    auto request_icmp = net::parse_icmp_echo_packet(request_ipv4.value().payload);
    if (!request_icmp
        || request_icmp.value().type != net::IcmpType::echo_request
        || request_icmp.value().identifier != 0x1357u
        || request_icmp.value().sequence != 0x0009u
        || !bytes_eq(payload, request_icmp.value().payload.payload)) {
        return fail("icmp roundtrip smoke request icmp mismatch\n", 14);
    }

    auto reply_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{server_node.link.tx_bytes.data(), server_node.link.tx_size},
        0,
        0
    });
    if (!reply_frame
        || reply_frame.value().type != net::EtherType::ipv4
        || !same_mac(reply_frame.value().destination, client_mac)
        || !same_mac(reply_frame.value().source, server_mac)) {
        return fail("icmp roundtrip smoke reply ether parse failed\n", 15);
    }

    auto reply_ipv4 = net::parse_ipv4_packet(reply_frame.value().payload);
    if (!reply_ipv4
        || reply_ipv4.value().protocol != net::Ipv4Protocol::icmp
        || reply_ipv4.value().ttl != 47
        || reply_ipv4.value().identification != 0x4321u
        || reply_ipv4.value().dscp_ecn != 0x22u
        || !same_ipv4(reply_ipv4.value().source, server_ip)
        || !same_ipv4(reply_ipv4.value().destination, client_ip)) {
        return fail("icmp roundtrip smoke reply ipv4 mismatch\n", 16);
    }

    auto reply_icmp = net::parse_icmp_echo_packet(reply_ipv4.value().payload);
    if (!reply_icmp
        || reply_icmp.value().type != net::IcmpType::echo_reply
        || reply_icmp.value().identifier != 0x1357u
        || reply_icmp.value().sequence != 0x0009u
        || !bytes_eq(payload, reply_icmp.value().payload.payload)) {
        return fail("icmp roundtrip smoke reply icmp mismatch\n", 17);
    }

    std::puts("net icmp roundtrip smoke: ok");
    return 0;
}
