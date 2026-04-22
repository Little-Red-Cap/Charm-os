#include <cstdio>

import charm.net;
import util.core;

namespace {
    using Link = net::lab::DuplexLink<192>;
    using HostPump = net::IcmpStackPump<192, 4, 192, 4, 64>;
    using HostNode = net::lab::StackNode<HostPump>;
    using ForwardHop = net::Ipv4ForwardingHop<192, 4, 192, 4>;

    struct PhaseCounters {
        util::usize router1_forwarded{0};
        util::usize router1_unreachable{0};
        util::usize router2_forwarded{0};
        util::usize router2_unreachable{0};
        util::usize server_requests{0};
        util::usize server_replies{0};
        util::usize server_drops{0};
    };

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
        if (!lhs.is_ipv4() || !rhs.is_ipv4()) {
            return false;
        }
        for (util::usize index = 0; index < 4; ++index) {
            if (lhs.bytes[index] != rhs.bytes[index]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool same_mac(const net::MacAddress& lhs,
                                const net::MacAddress& rhs) noexcept {
        for (util::usize index = 0; index < lhs.bytes.size(); ++index) {
            if (lhs.bytes[index] != rhs.bytes[index]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool same_route(const net::Ipv4ForwardingRoute& route,
                                  const net::IpAddress& network,
                                  util::u8 prefix_length,
                                  net::Ipv4ForwardingPort egress_port,
                                  bool has_next_hop,
                                  const net::IpAddress& next_hop,
                                  util::u16 metric) noexcept {
        return same_ipv4(route.network, network)
            && route.prefix_length == prefix_length
            && route.egress_port == egress_port
            && route.has_next_hop == has_next_hop
            && same_ipv4(route.next_hop, next_hop)
            && route.metric == metric;
    }

    [[nodiscard]] bool same_decision(const net::Ipv4ForwardingDecisionSnapshot& decision,
                                     const net::IpAddress& network,
                                     util::u8 prefix_length,
                                     net::Ipv4ForwardingPort egress_port,
                                     bool has_next_hop,
                                     const net::IpAddress& next_hop,
                                     util::u16 metric,
                                     bool from_connected_prefix) noexcept {
        return same_ipv4(decision.network, network)
            && decision.prefix_length == prefix_length
            && decision.egress_port == egress_port
            && decision.has_next_hop == has_next_hop
            && same_ipv4(decision.next_hop, next_hop)
            && decision.metric == metric
            && decision.from_connected_prefix == from_connected_prefix;
    }

    [[nodiscard]] PhaseCounters capture_phase(const ForwardHop& router1,
                                              const ForwardHop& router2,
                                              const net::icmp::echo::AutoReplyServer& server) noexcept {
        return PhaseCounters{
            .router1_forwarded = router1.forwarded_count(),
            .router1_unreachable = router1.destination_unreachable_count(),
            .router2_forwarded = router2.forwarded_count(),
            .router2_unreachable = router2.destination_unreachable_count(),
            .server_requests = server.request_count(),
            .server_replies = server.reply_count(),
            .server_drops = server.drop_count(),
        };
    }

    [[nodiscard]] bool phase_delta_matches(const PhaseCounters& before,
                                           const PhaseCounters& after,
                                           util::usize router1_forwarded,
                                           util::usize router1_unreachable,
                                           util::usize router2_forwarded,
                                           util::usize router2_unreachable,
                                           util::usize server_requests,
                                           util::usize server_replies,
                                           util::usize server_drops = 0u) noexcept {
        return after.router1_forwarded - before.router1_forwarded == router1_forwarded
            && after.router1_unreachable - before.router1_unreachable == router1_unreachable
            && after.router2_forwarded - before.router2_forwarded == router2_forwarded
            && after.router2_unreachable - before.router2_unreachable == router2_unreachable
            && after.server_requests - before.server_requests == server_requests
            && after.server_replies - before.server_replies == server_replies
            && after.server_drops - before.server_drops == server_drops;
    }

    [[nodiscard]] bool system_idle(const net::icmp::echo::Probe<16>& probe,
                                   const HostNode& client_node,
                                   const ForwardHop& router1,
                                   const ForwardHop& router2,
                                   const HostNode& server_node,
                                   const Link& client_link,
                                   const Link& middle_link,
                                   const Link& server_link) noexcept {
        return probe.ready()
            && client_node.pump().pending_count() == 0u
            && router1.pending_count() == 0u
            && router2.pending_count() == 0u
            && server_node.pump().pending_count() == 0u
            && client_link.idle()
            && middle_link.idle()
            && server_link.idle();
    }

    [[nodiscard]] bool service_step(net::icmp::echo::Probe<16>& probe,
                                    HostNode& client_node,
                                    ForwardHop& router1,
                                    ForwardHop& router2,
                                    HostNode& server_node,
                                    Link& client_link,
                                    Link& middle_link,
                                    Link& server_link) noexcept {
        client_link.advance(1);
        middle_link.advance(1);
        server_link.advance(1);

        auto client_progress = client_node.service(1);
        if (!client_progress) {
            std::fprintf(stderr,
                         "route default precedence client service error=%d\n",
                         static_cast<int>(client_progress.error()));
            return false;
        }

        auto router1_progress = router1.service(1);
        if (!router1_progress) {
            std::fprintf(stderr,
                         "route default precedence router1 service error=%d\n",
                         static_cast<int>(router1_progress.error()));
            return false;
        }

        auto router2_progress = router2.service(1);
        if (!router2_progress) {
            std::fprintf(stderr,
                         "route default precedence router2 service error=%d\n",
                         static_cast<int>(router2_progress.error()));
            return false;
        }

        auto server_progress = server_node.service(1);
        if (!server_progress) {
            std::fprintf(stderr,
                         "route default precedence server service error=%d\n",
                         static_cast<int>(server_progress.error()));
            return false;
        }

        probe.tick(client_link.now_ticks());
        return true;
    }

    [[nodiscard]] bool drive_until_ready(net::icmp::echo::Probe<16>& probe,
                                         HostNode& client_node,
                                         ForwardHop& router1,
                                         ForwardHop& router2,
                                         HostNode& server_node,
                                         Link& client_link,
                                         Link& middle_link,
                                         Link& server_link,
                                         util::usize max_steps = 96) noexcept {
        for (util::usize step = 0; step < max_steps; ++step) {
            if (!service_step(
                    probe,
                    client_node,
                    router1,
                    router2,
                    server_node,
                    client_link,
                    middle_link,
                    server_link)) {
                return false;
            }

            if (system_idle(
                    probe,
                    client_node,
                    router1,
                    router2,
                    server_node,
                    client_link,
                    middle_link,
                    server_link)) {
                return true;
            }
        }
        return false;
    }
}

int main() {
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x35u, 0x00u, 0x00u, 0x00u, 0x02u);
    constexpr auto router1_a_mac = net::MacAddress::from_bytes(0x02u, 0x35u, 0x00u, 0x00u, 0x00u, 0x11u);
    constexpr auto router1_b_mac = net::MacAddress::from_bytes(0x02u, 0x35u, 0x00u, 0x00u, 0x00u, 0x12u);
    constexpr auto router2_a_mac = net::MacAddress::from_bytes(0x02u, 0x35u, 0x00u, 0x00u, 0x00u, 0x21u);
    constexpr auto router2_b_mac = net::MacAddress::from_bytes(0x02u, 0x35u, 0x00u, 0x00u, 0x00u, 0x22u);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0x35u, 0x00u, 0x00u, 0x00u, 0x09u);

    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto router1_a_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto router1_b_ip = net::IpAddress::ipv4(10, 0, 1, 1);
    constexpr auto router2_a_ip = net::IpAddress::ipv4(10, 0, 1, 2);
    constexpr auto router2_b_ip = net::IpAddress::ipv4(10, 0, 2, 1);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 2, 9);
    constexpr auto default_network = net::IpAddress::ipv4_any();
    constexpr auto specific_network = net::IpAddress::ipv4(10, 0, 2, 0);

    static constexpr util::u8 payload[]{'d', 'e', 'f', 'a', 'u', 'l', 't'};

    auto fail = [](const char* message, int code) noexcept {
        std::fputs(message, stderr);
        return code;
    };

    Link client_link{};
    Link middle_link{};
    Link server_link{};
    client_link.set_latency_a_to_b(1);
    client_link.set_latency_b_to_a(1);
    middle_link.set_latency_a_to_b(1);
    middle_link.set_latency_b_to_a(1);
    server_link.set_latency_a_to_b(1);
    server_link.set_latency_b_to_a(1);

    HostNode client_node{};
    auto client_init = client_node.init(
        client_link.endpoint_a(),
        client_mac,
        client_ip,
        net::IcmpStackPumpConfig{
            .egress = net::IcmpEgressPumpConfig{
                .retry_interval_ticks = 4,
                .max_attempts = 4,
            }
        });
    if (!client_init || !client_node.ready()) {
        return fail("net lab route default precedence smoke client init failed\n", 1);
    }

    ForwardHop router1{};
    router1.configure(net::Ipv4ForwardingHopConfig{
        .retry_interval_ticks = 4,
        .max_attempts = 4,
        .icmp_ttl = 64,
    });
    auto router1_a_init = router1.init_port_a(client_link.endpoint_b(), router1_a_mac, router1_a_ip);
    auto router1_b_init = router1.init_port_b(middle_link.endpoint_a(), router1_b_mac, router1_b_ip);
    auto router1_prefix_a = router1.set_port_a_prefix_length(24u);
    auto router1_prefix_b = router1.set_port_b_prefix_length(24u);
    auto router1_default_route = router1.set_gateway_route(
        default_network,
        0u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    if (!router1_a_init
        || !router1_b_init
        || !router1_prefix_a
        || !router1_prefix_b
        || !router1_default_route
        || router1.route_count() != 1u
        || !router1.ready()) {
        return fail("net lab route default precedence smoke router1 init failed\n", 2);
    }

    ForwardHop router2{};
    router2.configure(net::Ipv4ForwardingHopConfig{
        .retry_interval_ticks = 4,
        .max_attempts = 4,
        .icmp_ttl = 64,
    });
    auto router2_a_init = router2.init_port_a(middle_link.endpoint_b(), router2_a_mac, router2_a_ip);
    auto router2_b_init = router2.init_port_b(server_link.endpoint_a(), router2_b_mac, router2_b_ip);
    auto router2_prefix_a = router2.set_port_a_prefix_length(24u);
    auto router2_prefix_b = router2.set_port_b_prefix_length(24u);
    auto router2_return_route = router2.set_gateway_route(
        net::IpAddress::ipv4(10, 0, 0, 0),
        24u,
        net::Ipv4ForwardingPort::a,
        router1_b_ip);
    if (!router2_a_init
        || !router2_b_init
        || !router2_prefix_a
        || !router2_prefix_b
        || !router2_return_route
        || router2.route_count() != 1u
        || !router2.ready()) {
        return fail("net lab route default precedence smoke router2 init failed\n", 3);
    }

    HostNode server_node{};
    auto server_init = server_node.init(
        server_link.endpoint_b(),
        server_mac,
        server_ip,
        net::IcmpStackPumpConfig{
            .egress = net::IcmpEgressPumpConfig{
                .retry_interval_ticks = 4,
                .max_attempts = 4,
            }
        });
    if (!server_init || !server_node.ready()) {
        return fail("net lab route default precedence smoke server init failed\n", 4);
    }

    net::icmp::echo::Probe<16> probe{server_ip};
    net::icmp::echo::AutoReplyServer server{};
    auto bound_probe = probe.bind(client_node.pump());
    auto bound_server = server.bind(server_node.pump());
    if (!bound_probe
        || !bound_server
        || !client_node.pump().has_echo_sink()
        || !server_node.pump().has_echo_sink()) {
        return fail("net lab route default precedence smoke protocol bind failed\n", 5);
    }

    net::Ipv4ForwardingDecisionSnapshot decision{};
    const auto default_route = router1.route_at(0u);
    if (!default_route.has_value()
        || !same_route(
            default_route.value(),
            default_network,
            0u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            0u)
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            default_network,
            0u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            0u,
            false)) {
        return fail("net lab route default precedence smoke default decision mismatch\n", 6);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto default_before = capture_phase(router1, router2, server);
    auto default_ping = probe.ping(payload, client_link.now_ticks(), 20);
    if (!default_ping
        || default_ping.value().disposition != net::IcmpSendDisposition::queued
        || !probe.pending()) {
        return fail("net lab route default precedence smoke default submit failed\n", 7);
    }

    if (!drive_until_ready(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route default precedence smoke default stalled\n", 8);
    }

    const auto default_result = probe.result();
    const auto default_after = capture_phase(router1, router2, server);
    const auto client_target = client_node.pump().arp().table().lookup(server_ip);
    const auto router1_gateway = router1.arp_b().table().lookup(router2_a_ip);
    const auto server_client = server_node.pump().arp().table().lookup(client_ip);
    if (!default_result.ready()
        || !default_result.ok()
        || !default_result.has_value()
        || default_result.timed_out()
        || default_result.cancelled()
        || default_result.failed()
        || !bytes_eq(default_result.value_payload(), payload)
        || probe.request_count() != 1u
        || probe.reply_count() != 1u
        || probe.timeout_count() != 0u
        || probe.drop_count() != 0u
        || probe.queued_count() != 1u
        || probe.transmitted_count() != 0u
        || probe.error_count() != 0u
        || probe.last_error() != net::errc::ok
        || probe.observed_error() != net::errc::ok
        || !client_target
        || !same_mac(client_target.value(), router1_a_mac)
        || !router1_gateway
        || !same_mac(router1_gateway.value(), router2_a_mac)
        || !server_client
        || !same_mac(server_client.value(), router2_b_mac)
        || !phase_delta_matches(default_before, default_after, 2u, 0u, 2u, 0u, 1u, 1u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route default precedence smoke default mismatch\n", 9);
    }

    auto bad_specific_route = router1.add_gateway_route(
        specific_network,
        24u,
        net::Ipv4ForwardingPort::a,
        client_ip);
    const auto stored_bad_specific = router1.route_at(1u);
    if (!bad_specific_route
        || router1.route_count() != 2u
        || !stored_bad_specific.has_value()
        || !same_route(
            stored_bad_specific.value(),
            specific_network,
            24u,
            net::Ipv4ForwardingPort::a,
            true,
            client_ip,
            0u)
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            specific_network,
            24u,
            net::Ipv4ForwardingPort::a,
            true,
            client_ip,
            0u,
            false)) {
        return fail("net lab route default precedence smoke bad specific decision mismatch\n", 10);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto bad_before = capture_phase(router1, router2, server);
    auto bad_ping = probe.ping(payload, client_link.now_ticks(), 8);
    if (!bad_ping
        || bad_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route default precedence smoke bad specific submit failed\n", 11);
    }

    if (!drive_until_ready(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link,
            48)) {
        return fail("net lab route default precedence smoke bad specific stalled\n", 12);
    }

    const auto bad_result = probe.result();
    const auto bad_after = capture_phase(router1, router2, server);
    if (!bad_result.ready()
        || !bad_result.timed_out()
        || bad_result.ok()
        || bad_result.has_value()
        || bad_result.cancelled()
        || bad_result.failed()
        || probe.request_count() != 1u
        || probe.reply_count() != 0u
        || probe.timeout_count() != 1u
        || probe.drop_count() != 0u
        || probe.queued_count() != 0u
        || probe.transmitted_count() != 1u
        || probe.error_count() != 0u
        || probe.last_error() != net::errc::ok
        || probe.observed_error() != net::errc::ok
        || !phase_delta_matches(bad_before, bad_after, 1u, 0u, 0u, 0u, 0u, 0u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route default precedence smoke bad specific mismatch\n", 13);
    }

    auto host_override_route = router1.add_gateway_route(
        server_ip,
        32u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    const auto stored_host_override = router1.route_at(2u);
    if (!host_override_route
        || router1.route_count() != 3u
        || !stored_host_override.has_value()
        || !same_route(
            stored_host_override.value(),
            server_ip,
            32u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            0u)
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            server_ip,
            32u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            0u,
            false)) {
        return fail("net lab route default precedence smoke host override decision mismatch\n", 14);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto host_before = capture_phase(router1, router2, server);
    auto host_ping = probe.ping(payload, client_link.now_ticks(), 20);
    if (!host_ping
        || host_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route default precedence smoke host override submit failed\n", 15);
    }

    if (!drive_until_ready(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route default precedence smoke host override stalled\n", 16);
    }

    const auto host_result = probe.result();
    const auto host_after = capture_phase(router1, router2, server);
    if (!host_result.ready()
        || !host_result.ok()
        || !host_result.has_value()
        || host_result.timed_out()
        || host_result.cancelled()
        || host_result.failed()
        || !bytes_eq(host_result.value_payload(), payload)
        || probe.request_count() != 1u
        || probe.reply_count() != 1u
        || probe.timeout_count() != 0u
        || probe.drop_count() != 0u
        || probe.queued_count() != 0u
        || probe.transmitted_count() != 1u
        || probe.error_count() != 0u
        || probe.last_error() != net::errc::ok
        || probe.observed_error() != net::errc::ok
        || !phase_delta_matches(host_before, host_after, 2u, 0u, 2u, 0u, 1u, 1u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route default precedence smoke host override mismatch\n", 17);
    }

    if (!router1.remove_route(server_ip, 32u)
        || router1.route_count() != 2u
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            specific_network,
            24u,
            net::Ipv4ForwardingPort::a,
            true,
            client_ip,
            0u,
            false)) {
        return fail("net lab route default precedence smoke remove host route failed\n", 18);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto fallback_before = capture_phase(router1, router2, server);
    auto fallback_ping = probe.ping(payload, client_link.now_ticks(), 8);
    if (!fallback_ping
        || fallback_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route default precedence smoke fallback submit failed\n", 19);
    }

    if (!drive_until_ready(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link,
            48)) {
        return fail("net lab route default precedence smoke fallback stalled\n", 20);
    }

    const auto fallback_result = probe.result();
    const auto fallback_after = capture_phase(router1, router2, server);
    if (!fallback_result.ready()
        || !fallback_result.timed_out()
        || fallback_result.ok()
        || fallback_result.has_value()
        || fallback_result.cancelled()
        || fallback_result.failed()
        || probe.request_count() != 1u
        || probe.reply_count() != 0u
        || probe.timeout_count() != 1u
        || probe.drop_count() != 0u
        || probe.queued_count() != 0u
        || probe.transmitted_count() != 1u
        || probe.error_count() != 0u
        || probe.last_error() != net::errc::ok
        || probe.observed_error() != net::errc::ok
        || !phase_delta_matches(fallback_before, fallback_after, 1u, 0u, 0u, 0u, 0u, 0u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route default precedence smoke fallback mismatch\n", 21);
    }

    if (!router1.remove_route(specific_network, 24u)
        || router1.route_count() != 1u
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            default_network,
            0u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            0u,
            false)) {
        return fail("net lab route default precedence smoke remove specific route failed\n", 22);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto restored_before = capture_phase(router1, router2, server);
    auto restored_ping = probe.ping(payload, client_link.now_ticks(), 20);
    if (!restored_ping
        || restored_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route default precedence smoke restored submit failed\n", 23);
    }

    if (!drive_until_ready(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route default precedence smoke restored stalled\n", 24);
    }

    const auto restored_result = probe.result();
    const auto restored_after = capture_phase(router1, router2, server);
    if (!restored_result.ready()
        || !restored_result.ok()
        || !restored_result.has_value()
        || restored_result.timed_out()
        || restored_result.cancelled()
        || restored_result.failed()
        || !bytes_eq(restored_result.value_payload(), payload)
        || probe.request_count() != 1u
        || probe.reply_count() != 1u
        || probe.timeout_count() != 0u
        || probe.drop_count() != 0u
        || probe.queued_count() != 0u
        || probe.transmitted_count() != 1u
        || probe.error_count() != 0u
        || probe.last_error() != net::errc::ok
        || probe.observed_error() != net::errc::ok
        || !phase_delta_matches(restored_before, restored_after, 2u, 0u, 2u, 0u, 1u, 1u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route default precedence smoke restored mismatch\n", 25);
    }

    std::puts("net lab route default precedence smoke: ok");
    return 0;
}
