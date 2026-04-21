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
            std::fprintf(stderr, "route precise delete client service error=%d\n", static_cast<int>(client_progress.error()));
            return false;
        }

        auto router1_progress = router1.service(1);
        if (!router1_progress) {
            std::fprintf(stderr, "route precise delete router1 service error=%d\n", static_cast<int>(router1_progress.error()));
            return false;
        }

        auto router2_progress = router2.service(1);
        if (!router2_progress) {
            std::fprintf(stderr, "route precise delete router2 service error=%d\n", static_cast<int>(router2_progress.error()));
            return false;
        }

        auto server_progress = server_node.service(1);
        if (!server_progress) {
            std::fprintf(stderr, "route precise delete server service error=%d\n", static_cast<int>(server_progress.error()));
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
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x34u, 0x00u, 0x00u, 0x00u, 0x02u);
    constexpr auto router1_a_mac = net::MacAddress::from_bytes(0x02u, 0x34u, 0x00u, 0x00u, 0x00u, 0x11u);
    constexpr auto router1_b_mac = net::MacAddress::from_bytes(0x02u, 0x34u, 0x00u, 0x00u, 0x00u, 0x12u);
    constexpr auto router2_a_mac = net::MacAddress::from_bytes(0x02u, 0x34u, 0x00u, 0x00u, 0x00u, 0x21u);
    constexpr auto router2_b_mac = net::MacAddress::from_bytes(0x02u, 0x34u, 0x00u, 0x00u, 0x00u, 0x22u);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0x34u, 0x00u, 0x00u, 0x00u, 0x09u);

    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto router1_a_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto router1_b_ip = net::IpAddress::ipv4(10, 0, 1, 1);
    constexpr auto router2_a_ip = net::IpAddress::ipv4(10, 0, 1, 2);
    constexpr auto router2_b_ip = net::IpAddress::ipv4(10, 0, 2, 1);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 2, 9);
    constexpr auto bad_gateway = net::IpAddress::ipv4(10, 0, 1, 99);

    static constexpr util::u8 payload[]{'p', 'r', 'e', 'c', 'i', 's', 'e'};

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
        return fail("net lab route precise delete smoke client init failed\n", 1);
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
    auto good_route = router1.add_gateway_route(
        net::IpAddress::ipv4(10, 0, 2, 0),
        24u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip,
        20u);
    auto bad_route = router1.add_gateway_route(
        net::IpAddress::ipv4(10, 0, 2, 123),
        24u,
        net::Ipv4ForwardingPort::b,
        bad_gateway,
        10u);
    if (!router1_a_init
        || !router1_b_init
        || !router1_prefix_a
        || !router1_prefix_b
        || !good_route
        || !bad_route
        || router1.route_count() != 2u
        || !router1.ready()) {
        return fail("net lab route precise delete smoke router1 init failed\n", 2);
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
        return fail("net lab route precise delete smoke router2 init failed\n", 3);
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
        return fail("net lab route precise delete smoke server init failed\n", 4);
    }

    net::icmp::echo::Probe<16> probe{server_ip};
    net::icmp::echo::AutoReplyServer server{};
    auto bound_probe = probe.bind(client_node.pump());
    auto bound_server = server.bind(server_node.pump());
    if (!bound_probe
        || !bound_server
        || !client_node.pump().has_echo_sink()
        || !server_node.pump().has_echo_sink()) {
        return fail("net lab route precise delete smoke protocol bind failed\n", 5);
    }

    net::Ipv4ForwardingDecisionSnapshot decision{};
    const auto route0 = router1.route_at(0u);
    const auto route1 = router1.route_at(1u);
    if (!route0.has_value()
        || !route1.has_value()
        || !same_route(
            route0.value(),
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            20u)
        || !same_route(
            route1.value(),
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            true,
            bad_gateway,
            10u)
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            true,
            bad_gateway,
            10u,
            false)) {
        return fail("net lab route precise delete smoke initial snapshot mismatch\n", 6);
    }

    auto bad_ping = probe.ping(payload, client_link.now_ticks(), 8);
    if (!bad_ping
        || bad_ping.value().disposition != net::IcmpSendDisposition::queued
        || !probe.pending()) {
        return fail("net lab route precise delete smoke bad ping submit failed\n", 7);
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
        return fail("net lab route precise delete smoke bad ping stalled\n", 8);
    }

    const auto bad_result = probe.result();
    if (!bad_result.ready()
        || !bad_result.timed_out()
        || bad_result.ok()
        || bad_result.has_value()
        || bad_result.cancelled()
        || bad_result.failed()
        || bad_result.identifier() != bad_ping.value().info.identifier
        || bad_result.sequence() != bad_ping.value().info.sequence
        || probe.request_count() != 1u
        || probe.reply_count() != 0u
        || probe.timeout_count() != 1u
        || probe.drop_count() != 0u
        || probe.queued_count() != 1u
        || probe.transmitted_count() != 0u
        || router1.forwarded_count() != 1u
        || router2.forwarded_count() != 0u
        || router1.destination_unreachable_count() != 0u
        || router2.destination_unreachable_count() != 0u
        || server.request_count() != 0u
        || server.reply_count() != 0u
        || server.drop_count() != 0u
        || !client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()) {
        return fail("net lab route precise delete smoke bad ping mismatch\n", 9);
    }

    if (!router1.remove_route_at(1u)
        || router1.route_count() != 1u
        || router1.remove_route_at(1u)) {
        return fail("net lab route precise delete smoke remove index failed\n", 10);
    }

    const auto after_route0 = router1.route_at(0u);
    const auto after_route1 = router1.route_at(1u);
    if (!after_route0.has_value()
        || after_route1.has_value()
        || !same_route(
            after_route0.value(),
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            20u)
        || !router1.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            20u,
            false)) {
        return fail("net lab route precise delete smoke snapshot after remove mismatch\n", 11);
    }

    auto good_ping = probe.ping(payload, client_link.now_ticks(), 20);
    if (!good_ping
        || good_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route precise delete smoke good ping submit failed\n", 12);
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
        return fail("net lab route precise delete smoke good ping stalled\n", 13);
    }

    const auto good_result = probe.result();
    const auto client_target = client_node.pump().arp().table().lookup(server_ip);
    if (!good_result.ready()
        || !good_result.ok()
        || !good_result.has_value()
        || good_result.timed_out()
        || good_result.cancelled()
        || good_result.failed()
        || !bytes_eq(good_result.value_payload(), payload)
        || probe.request_count() != 2u
        || probe.reply_count() != 1u
        || probe.timeout_count() != 1u
        || !client_target
        || !same_mac(client_target.value(), router1_a_mac)
        || !client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()
        || router1.pending_count() != 0u
        || router2.pending_count() != 0u
        || client_node.pump().pending_count() != 0u
        || server_node.pump().pending_count() != 0u) {
        return fail("net lab route precise delete smoke good ping mismatch\n", 14);
    }

    std::puts("net lab route precise delete smoke: ok");
    return 0;
}
