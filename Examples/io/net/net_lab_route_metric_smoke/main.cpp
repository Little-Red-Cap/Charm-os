#include <cstdio>

import charm.net;
import util.core;

namespace {
    using Link = net::lab::DuplexLink<192>;
    using HostPump = net::IcmpStackPump<192, 4, 192, 4, 64>;
    using HostNode = net::lab::StackNode<HostPump>;
    using ForwardHop = net::Ipv4ForwardingHop<192, 4, 192, 4>;

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

    [[nodiscard]] bool same_address(const net::IpAddress& lhs,
                                    const net::IpAddress& rhs) noexcept {
        if (lhs.family != rhs.family) {
            return false;
        }

        const auto limit = lhs.is_ipv4() ? 4u : lhs.bytes.size();
        for (util::usize index = 0; index < limit; ++index) {
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
        return same_address(route.network, network)
            && route.prefix_length == prefix_length
            && route.egress_port == egress_port
            && route.has_next_hop == has_next_hop
            && same_address(route.next_hop, next_hop)
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
        return same_address(decision.network, network)
            && decision.prefix_length == prefix_length
            && decision.egress_port == egress_port
            && decision.has_next_hop == has_next_hop
            && same_address(decision.next_hop, next_hop)
            && decision.metric == metric
            && decision.from_connected_prefix == from_connected_prefix;
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
            std::fprintf(stderr, "route metric client service error=%d\n", static_cast<int>(client_progress.error()));
            return false;
        }

        auto router1_progress = router1.service(1);
        if (!router1_progress) {
            std::fprintf(stderr, "route metric router1 service error=%d\n", static_cast<int>(router1_progress.error()));
            return false;
        }

        auto router2_progress = router2.service(1);
        if (!router2_progress) {
            std::fprintf(stderr, "route metric router2 service error=%d\n", static_cast<int>(router2_progress.error()));
            return false;
        }

        auto server_progress = server_node.service(1);
        if (!server_progress) {
            std::fprintf(stderr, "route metric server service error=%d\n", static_cast<int>(server_progress.error()));
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

            if (probe.ready()
                && client_node.pump().pending_count() == 0
                && router1.pending_count() == 0
                && router2.pending_count() == 0
                && server_node.pump().pending_count() == 0
                && client_link.idle()
                && middle_link.idle()
                && server_link.idle()) {
                return true;
            }
        }
        return false;
    }
}

