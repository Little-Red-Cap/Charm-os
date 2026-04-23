#include <cstdio>

import charm.net;
import net.protocol.diagnostic_udp;
import util.core;

namespace {
    using Link = net::lab::DuplexLink<192>;
    using HostPump = net::UdpStackPump<192, 4, 192, 4, 64, 4>;
    using HostNode = net::lab::StackNode<HostPump>;
    using ForwardHop = net::Ipv4ForwardingHop<192, 4, 192, 4>;

    struct PhaseCounters {
        util::usize router1_forwarded{0};
        util::usize router1_unreachable{0};
        util::usize router2_forwarded{0};
        util::usize router2_unreachable{0};
        util::usize server_requests{0};
        util::usize server_replies{0};
        util::usize server_error_replies{0};
        util::usize server_queued_replies{0};
    };

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
                                              const net::diag::udp::EndpointServer<16>& server) noexcept {
        return PhaseCounters{
            .router1_forwarded = router1.forwarded_count(),
            .router1_unreachable = router1.destination_unreachable_count(),
            .router2_forwarded = router2.forwarded_count(),
            .router2_unreachable = router2.destination_unreachable_count(),
            .server_requests = server.request_count(),
            .server_replies = server.reply_count(),
            .server_error_replies = server.error_reply_count(),
            .server_queued_replies = server.queued_reply_count(),
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
                                           util::usize server_error_replies = 0u,
                                           util::usize server_queued_replies = 0u) noexcept {
        return after.router1_forwarded - before.router1_forwarded == router1_forwarded
            && after.router1_unreachable - before.router1_unreachable == router1_unreachable
            && after.router2_forwarded - before.router2_forwarded == router2_forwarded
            && after.router2_unreachable - before.router2_unreachable == router2_unreachable
            && after.server_requests - before.server_requests == server_requests
            && after.server_replies - before.server_replies == server_replies
            && after.server_error_replies - before.server_error_replies == server_error_replies
            && after.server_queued_replies - before.server_queued_replies == server_queued_replies;
    }

    struct ClientState {
        util::u16 count_request_id{0};
        util::u16 timeout_request_id{0};
        bool got_count{false};
        bool got_timeout{false};
        bool failed{false};
        util::usize error_calls{0};
        net::errc last_error{net::errc::ok};

        static void on_count(void* ctx,
                             util::u16 request_id,
                             net::diag::udp::Status status,
                             const net::diag::CounterValue& response) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->got_count = request_id == self->count_request_id
                && status == net::diag::udp::Status::ok
                && response.value == 7u;
            if (!self->got_count) {
                self->failed = true;
            }
        }

        static void on_timeout(void* ctx, util::u16 request_id) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->got_timeout = request_id == self->timeout_request_id;
            if (!self->got_timeout) {
                self->failed = true;
            }
        }

        static void on_error(void* ctx, net::errc error) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            ++self->error_calls;
            self->last_error = error;
            self->failed = true;
        }
    };

    struct ServerState {
        bool saw_count{false};
        util::usize error_calls{0};
        net::errc last_error{net::errc::ok};

        static net::diag::udp::Status on_count(void* ctx,
                                               const net::EmptyMessage&,
                                               net::diag::CounterValue& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return net::diag::udp::Status::internal_error;
            }

            self->saw_count = true;
            response.value = 7u;
            return net::diag::udp::Status::ok;
        }

        static void on_error(void* ctx, net::errc error) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return;
            }

            ++self->error_calls;
            self->last_error = error;
        }
    };

    template <typename Client>
    [[nodiscard]] bool service_step(Client& client,
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
                         "udp diag forward default precedence client service error=%d\n",
                         static_cast<int>(client_progress.error()));
            return false;
        }

        auto router1_progress = router1.service(1);
        if (!router1_progress) {
            std::fprintf(stderr,
                         "udp diag forward default precedence router1 service error=%d\n",
                         static_cast<int>(router1_progress.error()));
            return false;
        }

        auto router2_progress = router2.service(1);
        if (!router2_progress) {
            std::fprintf(stderr,
                         "udp diag forward default precedence router2 service error=%d\n",
                         static_cast<int>(router2_progress.error()));
            return false;
        }

        auto server_progress = server_node.service(1);
        if (!server_progress) {
            std::fprintf(stderr,
                         "udp diag forward default precedence server service error=%d\n",
                         static_cast<int>(server_progress.error()));
            return false;
        }

        client.tick(client_link.now_ticks());
        return true;
    }

    [[nodiscard]] bool network_idle(const HostNode& client_node,
                                    const ForwardHop& router1,
                                    const ForwardHop& router2,
                                    const HostNode& server_node,
                                    const Link& client_link,
                                    const Link& middle_link,
                                    const Link& server_link) noexcept {
        return client_node.pump().pending_count() == 0u
            && router1.pending_count() == 0u
            && router2.pending_count() == 0u
            && server_node.pump().pending_count() == 0u
            && client_link.idle()
            && middle_link.idle()
            && server_link.idle();
    }

    template <typename Client>
    [[nodiscard]] bool drive_until_settled(Client& client,
                                           HostNode& client_node,
                                           ForwardHop& router1,
                                           ForwardHop& router2,
                                           HostNode& server_node,
                                           Link& client_link,
                                           Link& middle_link,
                                           Link& server_link,
                                           util::usize max_steps = 128) noexcept {
        for (util::usize step = 0; step < max_steps; ++step) {
            if (!service_step(
                    client,
                    client_node,
                    router1,
                    router2,
                    server_node,
                    client_link,
                    middle_link,
                    server_link)) {
                return false;
            }

            if (client.pending_count() == 0u
                && network_idle(
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
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x37u, 0x00u, 0x00u, 0x00u, 0x02u);
    constexpr auto router1_a_mac = net::MacAddress::from_bytes(0x02u, 0x37u, 0x00u, 0x00u, 0x00u, 0x11u);
    constexpr auto router1_b_mac = net::MacAddress::from_bytes(0x02u, 0x37u, 0x00u, 0x00u, 0x00u, 0x12u);
    constexpr auto router2_a_mac = net::MacAddress::from_bytes(0x02u, 0x37u, 0x00u, 0x00u, 0x00u, 0x21u);
    constexpr auto router2_b_mac = net::MacAddress::from_bytes(0x02u, 0x37u, 0x00u, 0x00u, 0x00u, 0x22u);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0x37u, 0x00u, 0x00u, 0x00u, 0x09u);

    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto router1_a_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto router1_b_ip = net::IpAddress::ipv4(10, 0, 1, 1);
    constexpr auto router2_a_ip = net::IpAddress::ipv4(10, 0, 1, 2);
    constexpr auto router2_b_ip = net::IpAddress::ipv4(10, 0, 2, 1);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 2, 9);
    constexpr auto default_network = net::IpAddress::ipv4_any();
    constexpr auto specific_network = net::IpAddress::ipv4(10, 0, 2, 0);

    constexpr auto client_local = net::Endpoint::ipv4_any(9001);
    constexpr auto server_local = net::Endpoint::ipv4_any(7001);
    constexpr auto server_peer = net::Endpoint::ipv4(10, 0, 2, 9, 7001);

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
        net::UdpStackPumpConfig{
            .egress = net::UdpEgressPumpConfig{
                .retry_interval_ticks = 4,
                .max_attempts = 4,
            }
        });
    if (!client_init || !client_node.ready()) {
        return fail("net lab udp diag forward default precedence smoke client init failed\n", 1);
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
        return fail("net lab udp diag forward default precedence smoke router1 init failed\n", 2);
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
    auto router2_client_route = router2.set_gateway_route(
        net::IpAddress::ipv4(10, 0, 0, 0),
        24u,
        net::Ipv4ForwardingPort::a,
        router1_b_ip);
    if (!router2_a_init
        || !router2_b_init
        || !router2_prefix_a
        || !router2_prefix_b
        || !router2_client_route
        || router2.route_count() != 1u
        || !router2.ready()) {
        return fail("net lab udp diag forward default precedence smoke router2 init failed\n", 3);
    }

    HostNode server_node{};
    auto server_init = server_node.init(
        server_link.endpoint_b(),
        server_mac,
        server_ip,
        net::UdpStackPumpConfig{
            .egress = net::UdpEgressPumpConfig{
                .retry_interval_ticks = 4,
                .max_attempts = 4,
            }
        });
    if (!server_init || !server_node.ready()) {
        return fail("net lab udp diag forward default precedence smoke server init failed\n", 4);
    }

    net::diag::udp::EndpointClient<16, 4> client{client_local, server_peer};
    ClientState client_state{};
    client.set_error_handler(&ClientState::on_error, &client_state);

    net::diag::udp::EndpointServer<16> server{server_local};
    ServerState server_state{};
    server.set_error_handler(&ServerState::on_error, &server_state);

    auto bound_client = client.bind(client_node.pump());
    auto bound_server = server.bind(server_node.pump());
    auto registered_count = server.on_count(&ServerState::on_count, &server_state);
    if (!bound_client
        || !bound_server
        || !registered_count
        || !client_node.pump().has_udp_binding(client.local_endpoint().port)
        || !server_node.pump().has_udp_binding(server.local_endpoint().port)
        || client_node.pump().udp_binding_count() != 1u
        || server_node.pump().udp_binding_count() != 1u) {
        return fail("net lab udp diag forward default precedence smoke bind failed\n", 5);
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
        return fail("net lab udp diag forward default precedence smoke default decision mismatch\n", 6);
    }

    client.reset();
    server.reset();
    client_state.count_request_id = 0u;
    client_state.timeout_request_id = 0u;
    client_state.got_count = false;
    client_state.got_timeout = false;
    client_state.failed = false;
    server_state.saw_count = false;
    const auto default_before = capture_phase(router1, router2, server);
    auto default_count = client.query_count(
        client_link.now_ticks(),
        20,
        &ClientState::on_count,
        &ClientState::on_timeout,
        &client_state);
    if (!default_count
        || client.pending_count() != 1u
        || client.request_count() != 1u
        || client.queued_count() != 1u) {
        return fail("net lab udp diag forward default precedence smoke default send failed\n", 7);
    }
    client_state.count_request_id = default_count.value();
    client_state.timeout_request_id = default_count.value();

    if (!drive_until_settled(
            client,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab udp diag forward default precedence smoke default stalled\n", 8);
    }

    const auto default_after = capture_phase(router1, router2, server);
    const auto client_target = client_node.pump().arp().table().lookup(server_ip);
    const auto router1_gateway = router1.arp_b().table().lookup(router2_a_ip);
    const auto router2_gateway = router2.arp_a().table().lookup(router1_b_ip);
    const auto server_target = server_node.pump().arp().table().lookup(client_ip);
    if (!client_state.got_count
        || client_state.got_timeout
        || !server_state.saw_count
        || client.pending_count() != 0u
        || client.response_count() != 1u
        || client.timeout_count() != 0u
        || client.drop_count() != 0u
        || client.queued_count() != 1u
        || client.last_error() != net::errc::ok
        || server.request_count() != 1u
        || server.reply_count() != 1u
        || server.error_reply_count() != 0u
        || server.queued_reply_count() != 1u
        || server.last_error() != net::errc::ok
        || !client_target
        || !same_mac(client_target.value(), router1_a_mac)
        || !router1_gateway
        || !same_mac(router1_gateway.value(), router2_a_mac)
        || !router2_gateway
        || !same_mac(router2_gateway.value(), router1_b_mac)
        || !server_target
        || !same_mac(server_target.value(), router2_b_mac)
        || !phase_delta_matches(default_before, default_after, 2u, 0u, 2u, 0u, 1u, 1u, 0u, 1u)
        || client_state.failed
        || client_state.error_calls != 0u
        || server_state.error_calls != 0u) {
        return fail("net lab udp diag forward default precedence smoke default mismatch\n", 9);
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
        return fail("net lab udp diag forward default precedence smoke bad route decision mismatch\n", 10);
    }

    client.reset();
    server.reset();
    client_state.count_request_id = 0u;
    client_state.timeout_request_id = 0u;
    client_state.got_count = false;
    client_state.got_timeout = false;
    client_state.failed = false;
    server_state.saw_count = false;
    const auto bad_before = capture_phase(router1, router2, server);
    auto bad_count = client.query_count(
        client_link.now_ticks(),
        8,
        &ClientState::on_count,
        &ClientState::on_timeout,
        &client_state);
    if (!bad_count
        || client.pending_count() != 1u
        || client.request_count() != 1u
        || client.queued_count() != 0u) {
        return fail("net lab udp diag forward default precedence smoke bad route send failed\n", 11);
    }
    client_state.count_request_id = bad_count.value();
    client_state.timeout_request_id = bad_count.value();

    if (!drive_until_settled(
            client,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link,
            48)) {
        return fail("net lab udp diag forward default precedence smoke bad route stalled\n", 12);
    }

    const auto bad_after = capture_phase(router1, router2, server);
    if (client_state.got_count
        || !client_state.got_timeout
        || server_state.saw_count
        || client.pending_count() != 0u
        || client.response_count() != 0u
        || client.timeout_count() != 1u
        || client.drop_count() != 0u
        || client.queued_count() != 0u
        || client.last_error() != net::errc::ok
        || server.request_count() != 0u
        || server.reply_count() != 0u
        || server.error_reply_count() != 0u
        || server.queued_reply_count() != 0u
        || server.last_error() != net::errc::ok
        || !phase_delta_matches(bad_before, bad_after, 1u, 0u, 0u, 0u, 0u, 0u)
        || client_state.failed
        || client_state.error_calls != 0u
        || server_state.error_calls != 0u) {
        return fail("net lab udp diag forward default precedence smoke bad route mismatch\n", 13);
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
        return fail("net lab udp diag forward default precedence smoke host route decision mismatch\n", 14);
    }

    client.reset();
    server.reset();
    client_state.count_request_id = 0u;
    client_state.timeout_request_id = 0u;
    client_state.got_count = false;
    client_state.got_timeout = false;
    client_state.failed = false;
    server_state.saw_count = false;
    const auto host_before = capture_phase(router1, router2, server);
    auto host_count = client.query_count(
        client_link.now_ticks(),
        20,
        &ClientState::on_count,
        &ClientState::on_timeout,
        &client_state);
    if (!host_count
        || client.pending_count() != 1u
        || client.request_count() != 1u
        || client.queued_count() != 0u) {
        return fail("net lab udp diag forward default precedence smoke host route send failed\n", 15);
    }
    client_state.count_request_id = host_count.value();
    client_state.timeout_request_id = host_count.value();

    if (!drive_until_settled(
            client,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab udp diag forward default precedence smoke host route stalled\n", 16);
    }

    const auto host_after = capture_phase(router1, router2, server);
    if (!client_state.got_count
        || client_state.got_timeout
        || !server_state.saw_count
        || client.pending_count() != 0u
        || client.response_count() != 1u
        || client.timeout_count() != 0u
        || client.drop_count() != 0u
        || client.queued_count() != 0u
        || client.last_error() != net::errc::ok
        || server.request_count() != 1u
        || server.reply_count() != 1u
        || server.error_reply_count() != 0u
        || server.queued_reply_count() != 0u
        || server.last_error() != net::errc::ok
        || !phase_delta_matches(host_before, host_after, 2u, 0u, 2u, 0u, 1u, 1u)
        || client_state.failed
        || client_state.error_calls != 0u
        || server_state.error_calls != 0u) {
        return fail("net lab udp diag forward default precedence smoke host route mismatch\n", 17);
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
        return fail("net lab udp diag forward default precedence smoke remove host route failed\n", 18);
    }

    client.reset();
    server.reset();
    client_state.count_request_id = 0u;
    client_state.timeout_request_id = 0u;
    client_state.got_count = false;
    client_state.got_timeout = false;
    client_state.failed = false;
    server_state.saw_count = false;
    const auto fallback_before = capture_phase(router1, router2, server);
    auto fallback_count = client.query_count(
        client_link.now_ticks(),
        8,
        &ClientState::on_count,
        &ClientState::on_timeout,
        &client_state);
    if (!fallback_count
        || client.pending_count() != 1u
        || client.request_count() != 1u
        || client.queued_count() != 0u) {
        return fail("net lab udp diag forward default precedence smoke fallback send failed\n", 19);
    }
    client_state.count_request_id = fallback_count.value();
    client_state.timeout_request_id = fallback_count.value();

    if (!drive_until_settled(
            client,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link,
            48)) {
        return fail("net lab udp diag forward default precedence smoke fallback stalled\n", 20);
    }

    const auto fallback_after = capture_phase(router1, router2, server);
    if (client_state.got_count
        || !client_state.got_timeout
        || server_state.saw_count
        || client.pending_count() != 0u
        || client.response_count() != 0u
        || client.timeout_count() != 1u
        || client.drop_count() != 0u
        || client.queued_count() != 0u
        || client.last_error() != net::errc::ok
        || server.request_count() != 0u
        || server.reply_count() != 0u
        || server.error_reply_count() != 0u
        || server.queued_reply_count() != 0u
        || server.last_error() != net::errc::ok
        || !phase_delta_matches(fallback_before, fallback_after, 1u, 0u, 0u, 0u, 0u, 0u)
        || client_state.failed
        || client_state.error_calls != 0u
        || server_state.error_calls != 0u) {
        return fail("net lab udp diag forward default precedence smoke fallback mismatch\n", 21);
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
        return fail("net lab udp diag forward default precedence smoke remove specific route failed\n", 22);
    }

    client.reset();
    server.reset();
    client_state.count_request_id = 0u;
    client_state.timeout_request_id = 0u;
    client_state.got_count = false;
    client_state.got_timeout = false;
    client_state.failed = false;
    server_state.saw_count = false;
    const auto restored_before = capture_phase(router1, router2, server);
    auto restored_count = client.query_count(
        client_link.now_ticks(),
        20,
        &ClientState::on_count,
        &ClientState::on_timeout,
        &client_state);
    if (!restored_count
        || client.pending_count() != 1u
        || client.request_count() != 1u
        || client.queued_count() != 0u) {
        return fail("net lab udp diag forward default precedence smoke restored send failed\n", 23);
    }
    client_state.count_request_id = restored_count.value();
    client_state.timeout_request_id = restored_count.value();

    if (!drive_until_settled(
            client,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab udp diag forward default precedence smoke restored stalled\n", 24);
    }

    const auto restored_after = capture_phase(router1, router2, server);
    if (!client_state.got_count
        || client_state.got_timeout
        || !server_state.saw_count
        || client.pending_count() != 0u
        || client.response_count() != 1u
        || client.timeout_count() != 0u
        || client.drop_count() != 0u
        || client.queued_count() != 0u
        || client.last_error() != net::errc::ok
        || server.request_count() != 1u
        || server.reply_count() != 1u
        || server.error_reply_count() != 0u
        || server.queued_reply_count() != 0u
        || server.last_error() != net::errc::ok
        || !phase_delta_matches(restored_before, restored_after, 2u, 0u, 2u, 0u, 1u, 1u)
        || client_state.failed
        || client_state.error_calls != 0u
        || server_state.error_calls != 0u
        || !network_idle(
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab udp diag forward default precedence smoke restored mismatch\n", 25);
    }

    std::puts("net lab udp diag forward default precedence smoke: ok");
    return 0;
}
