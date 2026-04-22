#include <array>
#include <cstdio>

import charm.net;
import net.arp;
import net.ether;
import net.ipv4;
import util.core;
import util.expected;

namespace {
    using Link = net::lab::DuplexLink<192>;
    using Pump = net::IcmpStackPump<192, 4, 192, 4, 64>;
    using Node = net::lab::StackNode<Pump>;

    template <util::usize Capacity>
    [[nodiscard]] net::Result<void> write_ipv4_ether_frame(net::PacketBuffer<Capacity>& frame,
                                                           net::MacAddress destination_mac,
                                                           net::MacAddress source_mac,
                                                           const net::Ipv4PacketSpec& spec,
                                                           net::ByteView payload) noexcept {
        net::PacketBuffer<Capacity> ipv4_packet{};
        auto encoded_ipv4 = net::write_ipv4_packet(ipv4_packet, spec, payload);
        if (!encoded_ipv4) {
            return util::unexpected(encoded_ipv4.error());
        }

        auto reset = frame.reset(net::ether_header_size());
        if (!reset) {
            return util::unexpected(reset.error());
        }

        auto appended_ipv4 = frame.append(ipv4_packet.view().payload);
        if (!appended_ipv4) {
            return util::unexpected(appended_ipv4.error());
        }

        return net::prepend_ether_header(
            frame,
            destination_mac,
            source_mac,
            net::EtherType::ipv4);
    }

