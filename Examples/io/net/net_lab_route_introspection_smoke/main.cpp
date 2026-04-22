#include <cstdio>

import charm.net;
import util.core;

namespace {
    using Link = net::lab::DuplexLink<128>;
    using ForwardHop = net::Ipv4ForwardingHop<128, 4, 128, 4>;

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

    [[nodiscard]] bool same_route(const net::Ipv4ForwardingRoute& route,
                                  const net::IpAddress& network,
                                  util::u8 prefix_length,
                                  net::Ipv4ForwardingPort egress_port,
                                  bool has_next_hop,
                                  const net::IpAddress& next_hop) noexcept {
        return same_address(route.network, network)
            && route.prefix_length == prefix_length
            && route.egress_port == egress_port
            && route.has_next_hop == has_next_hop
            && same_address(route.next_hop, next_hop);
    }

    [[nodiscard]] bool same_decision(const net::Ipv4ForwardingDecisionSnapshot& decision,
                                     const net::IpAddress& network,
                                     util::u8 prefix_length,
                                     net::Ipv4ForwardingPort egress_port,
                                     bool has_next_hop,
                                     const net::IpAddress& next_hop,
                                     bool from_connected_prefix) noexcept {
        return same_address(decision.network, network)
            && decision.prefix_length == prefix_length
            && decision.egress_port == egress_port
            && decision.has_next_hop == has_next_hop
            && same_address(decision.next_hop, next_hop)
            && decision.from_connected_prefix == from_connected_prefix;
    }
}

