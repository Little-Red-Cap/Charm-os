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

    [[nodiscard]] bool same_explanation(
        const net::Ipv4ForwardingExplanationSnapshot& explanation,
        net::Ipv4ForwardingDisposition disposition,
        net::Ipv4ForwardingReason reason,
        net::Ipv4ForwardingPort ingress_port,
        bool has_egress,
        net::Ipv4ForwardingPort egress_port,
        bool routing_configured,
        bool has_decision,
        const net::IpAddress& destination,
        util::u8 ttl) noexcept {
        return explanation.disposition == disposition
            && explanation.reason == reason
            && explanation.ingress_port == ingress_port
            && explanation.has_egress == has_egress
            && (!has_egress || explanation.egress_port == egress_port)
            && explanation.routing_configured == routing_configured
            && explanation.has_decision == has_decision
            && same_address(explanation.destination, destination)
            && explanation.ttl == ttl;
    }
}

int main() {
    constexpr auto port_a_mac = net::MacAddress::from_bytes(0x02u, 0x34u, 0x00u, 0x00u, 0x00u, 0x11u);
    constexpr auto port_b_mac = net::MacAddress::from_bytes(0x02u, 0x34u, 0x00u, 0x00u, 0x00u, 0x12u);

    constexpr auto port_a_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto port_b_ip = net::IpAddress::ipv4(10, 0, 1, 1);
    constexpr auto router2_a_ip = net::IpAddress::ipv4(10, 0, 1, 2);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 2, 9);
    constexpr auto direct_ip = net::IpAddress::ipv4(10, 0, 3, 77);
    constexpr auto no_route_ip = net::IpAddress::ipv4(10, 0, 9, 9);
    constexpr auto direct_network = net::IpAddress::ipv4(10, 0, 3, 0);
    constexpr auto broadcast_ip = net::IpAddress::ipv4_broadcast();

    auto fail = [](const char* message, int code) noexcept {
        std::fputs(message, stderr);
        return code;
    };

    Link fallback_link_a{};
    Link fallback_link_b{};
    ForwardHop fallback_router{};
    fallback_router.configure(net::Ipv4ForwardingHopConfig{
        .retry_interval_ticks = 4,
        .max_attempts = 4,
        .icmp_ttl = 64,
    });

    auto fallback_a_init = fallback_router.init_port_a(fallback_link_a.endpoint_a(), port_a_mac, port_a_ip);
    auto fallback_b_init = fallback_router.init_port_b(fallback_link_b.endpoint_a(), port_b_mac, port_b_ip);
    if (!fallback_a_init
        || !fallback_b_init
        || !fallback_router.ready()
        || fallback_router.route_count() != 0u
        || fallback_router.port_a_has_connected_prefix()
        || fallback_router.port_b_has_connected_prefix()) {
        return fail("net lab forward explain smoke fallback router init failed\n", 1);
    }

    net::Ipv4ForwardingExplanationSnapshot invalid_snapshot{};
    const auto invalid_explanation = fallback_router.inspect_forwarding_explanation(
        net::Ipv4ForwardingPort::a,
        net::IpAddress{},
        64u);
    const auto fallback_explanation_a = fallback_router.inspect_forwarding_explanation(
        net::Ipv4ForwardingPort::a,
        server_ip,
        64u);
    const auto fallback_explanation_b = fallback_router.inspect_forwarding_explanation(
        net::Ipv4ForwardingPort::b,
        server_ip,
        64u);
    if (invalid_explanation.has_value()
        || fallback_router.inspect_forwarding_explanation(
            net::Ipv4ForwardingPort::a,
            net::IpAddress{},
            64u,
            invalid_snapshot)
        || !fallback_explanation_a.has_value()
        || !same_explanation(
            fallback_explanation_a.value(),
            net::Ipv4ForwardingDisposition::forwarded,
            net::Ipv4ForwardingReason::opposite_port_fallback,
            net::Ipv4ForwardingPort::a,
            true,
            net::Ipv4ForwardingPort::b,
            false,
            false,
            server_ip,
            64u)
        || !fallback_explanation_a.value().forwarded()
        || !fallback_explanation_a.value().uses_opposite_port_fallback()
        || fallback_explanation_a.value().uses_explicit_route()
        || fallback_explanation_a.value().uses_connected_prefix()
        || !fallback_explanation_b.has_value()
        || !same_explanation(
            fallback_explanation_b.value(),
            net::Ipv4ForwardingDisposition::forwarded,
            net::Ipv4ForwardingReason::opposite_port_fallback,
            net::Ipv4ForwardingPort::b,
            true,
            net::Ipv4ForwardingPort::a,
            false,
            false,
            server_ip,
            64u)) {
        return fail("net lab forward explain smoke fallback explanation mismatch\n", 2);
    }

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
    auto host_route = router.add_gateway_route(
        server_ip,
        32u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip,
        7u);
    auto direct_route = router.add_direct_route(
        direct_network,
        24u,
        net::Ipv4ForwardingPort::a,
        2u);
    if (!port_a_init
        || !port_b_init
        || !prefix_a
        || !prefix_b
        || !host_route
        || !direct_route
        || !router.ready()
        || !router.port_a_has_connected_prefix()
        || !router.port_b_has_connected_prefix()
        || router.port_a_prefix_length() != 24u
        || router.port_b_prefix_length() != 24u
        || router.route_count() != 2u) {
        return fail("net lab forward explain smoke router init failed\n", 3);
    }

    net::Ipv4ForwardingExplanationSnapshot explicit_out{};
    const auto local_explanation = router.inspect_forwarding_explanation(
        net::Ipv4ForwardingPort::b,
        port_a_ip,
        64u);
    const auto broadcast_explanation = router.inspect_forwarding_explanation(
        net::Ipv4ForwardingPort::a,
        broadcast_ip,
        64u);
    const auto ttl_explanation = router.inspect_forwarding_explanation(
        net::Ipv4ForwardingPort::a,
        server_ip,
        1u);
    const auto explicit_explanation = router.inspect_forwarding_explanation(
        net::Ipv4ForwardingPort::a,
        server_ip,
        64u);
    const auto connected_explanation = router.inspect_forwarding_explanation(
        net::Ipv4ForwardingPort::a,
        router2_a_ip,
        64u);
    const auto direct_explanation = router.inspect_forwarding_explanation(
        net::Ipv4ForwardingPort::b,
        direct_ip,
        64u);
    const auto miss_explanation = router.inspect_forwarding_explanation(
        net::Ipv4ForwardingPort::a,
        no_route_ip,
        64u);
    if (!local_explanation.has_value()
        || !same_explanation(
            local_explanation.value(),
            net::Ipv4ForwardingDisposition::local_dropped,
            net::Ipv4ForwardingReason::local_address,
            net::Ipv4ForwardingPort::b,
            false,
            net::Ipv4ForwardingPort::a,
            true,
            false,
            port_a_ip,
            64u)
        || !local_explanation.value().local_dropped()
        || local_explanation.value().emits_icmp_error()
        || !broadcast_explanation.has_value()
        || !same_explanation(
            broadcast_explanation.value(),
            net::Ipv4ForwardingDisposition::local_dropped,
            net::Ipv4ForwardingReason::limited_broadcast,
            net::Ipv4ForwardingPort::a,
            false,
            net::Ipv4ForwardingPort::a,
            true,
            false,
            broadcast_ip,
            64u)
        || !ttl_explanation.has_value()
        || !same_explanation(
            ttl_explanation.value(),
            net::Ipv4ForwardingDisposition::ttl_expired,
            net::Ipv4ForwardingReason::ttl_exhausted,
            net::Ipv4ForwardingPort::a,
            false,
            net::Ipv4ForwardingPort::a,
            true,
            false,
            server_ip,
            1u)
        || !ttl_explanation.value().ttl_expired()
        || !ttl_explanation.value().emits_icmp_error()
        || !explicit_explanation.has_value()
        || !router.inspect_forwarding_explanation(
            net::Ipv4ForwardingPort::a,
            server_ip,
            64u,
            explicit_out)
        || !same_explanation(
            explicit_explanation.value(),
            net::Ipv4ForwardingDisposition::forwarded,
            net::Ipv4ForwardingReason::explicit_route,
            net::Ipv4ForwardingPort::a,
            true,
            net::Ipv4ForwardingPort::b,
            true,
            true,
            server_ip,
            64u)
        || !same_explanation(
            explicit_out,
            net::Ipv4ForwardingDisposition::forwarded,
            net::Ipv4ForwardingReason::explicit_route,
            net::Ipv4ForwardingPort::a,
            true,
            net::Ipv4ForwardingPort::b,
            true,
            true,
            server_ip,
            64u)
        || !explicit_explanation.value().forwarded()
        || !explicit_explanation.value().uses_explicit_route()
        || !same_decision(
            explicit_explanation.value().decision,
            server_ip,
            32u,
            net::Ipv4ForwardingPort::b,
            true,
            router2_a_ip,
            7u,
            false)
        || !connected_explanation.has_value()
        || !same_explanation(
            connected_explanation.value(),
            net::Ipv4ForwardingDisposition::forwarded,
            net::Ipv4ForwardingReason::connected_prefix,
            net::Ipv4ForwardingPort::a,
            true,
            net::Ipv4ForwardingPort::b,
            true,
            true,
            router2_a_ip,
            64u)
        || !connected_explanation.value().uses_connected_prefix()
        || !same_decision(
            connected_explanation.value().decision,
            net::IpAddress::ipv4(10, 0, 1, 0),
            24u,
            net::Ipv4ForwardingPort::b,
            false,
            net::IpAddress{},
            0u,
            true)
        || !direct_explanation.has_value()
        || !same_explanation(
            direct_explanation.value(),
            net::Ipv4ForwardingDisposition::forwarded,
            net::Ipv4ForwardingReason::explicit_route,
            net::Ipv4ForwardingPort::b,
            true,
            net::Ipv4ForwardingPort::a,
            true,
            true,
            direct_ip,
            64u)
        || !same_decision(
            direct_explanation.value().decision,
            direct_network,
            24u,
            net::Ipv4ForwardingPort::a,
            false,
            net::IpAddress{},
            2u,
            false)
        || !miss_explanation.has_value()
        || !same_explanation(
            miss_explanation.value(),
            net::Ipv4ForwardingDisposition::destination_unreachable,
            net::Ipv4ForwardingReason::no_route,
            net::Ipv4ForwardingPort::a,
            false,
            net::Ipv4ForwardingPort::a,
            true,
            false,
            no_route_ip,
            64u)
        || !miss_explanation.value().destination_unreachable()
        || !miss_explanation.value().emits_icmp_error()) {
        return fail("net lab forward explain smoke routed explanation mismatch\n", 4);
    }

    std::puts("net lab forward explain smoke: ok");
    return 0;
}