int main() {
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x33u, 0x00u, 0x00u, 0x00u, 0x02u);
    constexpr auto router1_a_mac = net::MacAddress::from_bytes(0x02u, 0x33u, 0x00u, 0x00u, 0x00u, 0x11u);
    constexpr auto router1_b_mac = net::MacAddress::from_bytes(0x02u, 0x33u, 0x00u, 0x00u, 0x00u, 0x12u);
    constexpr auto router2_a_mac = net::MacAddress::from_bytes(0x02u, 0x33u, 0x00u, 0x00u, 0x00u, 0x21u);
    constexpr auto router2_b_mac = net::MacAddress::from_bytes(0x02u, 0x33u, 0x00u, 0x00u, 0x00u, 0x22u);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0x33u, 0x00u, 0x00u, 0x00u, 0x09u);

    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto router1_a_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto router1_b_ip = net::IpAddress::ipv4(10, 0, 1, 1);
    constexpr auto router2_a_ip = net::IpAddress::ipv4(10, 0, 1, 2);
    constexpr auto router2_b_ip = net::IpAddress::ipv4(10, 0, 2, 1);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 2, 9);

    static constexpr util::u8 payload[]{'m', 'e', 't', 'r', 'i', 'c'};

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
        return fail("net lab route metric smoke client init failed\n", 1);
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
    auto router1_default_route = router1.add_gateway_route(
        net::IpAddress::ipv4_any(),
        0u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip,
        50u);
    if (!router1_a_init
        || !router1_b_init
        || !router1_prefix_a
        || !router1_prefix_b
        || !router1_default_route
        || router1.route_count() != 1u
        || !router1.ready()) {
        return fail("net lab route metric smoke router1 init failed\n", 2);
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
    auto router2_return_route = router2.add_gateway_route(
        net::IpAddress::ipv4(10, 0, 0, 0),
        24u,
        net::Ipv4ForwardingPort::a,
        router1_b_ip,
        0u);
    if (!router2_a_init
        || !router2_b_init
        || !router2_prefix_a
        || !router2_prefix_b
        || !router2_return_route
        || router2.route_count() != 1u
        || !router2.ready()) {
        return fail("net lab route metric smoke router2 init failed\n", 3);
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
        return fail("net lab route metric smoke server init failed\n", 4);
    }

    net::icmp::echo::Probe<16> probe{server_ip};
    net::icmp::echo::AutoReplyServer server{};
    auto bound_probe = probe.bind(client_node.pump());
    auto bound_server = server.bind(server_node.pump());
    if (!bound_probe
        || !bound_server
        || !client_node.pump().has_echo_sink()
        || !server_node.pump().has_echo_sink()) {
        return fail("net lab route metric smoke protocol bind failed\n", 5);
    }

    net::Ipv4ForwardingDecisionSnapshot decision{};
    const auto route0 = router1.route_at(0u);
    if (!route0.has_value()
        || !same_route(
            route0.value(),
            net::IpAddress::ipv4_any(),
            0u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            50u)
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            net::IpAddress::ipv4_any(),
            0u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            50u,
            false)) {
        return fail("net lab route metric smoke default decision mismatch\n", 6);
    }

    auto baseline_ping = probe.ping(payload, client_link.now_ticks(), 20);
    if (!baseline_ping
        || baseline_ping.value().disposition != net::IcmpSendDisposition::queued
        || !probe.pending()) {
        return fail("net lab route metric smoke baseline submit failed\n", 7);
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
        return fail("net lab route metric smoke baseline stalled\n", 8);
    }

    const auto baseline_result = probe.result();
    if (!baseline_result.ready()
        || !baseline_result.ok()
        || !baseline_result.has_value()
        || baseline_result.timed_out()
        || baseline_result.cancelled()
        || baseline_result.failed()
        || !bytes_eq(baseline_result.value_payload(), payload)
        || probe.request_count() != 1
        || probe.reply_count() != 1
        || probe.timeout_count() != 0
        || !client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()) {
        return fail("net lab route metric smoke baseline mismatch\n", 9);
    }

    auto bad_specific_route = router1.add_gateway_route(
        net::IpAddress::ipv4(10, 0, 2, 123),
        24u,
        net::Ipv4ForwardingPort::a,
        client_ip,
        100u);
    auto good_specific_route = router1.add_gateway_route(
        net::IpAddress::ipv4(10, 0, 2, 42),
        24u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip,
        10u);
    if (!bad_specific_route
        || !good_specific_route
        || router1.route_count() != 3u) {
        return fail("net lab route metric smoke specific route add failed\n", 10);
    }

    const auto route1 = router1.route_at(1u);
    const auto route2 = router1.route_at(2u);
    if (!route1.has_value()
        || !route2.has_value()
        || !same_route(
            route1.value(),
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::a,
            true,
            client_ip,
            100u)
        || !same_route(
            route2.value(),
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            10u)
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            10u,
            false)) {
        return fail("net lab route metric smoke lower metric selection mismatch\n", 11);
    }

    auto equal_metric_late_bad_route = router1.add_gateway_route(
        net::IpAddress::ipv4(10, 0, 2, 99),
        24u,
        net::Ipv4ForwardingPort::a,
        client_ip,
        10u);
    if (!equal_metric_late_bad_route
        || router1.route_count() != 4u
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            10u,
            false)) {
        return fail("net lab route metric smoke equal metric tie mismatch\n", 12);
    }

    auto lower_metric_ping = probe.ping(payload, client_link.now_ticks(), 20);
    if (!lower_metric_ping
        || lower_metric_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route metric smoke lower metric submit failed\n", 13);
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
        return fail("net lab route metric smoke lower metric stalled\n", 14);
    }

    const auto lower_metric_result = probe.result();
    if (!lower_metric_result.ready()
        || !lower_metric_result.ok()
        || !lower_metric_result.has_value()
        || lower_metric_result.timed_out()
        || lower_metric_result.cancelled()
        || lower_metric_result.failed()
        || !bytes_eq(lower_metric_result.value_payload(), payload)
        || probe.request_count() != 2
        || probe.reply_count() != 2
        || probe.timeout_count() != 0
        || !client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()) {
        return fail("net lab route metric smoke lower metric mismatch\n", 15);
    }

    auto bad_host_route = router1.add_gateway_route(
        server_ip,
        32u,
        net::Ipv4ForwardingPort::a,
        client_ip,
        250u);
    if (!bad_host_route
        || router1.route_count() != 5u
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            server_ip,
            32u,
            net::Ipv4ForwardingPort::a,
            true,
            client_ip,
            250u,
            false)) {
        return fail("net lab route metric smoke host route precedence mismatch\n", 16);
    }

    auto bad_host_ping = probe.ping(payload, client_link.now_ticks(), 8);
    if (!bad_host_ping
        || bad_host_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route metric smoke bad host submit failed\n", 17);
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
        return fail("net lab route metric smoke bad host stalled\n", 18);
    }

    const auto bad_host_result = probe.result();
    if (!bad_host_result.ready()
        || !bad_host_result.timed_out()
        || bad_host_result.ok()
        || bad_host_result.has_value()
        || bad_host_result.cancelled()
        || bad_host_result.failed()
        || probe.request_count() != 3
        || probe.reply_count() != 2
        || probe.timeout_count() != 1
        || !client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()) {
        return fail("net lab route metric smoke bad host mismatch\n", 19);
    }

    auto good_host_route = router1.add_gateway_route(
        server_ip,
        32u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip,
        5u);
    if (!good_host_route
        || router1.route_count() != 6u
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            server_ip,
            32u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            5u,
            false)) {
        return fail("net lab route metric smoke good host decision mismatch\n", 20);
    }

    auto good_host_ping = probe.ping(payload, client_link.now_ticks(), 20);
    if (!good_host_ping
        || good_host_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route metric smoke good host submit failed\n", 21);
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
        return fail("net lab route metric smoke good host stalled\n", 22);
    }

    const auto good_host_result = probe.result();
    const auto client_target = client_node.pump().arp().table().lookup(server_ip);
    if (!good_host_result.ready()
        || !good_host_result.ok()
        || !good_host_result.has_value()
        || good_host_result.timed_out()
        || good_host_result.cancelled()
        || good_host_result.failed()
        || !bytes_eq(good_host_result.value_payload(), payload)
        || probe.request_count() != 4
        || probe.reply_count() != 3
        || probe.timeout_count() != 1
        || !client_target
        || !same_mac(client_target.value(), router1_a_mac)
        || !client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()
        || router1.pending_count() != 0u
        || router2.pending_count() != 0u
        || client_node.pump().pending_count() != 0u
        || server_node.pump().pending_count() != 0u) {
        return fail("net lab route metric smoke good host mismatch\n", 23);
    }

    std::puts("net lab route metric smoke: ok");
    return 0;
}