int main() {
    constexpr auto port_a_mac = net::MacAddress::from_bytes(0x02u, 0x32u, 0x00u, 0x00u, 0x00u, 0x11u);
    constexpr auto port_b_mac = net::MacAddress::from_bytes(0x02u, 0x32u, 0x00u, 0x00u, 0x00u, 0x12u);

    constexpr auto port_a_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto port_b_ip = net::IpAddress::ipv4(10, 0, 1, 1);
    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto router2_a_ip = net::IpAddress::ipv4(10, 0, 1, 2);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 2, 9);
    constexpr auto unreachable_ip = net::IpAddress::ipv4(10, 0, 2, 77);

    auto fail = [](const char* message, int code) noexcept {
        std::fputs(message, stderr);
        return code;
    };

    Link link_a{};
    Link link_b{};

    ForwardHop router{};
    router.configure(net::Ipv4ForwardingHopConfig{
        .retry_interval_ticks = 4,
        .max_attempts = 4,
        .icmp_ttl = 64,
    });

    auto port_a_init = router.init_port_a(link_a.endpoint_a(), port_a_mac, port_a_ip);
    auto port_b_init = router.init_port_b(link_b.endpoint_a(), port_b_mac, port_b_ip);
    auto prefix_a = router.set_port_a_prefix_length(24u);
    auto prefix_b = router.set_port_b_prefix_length(24u);
    auto default_route = router.set_gateway_route(
        net::IpAddress::ipv4_any(),
        0u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    auto bad_specific_route = router.add_gateway_route(
        net::IpAddress::ipv4(10, 0, 2, 123),
        24u,
        net::Ipv4ForwardingPort::a,
        client_ip);
    auto host_route = router.set_gateway_route(
        server_ip,
        32u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    if (!port_a_init
        || !port_b_init
        || !prefix_a
        || !prefix_b
        || !default_route
        || !bad_specific_route
        || !host_route
        || !router.ready()
        || router.route_count() != 3u) {
        return fail("net lab route introspection smoke router init failed\n", 1);
    }

    const auto route0_opt = router.route_at(0u);
    const auto route2_opt = router.route_at(2u);
    const auto route3_opt = router.route_at(3u);
    net::Ipv4ForwardingRoute route1{};
    net::Ipv4ForwardingDecisionSnapshot decision{};
    if (!route0_opt.has_value()
        || !router.route_at(1u, route1)
        || !route2_opt.has_value()
        || route3_opt.has_value()
        || router.route_at(3u, route1)
        || !same_route(
            route0_opt.value(),
            net::IpAddress::ipv4_any(),
            0u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip)
        || !same_route(
            route1,
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::a,
            true,
            client_ip)
        || !same_route(
            route2_opt.value(),
            server_ip,
            32u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip)
        || !router.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            server_ip,
            32u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            false)
        || !router.inspect_forwarding_decision(router2_a_ip, decision)
        || !same_decision(
            decision,
            net::IpAddress::ipv4(10, 0, 1, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            false,
            net::IpAddress{},
            true)
        || !router.inspect_forwarding_decision(unreachable_ip, decision)
        || !same_decision(
            decision,
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::a,
            true,
            client_ip,
            false)) {
        return fail("net lab route introspection smoke initial snapshot mismatch\n", 2);
    }

    if (!router.remove_route(net::IpAddress::ipv4_any(), 0u)
        || router.route_count() != 2u) {
        return fail("net lab route introspection smoke remove default failed\n", 3);
    }

    const auto after_default_remove0 = router.route_at(0u);
    const auto after_default_remove1 = router.route_at(1u);
    if (!after_default_remove0.has_value()
        || !after_default_remove1.has_value()
        || !same_route(
            after_default_remove0.value(),
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::a,
            true,
            client_ip)
        || !same_route(
            after_default_remove1.value(),
            server_ip,
            32u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip)
        || !router.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            server_ip,
            32u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            false)) {
        return fail("net lab route introspection smoke remove default mismatch\n", 4);
    }

    auto restore_default = router.set_gateway_route(
        net::IpAddress::ipv4_any(),
        0u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    if (!restore_default
        || router.route_count() != 3u) {
        return fail("net lab route introspection smoke restore default failed\n", 5);
    }

    const auto restored_default = router.route_at(2u);
    if (!restored_default.has_value()
        || !same_route(
            restored_default.value(),
            net::IpAddress::ipv4_any(),
            0u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip)) {
        return fail("net lab route introspection smoke restored default mismatch\n", 6);
    }

    if (!router.remove_route(server_ip, 32u)
        || router.route_count() != 2u
        || !router.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            net::IpAddress::ipv4(10, 0, 2, 0),
            24u,
            net::Ipv4ForwardingPort::a,
            true,
            client_ip,
            false)) {
        return fail("net lab route introspection smoke remove host mismatch\n", 7);
    }

    if (!router.remove_route(net::IpAddress::ipv4(10, 0, 2, 250), 24u)
        || router.route_count() != 1u
        || !router.inspect_forwarding_decision(server_ip, decision)
        || !same_decision(
            decision,
            net::IpAddress::ipv4_any(),
            0u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            false)) {
        return fail("net lab route introspection smoke fallback mismatch\n", 8);
    }

    router.clear_routes();
    const auto clear_decision_server = router.inspect_forwarding_decision(server_ip);
    const auto clear_decision_port_b = router.inspect_forwarding_decision(router2_a_ip);
    const auto clear_decision_port_a = router.inspect_forwarding_decision(client_ip);
    if (router.route_count() != 0u
        || clear_decision_server.has_value()
        || !clear_decision_port_b.has_value()
        || !same_decision(
            clear_decision_port_b.value(),
            net::IpAddress::ipv4(10, 0, 1, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            false,
            net::IpAddress{},
            true)
        || !clear_decision_port_a.has_value()
        || !same_decision(
            clear_decision_port_a.value(),
            net::IpAddress::ipv4(10, 0, 0, 0),
            24u,
            net::Ipv4ForwardingPort::a,
            false,
            net::IpAddress{},
            true)) {
        return fail("net lab route introspection smoke clear routes mismatch\n", 9);
    }

    std::puts("net lab route introspection smoke: ok");
    return 0;
}
