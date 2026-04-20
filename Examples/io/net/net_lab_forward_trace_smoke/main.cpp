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

    [[nodiscard]] bool same_ipv4(const net::IpAddress& lhs,
                                 const net::IpAddress& rhs) noexcept {
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

    [[nodiscard]] bool same_mac(const net::MacAddress& lhs,
                                const net::MacAddress& rhs) noexcept {
        for (util::usize i = 0; i < lhs.bytes.size(); ++i) {
            if (lhs.bytes[i] != rhs.bytes[i]) {
                return false;
            }
        }
        return true;
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
            std::fprintf(stderr, "forward trace client service error=%d\n", static_cast<int>(client_progress.error()));
            return false;
        }

        auto router1_progress = router1.service(1);
        if (!router1_progress) {
            std::fprintf(stderr, "forward trace router1 service error=%d\n", static_cast<int>(router1_progress.error()));
            return false;
        }

        auto router2_progress = router2.service(1);
        if (!router2_progress) {
            std::fprintf(stderr, "forward trace router2 service error=%d\n", static_cast<int>(router2_progress.error()));
            return false;
        }

        auto server_progress = server_node.service(1);
        if (!server_progress) {
            std::fprintf(stderr, "forward trace server service error=%d\n", static_cast<int>(server_progress.error()));
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
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x10u, 0x00u, 0x00u, 0x00u, 0x02u);
    constexpr auto router1_a_mac = net::MacAddress::from_bytes(0x02u, 0x10u, 0x00u, 0x00u, 0x00u, 0x11u);
    constexpr auto router1_b_mac = net::MacAddress::from_bytes(0x02u, 0x10u, 0x00u, 0x00u, 0x00u, 0x12u);
    constexpr auto router2_a_mac = net::MacAddress::from_bytes(0x02u, 0x10u, 0x00u, 0x00u, 0x00u, 0x21u);
    constexpr auto router2_b_mac = net::MacAddress::from_bytes(0x02u, 0x10u, 0x00u, 0x00u, 0x00u, 0x22u);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0x10u, 0x00u, 0x00u, 0x00u, 0x09u);

    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto router1_a_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto router1_b_ip = net::IpAddress::ipv4(10, 0, 1, 1);
    constexpr auto router2_a_ip = net::IpAddress::ipv4(10, 0, 1, 2);
    constexpr auto router2_b_ip = net::IpAddress::ipv4(10, 0, 2, 1);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 2, 9);
    constexpr auto unreachable_ip = net::IpAddress::ipv4(10, 0, 3, 9);

    static constexpr util::u8 payload[]{'h', 'o', 'p'};

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
        return fail("net lab forward trace smoke client init failed\n", 1);
    }

    ForwardHop router1{};
    router1.configure(net::Ipv4ForwardingHopConfig{
        .retry_interval_ticks = 4,
        .max_attempts = 4,
        .icmp_ttl = 64,
    });
    auto router1_a_init = router1.init_port_a(client_link.endpoint_b(), router1_a_mac, router1_a_ip);
    auto router1_b_init = router1.init_port_b(middle_link.endpoint_a(), router1_b_mac, router1_b_ip);
    auto router1_client_route = router1.add_direct_route(
        net::IpAddress::ipv4(10, 0, 0, 0),
        24u,
        net::Ipv4ForwardingPort::a);
    auto router1_server_route = router1.add_gateway_route(
        net::IpAddress::ipv4(10, 0, 2, 0),
        24u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    auto router1_unreachable_route = router1.add_gateway_route(
        net::IpAddress::ipv4(10, 0, 3, 0),
        24u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    if (!router1_a_init
        || !router1_b_init
        || !router1_client_route
        || !router1_server_route
        || !router1_unreachable_route
        || !router1.ready()) {
        return fail("net lab forward trace smoke router1 init failed\n", 2);
    }

    ForwardHop router2{};
    router2.configure(net::Ipv4ForwardingHopConfig{
        .retry_interval_ticks = 4,
        .max_attempts = 4,
        .icmp_ttl = 64,
    });
    auto router2_a_init = router2.init_port_a(middle_link.endpoint_b(), router2_a_mac, router2_a_ip);
    auto router2_b_init = router2.init_port_b(server_link.endpoint_a(), router2_b_mac, router2_b_ip);
    auto router2_client_route = router2.add_gateway_route(
        net::IpAddress::ipv4(10, 0, 0, 0),
        24u,
        net::Ipv4ForwardingPort::a,
        router1_b_ip);
    auto router2_server_route = router2.add_direct_route(
        net::IpAddress::ipv4(10, 0, 2, 0),
        24u,
        net::Ipv4ForwardingPort::b);
    if (!router2_a_init
        || !router2_b_init
        || !router2_client_route
        || !router2_server_route
        || !router2.ready()) {
        return fail("net lab forward trace smoke router2 init failed\n", 3);
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
        return fail("net lab forward trace smoke server init failed\n", 4);
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
        return fail("net lab forward trace smoke protocol bind failed\n", 5);
    }

    auto first_probe = probe.probe(1u, payload, client_link.now_ticks(), 20);
    if (!first_probe
        || first_probe.value().disposition != net::IcmpSendDisposition::queued
        || !probe.pending()) {
        return fail("net lab forward trace smoke ttl1 submit failed\n", 6);
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
        return fail("net lab forward trace smoke ttl1 stalled\n", 7);
    }

    const auto first_result = probe.result();
    const auto client_target = client_node.pump().arp().table().lookup(server_ip);
    if (!first_result.ready()
        || !first_result.hop()
        || !first_result.ok()
        || first_result.has_value()
        || first_result.response_type != net::IcmpType::time_exceeded
        || first_result.response_code != 0u
        || first_result.ttl != 1u
        || !same_ipv4(first_result.responder, router1_a_ip)
        || probe.request_count() != 1
        || probe.response_count() != 1
        || probe.hop_count() != 1
        || probe.reach_count() != 0
        || probe.timeout_count() != 0
        || probe.error_count() != 0
        || probe.queued_count() != 1
        || probe.transmitted_count() != 0
        || !client_target
        || !same_mac(client_target.value(), router1_a_mac)
        || router1.ttl_expired_count() != 1
        || router1.destination_unreachable_count() != 0
        || router1.forwarded_count() != 0
        || router1.proxy_arp_reply_count() != 1
        || router2.ttl_expired_count() != 0
        || router2.destination_unreachable_count() != 0
        || router2.forwarded_count() != 0
        || router2.proxy_arp_reply_count() != 0
        || server.request_count() != 0
        || !client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()) {
        return fail("net lab forward trace smoke ttl1 mismatch\n", 8);
    }

    auto second_probe = probe.probe(2u, payload, client_link.now_ticks(), 20);
    if (!second_probe
        || second_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab forward trace smoke ttl2 submit failed\n", 9);
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
        return fail("net lab forward trace smoke ttl2 stalled\n", 10);
    }

    const auto second_result = probe.result();
    const auto middle_target = router1.arp_b().table().lookup(router2_a_ip);
    const auto middle_client = router2.arp_a().table().lookup(client_ip);
    if (!second_result.ready()
        || !second_result.hop()
        || !second_result.ok()
        || second_result.has_value()
        || second_result.response_type != net::IcmpType::time_exceeded
        || second_result.response_code != 0u
        || second_result.ttl != 2u
        || !same_ipv4(second_result.responder, router2_a_ip)
        || probe.request_count() != 2
        || probe.response_count() != 2
        || probe.hop_count() != 2
        || probe.reach_count() != 0
        || probe.timeout_count() != 0
        || probe.error_count() != 0
        || probe.queued_count() != 1
        || probe.transmitted_count() != 1
        || !middle_target
        || !same_mac(middle_target.value(), router2_a_mac)
        || !middle_client
        || !same_mac(middle_client.value(), router1_b_mac)
        || router1.ttl_expired_count() != 1
        || router1.destination_unreachable_count() != 0
        || router1.forwarded_count() != 2
        || router1.proxy_arp_reply_count() != 1
        || router2.ttl_expired_count() != 1
        || router2.destination_unreachable_count() != 0
        || router2.forwarded_count() != 0
        || router2.proxy_arp_reply_count() != 0
        || server.request_count() != 0
        || !client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()) {
        return fail("net lab forward trace smoke ttl2 mismatch\n", 11);
    }

    auto third_probe = probe.probe(3u, payload, client_link.now_ticks(), 20);
    if (!third_probe
        || third_probe.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.pending()) {
        return fail("net lab forward trace smoke ttl3 submit failed\n", 12);
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
        return fail("net lab forward trace smoke ttl3 stalled\n", 13);
    }

    const auto third_result = probe.result();
    const auto server_client = server_node.pump().arp().table().lookup(client_ip);
    if (!third_result.ready()
        || !third_result.reached()
        || !third_result.ok()
        || !third_result.has_value()
        || third_result.response_type != net::IcmpType::echo_reply
        || third_result.response_code != 0u
        || third_result.ttl != 3u
        || !same_ipv4(third_result.responder, server_ip)
        || !bytes_eq(third_result.value_payload(), payload)
        || probe.request_count() != 3
        || probe.response_count() != 3
        || probe.hop_count() != 2
        || probe.reach_count() != 1
        || probe.timeout_count() != 0
        || probe.error_count() != 0
        || probe.queued_count() != 1
        || probe.transmitted_count() != 2
        || !server_client
        || !same_mac(server_client.value(), router2_b_mac)
        || router1.ttl_expired_count() != 1
        || router1.destination_unreachable_count() != 0
        || router1.forwarded_count() != 4
        || router1.proxy_arp_reply_count() != 1
        || router2.ttl_expired_count() != 1
        || router2.destination_unreachable_count() != 0
        || router2.forwarded_count() != 2
        || router2.proxy_arp_reply_count() != 1
        || server.request_count() != 1
        || server.reply_count() != 1
        || server.drop_count() != 0
        || !client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()
        || router1.pending_count() != 0
        || router2.pending_count() != 0
        || client_node.pump().pending_count() != 0
        || server_node.pump().pending_count() != 0) {
        return fail("net lab forward trace smoke ttl3 mismatch\n", 14);
    }

    probe.configure(unreachable_ip);
    auto fourth_probe = probe.probe(4u, payload, client_link.now_ticks(), 20);
    if (!fourth_probe
        || fourth_probe.value().disposition != net::IcmpSendDisposition::queued
        || !probe.pending()) {
        return fail("net lab forward trace smoke no-route submit failed\n", 15);
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
        return fail("net lab forward trace smoke no-route stalled\n", 16);
    }

    const auto fourth_result = probe.result();
    const auto unreachable_target = client_node.pump().arp().table().lookup(unreachable_ip);
    if (!fourth_result.ready()
        || !fourth_result.unreachable()
        || !fourth_result.ok()
        || fourth_result.has_value()
        || fourth_result.response_type != net::IcmpType::destination_unreachable
        || fourth_result.response_code != 0u
        || fourth_result.ttl != 4u
        || !same_ipv4(fourth_result.responder, router2_a_ip)
        || probe.request_count() != 4
        || probe.response_count() != 4
        || probe.hop_count() != 2
        || probe.reach_count() != 1
        || probe.unreachable_count() != 1
        || probe.timeout_count() != 0
        || probe.error_count() != 0
        || probe.queued_count() != 2
        || probe.transmitted_count() != 2
        || !unreachable_target
        || !same_mac(unreachable_target.value(), router1_a_mac)
        || router1.ttl_expired_count() != 1
        || router1.destination_unreachable_count() != 0
        || router1.forwarded_count() != 6
        || router1.proxy_arp_reply_count() != 2
        || router2.ttl_expired_count() != 1
        || router2.destination_unreachable_count() != 1
        || router2.forwarded_count() != 2
        || router2.proxy_arp_reply_count() != 1
        || server.request_count() != 1
        || server.reply_count() != 1
        || server.drop_count() != 0
        || !client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()
        || router1.pending_count() != 0
        || router2.pending_count() != 0
        || client_node.pump().pending_count() != 0
        || server_node.pump().pending_count() != 0) {
        return fail("net lab forward trace smoke no-route mismatch\n", 17);
    }

    std::puts("net lab forward trace smoke: ok");
    return 0;
}