    template <util::usize N>
    [[nodiscard]] bool bytes_eq(net::ByteView bytes,
                                const util::u8 (&expected)[N]) noexcept {
        if (bytes.size() != N) {
            return false;
        }
        for (util::usize index = 0; index < N; ++index) {
            if (bytes[index] != expected[index]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool same_ipv4(const net::IpAddress& lhs,
                                 const net::IpAddress& rhs) noexcept {
        return net::is_same_ipv4_address(lhs, rhs);
    }

    struct ScriptedTracePeer {
        Link::Endpoint* endpoint{nullptr};
        net::IpAddress final_ip{};
        std::array<net::IpAddress, 2> hops{};
        bool silent_next{false};
        bool unreachable_next{false};
        util::u16 next_identification{0x7000u};
        util::usize arp_requests{0};
        util::usize echo_requests{0};
        util::usize time_exceeded_sent{0};
        util::usize destination_unreachable_sent{0};
        util::usize echo_replies_sent{0};
        util::usize silenced{0};

        void bind(Link::Endpoint& peer_endpoint,
                  net::IpAddress peer_final_ip,
                  net::IpAddress hop1,
                  net::IpAddress hop2) noexcept {
            endpoint = &peer_endpoint;
            final_ip = peer_final_ip;
            hops = {hop1, hop2};
        }

        void silence_once() noexcept {
            silent_next = true;
        }

        void send_destination_unreachable_once() noexcept {
            unreachable_next = true;
        }

        [[nodiscard]] net::Result<void> consume(net::OwnedPacket packet) noexcept {
            const auto frame = net::parse_ether_frame(packet.view());
            if (!frame) {
                return util::unexpected(frame.error());
            }

            switch (frame.value().type) {
                case net::EtherType::arp:
                    return consume_arp(frame.value());
                case net::EtherType::ipv4:
                    return consume_ipv4(frame.value());
                default:
                    return {};
            }
        }

    private:
        [[nodiscard]] net::Result<void> consume_arp(const net::EtherFrameView& frame) noexcept {
            const auto arp = net::parse_arp_ipv4_ethernet(frame.payload);
            if (!arp) {
                return util::unexpected(arp.error());
            }
            if (arp.value().operation != net::ArpOperation::request
                || !same_ipv4(arp.value().target_ip, final_ip)
                || endpoint == nullptr) {
                return {};
            }

            ++arp_requests;

            net::PacketBuffer<192> reply{};
            auto encoded_reply = net::write_arp_ipv4_reply_frame(
                reply,
                arp.value().sender_mac,
                endpoint->mac(),
                final_ip,
                arp.value().sender_mac,
                arp.value().sender_ip);
            if (!encoded_reply) {
                return util::unexpected(encoded_reply.error());
            }

            return endpoint->transmit(reply.view());
        }

        [[nodiscard]] net::Result<void> consume_ipv4(const net::EtherFrameView& frame) noexcept {
            const auto ipv4 = net::parse_ipv4_packet(frame.payload);
            if (!ipv4) {
                return util::unexpected(ipv4.error());
            }
            if (ipv4.value().protocol != net::Ipv4Protocol::icmp) {
                return {};
            }

            const auto echo = net::parse_icmp_echo_packet(ipv4.value().payload);
            if (!echo) {
                return util::unexpected(echo.error());
            }
            if (echo.value().type != net::IcmpType::echo_request || endpoint == nullptr) {
                return {};
            }

            ++echo_requests;

            if (silent_next) {
                silent_next = false;
                ++silenced;
                return {};
            }

            const auto quoted_payload_size = ipv4.value().payload.size() < 8u
                ? ipv4.value().payload.size()
                : 8u;
            const auto quoted_packet = frame.payload.subspan(
                0,
                ipv4.value().header_length + quoted_payload_size);

            if (ipv4.value().ttl <= 1u) {
                return send_time_exceeded(
                    hops[0],
                    frame.source,
                    ipv4.value().source,
                    quoted_packet.payload);
            }
            if (ipv4.value().ttl == 2u) {
                return send_time_exceeded(
                    hops[1],
                    frame.source,
                    ipv4.value().source,
                    quoted_packet.payload);
            }
            if (unreachable_next) {
                unreachable_next = false;
                return send_destination_unreachable(
                    final_ip,
                    frame.source,
                    ipv4.value().source,
                    quoted_packet.payload);
            }

            return send_echo_reply(
                final_ip,
                frame.source,
                ipv4.value().source,
                echo.value());
        }

        [[nodiscard]] net::Result<void> send_time_exceeded(net::IpAddress source_ip,
                                                           net::MacAddress destination_mac,
                                                           net::IpAddress destination_ip,
                                                           net::ByteView quoted_packet) noexcept {
            net::PacketBuffer<192> icmp_packet{};
            auto encoded_icmp = net::write_icmp_time_exceeded_packet(
                icmp_packet,
                0u,
                quoted_packet);
            if (!encoded_icmp) {
                return util::unexpected(encoded_icmp.error());
            }

            net::PacketBuffer<192> frame{};
            auto encoded_frame = write_ipv4_ether_frame(
                frame,
                destination_mac,
                endpoint->mac(),
                net::Ipv4PacketSpec{
                    .identification = next_identification++,
                    .flags_fragment = net::ipv4_do_not_fragment_flag(),
                    .ttl = 64,
                    .protocol = net::Ipv4Protocol::icmp,
                    .source = source_ip,
                    .destination = destination_ip,
                },
                icmp_packet.view().payload);
            if (!encoded_frame) {
                return util::unexpected(encoded_frame.error());
            }

            ++time_exceeded_sent;
            return endpoint->transmit(frame.view());
        }

        [[nodiscard]] net::Result<void> send_destination_unreachable(
            net::IpAddress source_ip,
            net::MacAddress destination_mac,
            net::IpAddress destination_ip,
            net::ByteView quoted_packet) noexcept {
            net::PacketBuffer<192> icmp_packet{};
            auto encoded_icmp = net::write_icmp_destination_unreachable_packet(
                icmp_packet,
                1u,
                quoted_packet);
            if (!encoded_icmp) {
                return util::unexpected(encoded_icmp.error());
            }

            net::PacketBuffer<192> frame{};
            auto encoded_frame = write_ipv4_ether_frame(
                frame,
                destination_mac,
                endpoint->mac(),
                net::Ipv4PacketSpec{
                    .identification = next_identification++,
                    .flags_fragment = net::ipv4_do_not_fragment_flag(),
                    .ttl = 64,
                    .protocol = net::Ipv4Protocol::icmp,
                    .source = source_ip,
                    .destination = destination_ip,
                },
                icmp_packet.view().payload);
            if (!encoded_frame) {
                return util::unexpected(encoded_frame.error());
            }

            ++destination_unreachable_sent;
            return endpoint->transmit(frame.view());
        }

        [[nodiscard]] net::Result<void> send_echo_reply(net::IpAddress source_ip,
                                                        net::MacAddress destination_mac,
                                                        net::IpAddress destination_ip,
                                                        const net::IcmpEchoView& echo) noexcept {
            net::PacketBuffer<192> icmp_packet{};
            auto encoded_icmp = net::write_icmp_echo_reply(
                icmp_packet,
                echo.identifier,
                echo.sequence,
                echo.payload.payload);
            if (!encoded_icmp) {
                return util::unexpected(encoded_icmp.error());
            }

            net::PacketBuffer<192> frame{};
            auto encoded_frame = write_ipv4_ether_frame(
                frame,
                destination_mac,
                endpoint->mac(),
                net::Ipv4PacketSpec{
                    .identification = next_identification++,
                    .flags_fragment = net::ipv4_do_not_fragment_flag(),
                    .ttl = 64,
                    .protocol = net::Ipv4Protocol::icmp,
                    .source = source_ip,
                    .destination = destination_ip,
                },
                icmp_packet.view().payload);
            if (!encoded_frame) {
                return util::unexpected(encoded_frame.error());
            }

            ++echo_replies_sent;
            return endpoint->transmit(frame.view());
        }
    };

    [[nodiscard]] bool service_step(net::icmp::trace::Probe<16>& probe,
                                    Node& client_node,
                                    Link& link) noexcept {
        link.advance(1);

        auto client_progress = client_node.service(1);
        if (!client_progress) {
            std::fprintf(stderr, "trace service client error=%d\n", static_cast<int>(client_progress.error()));
            return false;
        }

        auto peer_progress = link.endpoint_b().poll();
        if (!peer_progress) {
            std::fprintf(stderr, "trace service peer error=%d\n", static_cast<int>(peer_progress.error()));
            return false;
        }

        probe.tick(link.now_ticks());
        return true;
    }

    [[nodiscard]] bool drive_until_ready(net::icmp::trace::Probe<16>& probe,
                                         Node& client_node,
                                         Link& link,
                                         util::usize max_steps = 64) noexcept {
        for (util::usize step = 0; step < max_steps; ++step) {
            if (!service_step(probe, client_node, link)) {
                std::fprintf(
                    stderr,
                    "trace drive step failed: step=%zu state=%u probe_pending=%d egress_pending=%zu link_pending=%zu a_rx=%zu b_rx=%zu\n",
                    static_cast<std::size_t>(step),
                    static_cast<unsigned>(probe.state()),
                    probe.has_pending() ? 1 : 0,
                    static_cast<std::size_t>(client_node.pump().pending_count()),
                    static_cast<std::size_t>(link.pending_count()),
                    static_cast<std::size_t>(link.endpoint_a().rx_pending()),
                    static_cast<std::size_t>(link.endpoint_b().rx_pending()));
                return false;
            }

            if (probe.ready()
                && client_node.pump().pending_count() == 0
                && link.pending_count() == 0
                && link.endpoint_a().rx_pending() == 0) {
                return true;
            }
        }

        std::fprintf(
            stderr,
            "trace drive exhausted: state=%u probe_pending=%d egress_pending=%zu link_pending=%zu a_rx=%zu b_rx=%zu\n",
            static_cast<unsigned>(probe.state()),
            probe.has_pending() ? 1 : 0,
            static_cast<std::size_t>(client_node.pump().pending_count()),
            static_cast<std::size_t>(link.pending_count()),
            static_cast<std::size_t>(link.endpoint_a().rx_pending()),
            static_cast<std::size_t>(link.endpoint_b().rx_pending()));
        return false;
    }
}

int main() {
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto peer_mac = net::MacAddress::from_bytes(0x02u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu);
    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto hop1_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto hop2_ip = net::IpAddress::ipv4(10, 0, 0, 5);
    constexpr auto target_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    static constexpr util::u8 payload[]{'t', 'r', 'a', 'c', 'e'};

    auto fail = [](const char* message, int code) noexcept {
        std::fputs(message, stderr);
        return code;
    };

    Link link{};
    link.set_latency_a_to_b(1);
    link.set_latency_b_to_a(1);
    link.endpoint_b().set_mac(peer_mac);

    Node client_node{};
    auto client_init = client_node.init(
        link.endpoint_a(),
        client_mac,
        client_ip,
        net::IcmpStackPumpConfig{
            .egress = net::IcmpEgressPumpConfig{
                .retry_interval_ticks = 4,
                .max_attempts = 4,
            }
        });
    if (!client_init || !client_node.ready()) {
        return fail("net lab trace smoke client init failed\n", 1);
    }

    ScriptedTracePeer peer{};
    peer.bind(link.endpoint_b(), target_ip, hop1_ip, hop2_ip);
    auto sink_bound = link.endpoint_b().set_input_sink(net::make_owned_packet_sink_ref(peer));
    if (!sink_bound) {
        return fail("net lab trace smoke peer bind failed\n", 2);
    }

    net::icmp::trace::Probe<16> probe{target_ip};
    auto bound_probe = probe.bind(client_node.pump());
    if (!bound_probe
        || !client_node.pump().has_echo_sink()
        || !client_node.pump().has_error_quote_sink()) {
        return fail("net lab trace smoke probe bind failed\n", 3);
    }

    auto hop1_probe = probe.probe(1u, payload, link.now_ticks(), 12);
    if (!hop1_probe
        || hop1_probe.value().disposition != net::IcmpSendDisposition::queued
        || !probe.pending()) {
        return fail("net lab trace smoke ttl1 submit failed\n", 4);
    }

    if (!drive_until_ready(probe, client_node, link)) {
        return fail("net lab trace smoke ttl1 stalled\n", 5);
    }

    const auto hop1_result = probe.result();
    if (!hop1_result.ready()
        || !hop1_result.hop()
        || !hop1_result.ok()
        || hop1_result.has_value()
        || hop1_result.response_type != net::IcmpType::time_exceeded
        || hop1_result.response_code != 0u
        || hop1_result.ttl != 1u
        || !same_ipv4(hop1_result.responder, hop1_ip)
        || hop1_result.identifier() != hop1_probe.value().info.identifier
        || hop1_result.sequence() != hop1_probe.value().info.sequence
        || probe.request_count() != 1
        || probe.response_count() != 1
        || probe.hop_count() != 1
        || probe.reach_count() != 0
        || probe.unreachable_count() != 0
        || probe.timeout_count() != 0
        || probe.drop_count() != 0
        || probe.error_count() != 0
        || probe.queued_count() != 1
        || probe.transmitted_count() != 0
        || peer.arp_requests != 1
        || peer.echo_requests != 1
        || peer.time_exceeded_sent != 1
        || peer.destination_unreachable_sent != 0
        || peer.echo_replies_sent != 0
        || peer.silenced != 0
        || !link.idle()) {
        return fail("net lab trace smoke ttl1 mismatch\n", 6);
    }

    auto hop2_probe = probe.probe(2u, payload, link.now_ticks(), 12);
    if (!hop2_probe
        || hop2_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab trace smoke ttl2 submit failed\n", 7);
    }

    if (!drive_until_ready(probe, client_node, link)) {
        return fail("net lab trace smoke ttl2 stalled\n", 8);
    }

    const auto hop2_result = probe.result();
    if (!hop2_result.ready()
        || !hop2_result.hop()
        || !hop2_result.ok()
        || hop2_result.has_value()
        || hop2_result.response_type != net::IcmpType::time_exceeded
        || hop2_result.response_code != 0u
        || hop2_result.ttl != 2u
        || !same_ipv4(hop2_result.responder, hop2_ip)
        || hop2_result.identifier() != hop2_probe.value().info.identifier
        || hop2_result.sequence() != hop2_probe.value().info.sequence
        || probe.request_count() != 2
        || probe.response_count() != 2
        || probe.hop_count() != 2
        || probe.reach_count() != 0
        || probe.unreachable_count() != 0
        || probe.timeout_count() != 0
        || probe.drop_count() != 0
        || probe.error_count() != 0
        || probe.queued_count() != 1
        || probe.transmitted_count() != 1
        || peer.arp_requests != 1
        || peer.echo_requests != 2
        || peer.time_exceeded_sent != 2
        || peer.destination_unreachable_sent != 0
        || peer.echo_replies_sent != 0
        || !link.idle()) {
        return fail("net lab trace smoke ttl2 mismatch\n", 9);
    }

    peer.send_destination_unreachable_once();
    auto unreachable_probe = probe.probe(3u, payload, link.now_ticks(), 12);
    if (!unreachable_probe
        || unreachable_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab trace smoke unreachable submit failed\n", 10);
    }

    if (!drive_until_ready(probe, client_node, link)) {
        return fail("net lab trace smoke unreachable stalled\n", 11);
    }

    const auto unreachable_result = probe.result();
    if (!unreachable_result.ready()
        || !unreachable_result.unreachable()
        || !unreachable_result.ok()
        || unreachable_result.has_value()
        || unreachable_result.response_type != net::IcmpType::destination_unreachable
        || unreachable_result.response_code != 1u
        || unreachable_result.ttl != 3u
        || !same_ipv4(unreachable_result.responder, target_ip)
        || unreachable_result.identifier() != unreachable_probe.value().info.identifier
        || unreachable_result.sequence() != unreachable_probe.value().info.sequence
        || probe.request_count() != 3
        || probe.response_count() != 3
        || probe.hop_count() != 2
        || probe.reach_count() != 0
        || probe.unreachable_count() != 1
        || probe.timeout_count() != 0
        || probe.drop_count() != 0
        || probe.error_count() != 0
        || probe.queued_count() != 1
        || probe.transmitted_count() != 2
        || peer.echo_requests != 3
        || peer.time_exceeded_sent != 2
        || peer.destination_unreachable_sent != 1
        || peer.echo_replies_sent != 0
        || !link.idle()) {
        return fail("net lab trace smoke unreachable mismatch\n", 12);
    }

    auto reach_probe = probe.probe(3u, payload, link.now_ticks(), 12);
    if (!reach_probe
        || reach_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab trace smoke reach submit failed\n", 13);
    }

    if (!drive_until_ready(probe, client_node, link)) {
        return fail("net lab trace smoke reach stalled\n", 14);
    }

    const auto reach_result = probe.result();
    if (!reach_result.ready()
        || !reach_result.reached()
        || !reach_result.ok()
        || !reach_result.has_value()
        || reach_result.response_type != net::IcmpType::echo_reply
        || reach_result.response_code != 0u
        || reach_result.ttl != 3u
        || !same_ipv4(reach_result.responder, target_ip)
        || !bytes_eq(reach_result.value_payload(), payload)
        || reach_result.identifier() != reach_probe.value().info.identifier
        || reach_result.sequence() != reach_probe.value().info.sequence
        || probe.request_count() != 4
        || probe.response_count() != 4
        || probe.hop_count() != 2
        || probe.reach_count() != 1
        || probe.unreachable_count() != 1
        || probe.timeout_count() != 0
        || probe.drop_count() != 0
        || probe.error_count() != 0
        || probe.queued_count() != 1
        || probe.transmitted_count() != 3
        || peer.echo_requests != 4
        || peer.time_exceeded_sent != 2
        || peer.destination_unreachable_sent != 1
        || peer.echo_replies_sent != 1
        || !link.idle()) {
        return fail("net lab trace smoke reach mismatch\n", 15);
    }

    peer.silence_once();
    auto timeout_probe = probe.probe(3u, payload, link.now_ticks(), 6);
    if (!timeout_probe
        || timeout_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab trace smoke timeout submit failed\n", 16);
    }

    if (!drive_until_ready(probe, client_node, link, 24)) {
        return fail("net lab trace smoke timeout stalled\n", 17);
    }

    const auto timeout_result = probe.result();
    if (!timeout_result.ready()
        || !timeout_result.timed_out()
        || timeout_result.ok()
        || timeout_result.has_value()
        || timeout_result.ttl != 3u
        || timeout_result.identifier() != timeout_probe.value().info.identifier
        || timeout_result.sequence() != timeout_probe.value().info.sequence
        || probe.request_count() != 5
        || probe.response_count() != 4
        || probe.hop_count() != 2
        || probe.reach_count() != 1
        || probe.unreachable_count() != 1
        || probe.timeout_count() != 1
        || probe.drop_count() != 0
        || probe.error_count() != 0
        || probe.queued_count() != 1
        || probe.transmitted_count() != 4
        || peer.echo_requests != 5
        || peer.time_exceeded_sent != 2
        || peer.destination_unreachable_sent != 1
        || peer.echo_replies_sent != 1
        || peer.silenced != 1
        || !link.idle()) {
        return fail("net lab trace smoke timeout mismatch\n", 18);
    }

    auto recovery_probe = probe.probe(3u, payload, link.now_ticks(), 12);
    if (!recovery_probe
        || recovery_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab trace smoke recovery submit failed\n", 19);
    }

    if (!drive_until_ready(probe, client_node, link)) {
        return fail("net lab trace smoke recovery stalled\n", 20);
    }

    const auto recovery_result = probe.result();
    if (!recovery_result.ready()
        || !recovery_result.reached()
        || !recovery_result.ok()
        || !recovery_result.has_value()
        || recovery_result.response_type != net::IcmpType::echo_reply
        || recovery_result.response_code != 0u
        || recovery_result.ttl != 3u
        || !same_ipv4(recovery_result.responder, target_ip)
        || !bytes_eq(recovery_result.value_payload(), payload)
        || recovery_result.identifier() != recovery_probe.value().info.identifier
        || recovery_result.sequence() != recovery_probe.value().info.sequence
        || probe.request_count() != 6
        || probe.response_count() != 5
        || probe.hop_count() != 2
        || probe.reach_count() != 2
        || probe.unreachable_count() != 1
        || probe.timeout_count() != 1
        || probe.drop_count() != 0
        || probe.error_count() != 0
        || probe.queued_count() != 1
        || probe.transmitted_count() != 5
        || peer.echo_requests != 6
        || peer.time_exceeded_sent != 2
        || peer.destination_unreachable_sent != 1
        || peer.echo_replies_sent != 2
        || peer.silenced != 1
        || client_node.pump().pending_count() != 0
        || !link.idle()) {
        return fail("net lab trace smoke recovery mismatch\n", 21);
    }

    std::puts("net lab trace smoke: ok");
    return 0;
}
