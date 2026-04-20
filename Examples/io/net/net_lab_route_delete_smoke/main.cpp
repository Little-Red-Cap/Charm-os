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

    [[nodiscard]] bool system_idle(const net::icmp::trace::Probe<16>& probe,
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

    [[nodiscard]] bool service_step(net::icmp::trace::Probe<16>& probe,
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
                         "route delete client service error=%d\n",
                         static_cast<int>(client_progress.error()));
            return false;
        }

        auto router1_progress = router1.service(1);
        if (!router1_progress) {
            std::fprintf(stderr,
                         "route delete router1 service error=%d\n",
                         static_cast<int>(router1_progress.error()));
            return false;
        }

        auto router2_progress = router2.service(1);
        if (!router2_progress) {
            std::fprintf(stderr,
                         "route delete router2 service error=%d\n",
                         static_cast<int>(router2_progress.error()));
            return false;
        }

        auto server_progress = server_node.service(1);
        if (!server_progress) {
            std::fprintf(stderr,
                         "route delete server service error=%d\n",
                         static_cast<int>(server_progress.error()));
            return false;
        }

        probe.tick(client_link.now_ticks());
        return true;
    }

    [[nodiscard]] bool drive_until_ready(net::icmp::trace::Probe<16>& probe,
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
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x31u, 0x00u, 0x00u, 0x00u, 0x02u);
    constexpr auto router1_a_mac = net::MacAddress::from_bytes(0x02u, 0x31u, 0x00u, 0x00u, 0x00u, 0x11u);
    constexpr auto router1_b_mac = net::MacAddress::from_bytes(0x02u, 0x31u, 0x00u, 0x00u, 0x00u, 0x12u);
    constexpr auto router2_a_mac = net::MacAddress::from_bytes(0x02u, 0x31u, 0x00u, 0x00u, 0x00u, 0x21u);
    constexpr auto router2_b_mac = net::MacAddress::from_bytes(0x02u, 0x31u, 0x00u, 0x00u, 0x00u, 0x22u);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0x31u, 0x00u, 0x00u, 0x00u, 0x09u);

    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto router1_a_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto router1_b_ip = net::IpAddress::ipv4(10, 0, 1, 1);
    constexpr auto router2_a_ip = net::IpAddress::ipv4(10, 0, 1, 2);
    constexpr auto router2_b_ip = net::IpAddress::ipv4(10, 0, 2, 1);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 2, 9);
    constexpr auto unreachable_ip = net::IpAddress::ipv4(10, 0, 2, 77);

    static constexpr util::u8 payload[]{'d', 'e', 'l', 'e', 't', 'e'};

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
        return fail("net lab route delete smoke client init failed\n", 1);
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
        net::IpAddress::ipv4_any(),
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
        return fail("net lab route delete smoke router1 init failed\n", 2);
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
        net::IpAddress::ipv4(10, 0, 0, 99),
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
        return fail("net lab route delete smoke router2 init failed\n", 3);
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
        return fail("net lab route delete smoke server init failed\n", 4);
    }

    net::icmp::trace::Probe<16> probe{server_ip};
    net::icmp::echo::AutoReplyServer server{};
    auto bound_probe = probe.bind(client_node.pump());
    auto bound_server = server.bind(server_node.pump());
    if (!bound_probe
        || !bound_server
        || !client_node.pump().has_echo_sink()
        || !client_node.pump().has_error_quote_sink()
        || !server_node.pump().has_echo_sink()) {
        return fail("net lab route delete smoke protocol bind failed\n", 5);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto baseline_before = capture_phase(router1, router2, server);
    auto baseline_probe = probe.probe(4u, payload, client_link.now_ticks(), 20);
    if (!baseline_probe
        || baseline_probe.value().disposition != net::IcmpSendDisposition::queued
        || !probe.pending()) {
        return fail("net lab route delete smoke baseline submit failed\n", 6);
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
        return fail("net lab route delete smoke baseline stalled\n", 7);
    }

    const auto baseline_result = probe.result();
    const auto baseline_after = capture_phase(router1, router2, server);
    if (!baseline_result.ready()
        || !baseline_result.reached()
        || !baseline_result.ok()
        || !baseline_result.has_value()
        || baseline_result.response_type != net::IcmpType::echo_reply
        || baseline_result.response_code != 0u
        || baseline_result.ttl != 4u
        || !same_ipv4(baseline_result.responder, server_ip)
        || !bytes_eq(baseline_result.value_payload(), payload)
        || probe.request_count() != 1u
        || probe.response_count() != 1u
        || probe.hop_count() != 0u
        || probe.reach_count() != 1u
        || probe.unreachable_count() != 0u
        || probe.timeout_count() != 0u
        || probe.error_count() != 0u
        || probe.queued_count() != 1u
        || probe.transmitted_count() != 0u
        || !phase_delta_matches(baseline_before, baseline_after, 2u, 0u, 2u, 0u, 1u, 1u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route delete smoke baseline mismatch\n", 8);
    }

    auto bad_specific_route = router1.set_gateway_route(
        net::IpAddress::ipv4(10, 0, 2, 123),
        24u,
        net::Ipv4ForwardingPort::a,
        client_ip);
    if (!bad_specific_route || router1.route_count() != 2u) {
        return fail("net lab route delete smoke add bad route failed\n", 9);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto bad_specific_before = capture_phase(router1, router2, server);
    auto bad_specific_probe = probe.probe(4u, payload, client_link.now_ticks(), 8);
    if (!bad_specific_probe
        || bad_specific_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route delete smoke bad route submit failed\n", 10);
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
        return fail("net lab route delete smoke bad route stalled\n", 11);
    }

    const auto bad_specific_result = probe.result();
    const auto bad_specific_after = capture_phase(router1, router2, server);
    if (!bad_specific_result.ready()
        || !bad_specific_result.timed_out()
        || bad_specific_result.ok()
        || bad_specific_result.has_value()
        || bad_specific_result.ttl != 4u
        || probe.request_count() != 1u
        || probe.response_count() != 0u
        || probe.hop_count() != 0u
        || probe.reach_count() != 0u
        || probe.unreachable_count() != 0u
        || probe.timeout_count() != 1u
        || probe.error_count() != 0u
        || probe.queued_count() != 0u
        || probe.transmitted_count() != 1u
        || !phase_delta_matches(bad_specific_before, bad_specific_after, 1u, 0u, 0u, 0u, 0u, 0u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route delete smoke bad route mismatch\n", 12);
    }

    if (!router1.remove_route(net::IpAddress::ipv4(10, 0, 2, 250), 24u)
        || router1.route_count() != 1u
        || router1.remove_route(net::IpAddress::ipv4(10, 0, 2, 250), 24u)) {
        return fail("net lab route delete smoke remove specific route failed\n", 13);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto restored_before = capture_phase(router1, router2, server);
    auto restored_probe = probe.probe(4u, payload, client_link.now_ticks(), 20);
    if (!restored_probe
        || restored_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route delete smoke restored submit failed\n", 14);
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
        return fail("net lab route delete smoke restored stalled\n", 15);
    }

    const auto restored_result = probe.result();
    const auto restored_after = capture_phase(router1, router2, server);
    if (!restored_result.ready()
        || !restored_result.reached()
        || !restored_result.ok()
        || !restored_result.has_value()
        || restored_result.response_type != net::IcmpType::echo_reply
        || restored_result.response_code != 0u
        || restored_result.ttl != 4u
        || !same_ipv4(restored_result.responder, server_ip)
        || !bytes_eq(restored_result.value_payload(), payload)
        || probe.request_count() != 1u
        || probe.response_count() != 1u
        || probe.hop_count() != 0u
        || probe.reach_count() != 1u
        || probe.unreachable_count() != 0u
        || probe.timeout_count() != 0u
        || probe.error_count() != 0u
        || probe.transmitted_count() != 1u
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
        return fail("net lab route delete smoke restored mismatch\n", 16);
    }

    auto host_route = router1.set_gateway_route(
        server_ip,
        32u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    if (!host_route || router1.route_count() != 2u) {
        return fail("net lab route delete smoke add host route failed\n", 17);
    }

    if (!router1.remove_route(net::IpAddress::ipv4_any(), 0u)
        || router1.route_count() != 1u) {
        return fail("net lab route delete smoke remove default route failed\n", 18);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto host_only_before = capture_phase(router1, router2, server);
    auto host_only_probe = probe.probe(4u, payload, client_link.now_ticks(), 20);
    if (!host_only_probe
        || host_only_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route delete smoke host-only submit failed\n", 19);
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
        return fail("net lab route delete smoke host-only stalled\n", 20);
    }

    const auto host_only_result = probe.result();
    const auto host_only_after = capture_phase(router1, router2, server);
    if (!host_only_result.ready()
        || !host_only_result.reached()
        || !host_only_result.ok()
        || !host_only_result.has_value()
        || host_only_result.response_type != net::IcmpType::echo_reply
        || host_only_result.response_code != 0u
        || host_only_result.ttl != 4u
        || !same_ipv4(host_only_result.responder, server_ip)
        || !bytes_eq(host_only_result.value_payload(), payload)
        || probe.request_count() != 1u
        || probe.response_count() != 1u
        || probe.reach_count() != 1u
        || probe.unreachable_count() != 0u
        || probe.timeout_count() != 0u
        || !phase_delta_matches(host_only_before, host_only_after, 2u, 0u, 2u, 0u, 1u, 1u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route delete smoke host-only mismatch\n", 21);
    }

    probe.reset();
    probe.configure(unreachable_ip);
    const auto no_default_before = capture_phase(router1, router2, server);
    auto no_default_probe = probe.probe(4u, payload, client_link.now_ticks(), 20);
    if (!no_default_probe
        || no_default_probe.value().disposition != net::IcmpSendDisposition::queued
        || !probe.pending()) {
        return fail("net lab route delete smoke no-default submit failed\n", 22);
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
        return fail("net lab route delete smoke no-default stalled\n", 23);
    }

    const auto no_default_result = probe.result();
    const auto no_default_after = capture_phase(router1, router2, server);
    if (!no_default_result.ready()
        || !no_default_result.timed_out()
        || no_default_result.ok()
        || no_default_result.has_value()
        || no_default_result.ttl != 4u
        || probe.request_count() != 1u
        || probe.response_count() != 0u
        || probe.reach_count() != 0u
        || probe.unreachable_count() != 0u
        || probe.timeout_count() != 1u
        || probe.queued_count() != 1u
        || probe.transmitted_count() != 0u
        || !phase_delta_matches(no_default_before, no_default_after, 0u, 0u, 0u, 0u, 0u, 0u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route delete smoke no-default mismatch\n", 24);
    }

    if (!router1.remove_route(server_ip, 32u)
        || router1.route_count() != 0u) {
        return fail("net lab route delete smoke remove host route failed\n", 25);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto no_routes_before = capture_phase(router1, router2, server);
    auto no_routes_probe = probe.probe(4u, payload, client_link.now_ticks(), 20);
    if (!no_routes_probe
        || no_routes_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route delete smoke no-routes submit failed\n", 26);
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
        return fail("net lab route delete smoke no-routes stalled\n", 27);
    }

    const auto no_routes_result = probe.result();
    const auto no_routes_after = capture_phase(router1, router2, server);
    if (!no_routes_result.ready()
        || !no_routes_result.unreachable()
        || !no_routes_result.ok()
        || no_routes_result.has_value()
        || no_routes_result.response_type != net::IcmpType::destination_unreachable
        || no_routes_result.response_code != 0u
        || no_routes_result.ttl != 4u
        || !same_ipv4(no_routes_result.responder, router1_a_ip)
        || probe.request_count() != 1u
        || probe.response_count() != 1u
        || probe.reach_count() != 0u
        || probe.unreachable_count() != 1u
        || probe.timeout_count() != 0u
        || !phase_delta_matches(no_routes_before, no_routes_after, 0u, 1u, 0u, 0u, 0u, 0u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route delete smoke no-routes mismatch\n", 28);
    }

    auto readded_default_route = router1.set_gateway_route(
        net::IpAddress::ipv4_any(),
        0u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    if (!readded_default_route
        || router1.route_count() != 1u
        || router1.remove_route(net::IpAddress::ipv4(10, 0, 3, 1), 24u)) {
        return fail("net lab route delete smoke readd default route failed\n", 29);
    }

    probe.reset();
    probe.configure(server_ip);
    const auto readded_before = capture_phase(router1, router2, server);
    auto readded_probe = probe.probe(4u, payload, client_link.now_ticks(), 20);
    if (!readded_probe
        || readded_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab route delete smoke readded submit failed\n", 30);
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
        return fail("net lab route delete smoke readded stalled\n", 31);
    }

    const auto readded_result = probe.result();
    const auto readded_after = capture_phase(router1, router2, server);
    if (!readded_result.ready()
        || !readded_result.reached()
        || !readded_result.ok()
        || !readded_result.has_value()
        || readded_result.response_type != net::IcmpType::echo_reply
        || readded_result.response_code != 0u
        || readded_result.ttl != 4u
        || !same_ipv4(readded_result.responder, server_ip)
        || !bytes_eq(readded_result.value_payload(), payload)
        || probe.request_count() != 1u
        || probe.response_count() != 1u
        || probe.reach_count() != 1u
        || probe.unreachable_count() != 0u
        || probe.timeout_count() != 0u
        || !phase_delta_matches(readded_before, readded_after, 2u, 0u, 2u, 0u, 1u, 1u)
        || !system_idle(
            probe,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab route delete smoke readded mismatch\n", 32);
    }

    std::puts("net lab route delete smoke: ok");
    return 0;
}
