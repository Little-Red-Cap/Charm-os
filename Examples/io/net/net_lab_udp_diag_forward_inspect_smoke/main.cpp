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

    [[nodiscard]] bool route_matches_decision(const net::Ipv4ForwardingRoute& route,
                                              const net::Ipv4ForwardingDecisionSnapshot& decision) noexcept {
        return same_route(
            route,
            decision.network,
            decision.prefix_length,
            decision.egress_port,
            decision.has_next_hop,
            decision.next_hop,
            decision.metric);
    }

    [[nodiscard]] bool same_request(const net::diag::ForwardInspectRequest& lhs,
                                    const net::diag::ForwardInspectRequest& rhs) noexcept {
        if (lhs.ingress_port != rhs.ingress_port || lhs.ttl != rhs.ttl) {
            return false;
        }
        for (util::usize index = 0; index < lhs.destination.size(); ++index) {
            if (lhs.destination[index] != rhs.destination[index]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool same_reply(const net::diag::ForwardInspectReply& reply,
                                  net::Ipv4ForwardingDisposition disposition,
                                  net::Ipv4ForwardingReason reason,
                                  bool has_egress,
                                  net::Ipv4ForwardingPort egress_port,
                                  bool routing_configured,
                                  bool has_decision,
                                  const net::IpAddress& network,
                                  util::u8 prefix_length,
                                  bool has_next_hop,
                                  const net::IpAddress& next_hop,
                                  util::u16 metric,
                                  bool from_connected_prefix,
                                  util::usize route_count,
                                  bool has_selected_route,
                                  util::u8 selected_route_index,
                                  const net::IpAddress& selected_network,
                                  util::u8 selected_prefix_length,
                                  net::Ipv4ForwardingPort selected_egress_port,
                                  bool selected_has_next_hop,
                                  const net::IpAddress& selected_next_hop,
                                  util::u16 selected_metric) noexcept {
        if (reply.disposition != disposition
            || reply.reason != reason
            || reply.has_egress_port() != has_egress
            || reply.routing_is_configured() != routing_configured
            || reply.has_decision_snapshot() != has_decision
            || reply.route_total() != route_count
            || reply.has_selected_route_entry() != has_selected_route) {
            return false;
        }

        if (has_egress && reply.egress_port != egress_port) {
            return false;
        }

        if (has_decision
            && !same_decision(
                reply.decision(),
                network,
                prefix_length,
                egress_port,
                has_next_hop,
                next_hop,
                metric,
                from_connected_prefix)) {
            return false;
        }

        if (!has_selected_route) {
            return true;
        }

        if (reply.selected_route_index != selected_route_index) {
            return false;
        }

        return same_route(
            reply.selected_route(),
            selected_network,
            selected_prefix_length,
            selected_egress_port,
            selected_has_next_hop,
            selected_next_hop,
            selected_metric);
    }

    [[nodiscard]] PhaseCounters capture_phase(const ForwardHop& router1,
                                              const ForwardHop& router2,
                                              const net::diag::udp::EndpointServer<64>& server) noexcept {
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
        util::u16 request_id{0};
        bool got_reply{false};
        bool got_timeout{false};
        bool failed{false};
        util::usize error_calls{0};
        net::errc last_error{net::errc::ok};
        net::diag::ForwardInspectReply reply{};

        void reset_round() noexcept {
            request_id = 0u;
            got_reply = false;
            got_timeout = false;
            failed = false;
            error_calls = 0u;
            last_error = net::errc::ok;
            reply = {};
        }

        static void on_forward_inspect(void* ctx,
                                       util::u16 request_id,
                                       net::diag::udp::Status status,
                                       const net::diag::ForwardInspectReply& response) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->got_reply = request_id == self->request_id
                && status == net::diag::udp::Status::ok;
            self->reply = response;
            if (!self->got_reply) {
                self->failed = true;
            }
        }

        static void on_timeout(void* ctx, util::u16 request_id) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->got_timeout = request_id == self->request_id;
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
        ForwardHop* router{nullptr};
        bool saw_inspect{false};
        util::usize inspect_calls{0};
        util::usize error_calls{0};
        net::errc last_error{net::errc::ok};
        net::diag::ForwardInspectRequest last_request{};

        void reset_round() noexcept {
            saw_inspect = false;
            error_calls = 0u;
            last_error = net::errc::ok;
            last_request = {};
        }

        static net::diag::udp::Status on_forward_inspect(
            void* ctx,
            const net::diag::ForwardInspectRequest& request,
            net::diag::ForwardInspectReply& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self || self->router == nullptr) {
                return net::diag::udp::Status::internal_error;
            }

            self->saw_inspect = true;
            ++self->inspect_calls;
            self->last_request = request;

            const auto snapshot = self->router->inspect_forwarding_explanation(
                request.ingress_port,
                request.destination_address(),
                request.ttl);
            if (!snapshot.has_value()) {
                return net::diag::udp::Status::bad_request;
            }

            bool has_selected_route = false;
            util::u8 selected_route_index = 0u;
            net::Ipv4ForwardingRoute selected_route{};
            if (snapshot.value().uses_explicit_route()) {
                for (util::usize index = 0; index < self->router->route_count(); ++index) {
                    auto route = self->router->route_at(index);
                    if (!route.has_value()) {
                        continue;
                    }
                    if (!route_matches_decision(route.value(), snapshot.value().decision)) {
                        continue;
                    }
                    has_selected_route = true;
                    selected_route_index = static_cast<util::u8>(index);
                    selected_route = route.value();
                    break;
                }
            }

            response = net::diag::ForwardInspectReply::from_snapshot(
                snapshot.value(),
                static_cast<util::u8>(self->router->route_count()),
                has_selected_route,
                selected_route_index,
                selected_route);
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
                         "udp diag forward inspect client service error=%d\n",
                         static_cast<int>(client_progress.error()));
            return false;
        }

        auto router1_progress = router1.service(1);
        if (!router1_progress) {
            std::fprintf(stderr,
                         "udp diag forward inspect router1 service error=%d\n",
                         static_cast<int>(router1_progress.error()));
            return false;
        }

        auto router2_progress = router2.service(1);
        if (!router2_progress) {
            std::fprintf(stderr,
                         "udp diag forward inspect router2 service error=%d\n",
                         static_cast<int>(router2_progress.error()));
            return false;
        }

        auto server_progress = server_node.service(1);
        if (!server_progress) {
            std::fprintf(stderr,
                         "udp diag forward inspect server service error=%d\n",
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
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x3Eu, 0x00u, 0x00u, 0x00u, 0x02u);
    constexpr auto router1_a_mac = net::MacAddress::from_bytes(0x02u, 0x3Eu, 0x00u, 0x00u, 0x00u, 0x11u);
    constexpr auto router1_b_mac = net::MacAddress::from_bytes(0x02u, 0x3Eu, 0x00u, 0x00u, 0x00u, 0x12u);
    constexpr auto router2_a_mac = net::MacAddress::from_bytes(0x02u, 0x3Eu, 0x00u, 0x00u, 0x00u, 0x21u);
    constexpr auto router2_b_mac = net::MacAddress::from_bytes(0x02u, 0x3Eu, 0x00u, 0x00u, 0x00u, 0x22u);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0x3Eu, 0x00u, 0x00u, 0x00u, 0x09u);

    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto router1_a_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto router1_b_ip = net::IpAddress::ipv4(10, 0, 1, 1);
    constexpr auto router2_a_ip = net::IpAddress::ipv4(10, 0, 1, 2);
    constexpr auto router2_b_ip = net::IpAddress::ipv4(10, 0, 2, 1);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 2, 9);
    constexpr auto server_network = net::IpAddress::ipv4(10, 0, 2, 0);
    constexpr auto inspect_ip = net::IpAddress::ipv4(10, 0, 9, 77);
    constexpr auto inspect_network = net::IpAddress::ipv4(10, 0, 9, 0);
    constexpr auto default_network = net::IpAddress::ipv4_any();

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
        return fail("net lab udp diag forward inspect smoke client init failed\n", 1);
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
    auto router1_server_route = router1.add_gateway_route(
        server_network,
        24u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    auto router1_default_route = router1.add_gateway_route(
        default_network,
        0u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip,
        9u);
    auto router1_bad_specific_route = router1.add_direct_route(
        inspect_network,
        24u,
        net::Ipv4ForwardingPort::a,
        7u);
    auto router1_host_route = router1.add_gateway_route(
        inspect_ip,
        32u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip,
        1u);
    if (!router1_a_init
        || !router1_b_init
        || !router1_prefix_a
        || !router1_prefix_b
        || !router1_server_route
        || !router1_default_route
        || !router1_bad_specific_route
        || !router1_host_route
        || router1.route_count() != 4u
        || !router1.ready()) {
        return fail("net lab udp diag forward inspect smoke router1 init failed\n", 2);
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
    auto router2_client_route = router2.add_gateway_route(
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
        return fail("net lab udp diag forward inspect smoke router2 init failed\n", 3);
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
        return fail("net lab udp diag forward inspect smoke server init failed\n", 4);
    }

    net::diag::udp::EndpointClient<64, 4> client{client_local, server_peer};
    ClientState client_state{};
    client.set_error_handler(&ClientState::on_error, &client_state);

    net::diag::udp::EndpointServer<64> server{server_local};
    ServerState server_state{.router = &router1};
    server.set_error_handler(&ServerState::on_error, &server_state);

    auto bound_client = client.bind(client_node.pump());
    auto bound_server = server.bind(server_node.pump());
    auto registered_forward_inspect = server.on_forward_inspect(
        &ServerState::on_forward_inspect,
        &server_state);
    if (!bound_client
        || !bound_server
        || !registered_forward_inspect
        || !server.has_forward_inspect()
        || !client_node.pump().has_udp_binding(client.local_endpoint().port)
        || !server_node.pump().has_udp_binding(server.local_endpoint().port)
        || client_node.pump().udp_binding_count() != 1u
        || server_node.pump().udp_binding_count() != 1u) {
        return fail("net lab udp diag forward inspect smoke bind failed\n", 5);
    }

    auto run_query = [&](const char* send_fail,
                         const char* stall_fail,
                         const char* verify_fail,
                         const net::diag::ForwardInspectRequest& request,
                         util::u32 timeout_ms,
                         util::usize expected_client_queued,
                         util::usize expected_server_queued_replies,
                         auto&& verify_reply) -> bool {
        client.reset();
        server.reset();
        client_state.reset_round();
        server_state.reset_round();

        const auto inspect_calls_before = server_state.inspect_calls;
        const auto before = capture_phase(router1, router2, server);
        auto sent = client.query_forward_inspect(
            request,
            client_link.now_ticks(),
            timeout_ms,
            &ClientState::on_forward_inspect,
            &ClientState::on_timeout,
            &client_state);
        if (!sent
            || client.pending_count() != 1u
            || client.request_count() != 1u
            || client.queued_count() != expected_client_queued) {
            std::fputs(send_fail, stderr);
            return false;
        }
        client_state.request_id = sent.value();

        if (!drive_until_settled(
                client,
                client_node,
                router1,
                router2,
                server_node,
                client_link,
                middle_link,
                server_link)) {
            std::fputs(stall_fail, stderr);
            return false;
        }

        const auto after = capture_phase(router1, router2, server);
        const auto reply_ok = verify_reply(client_state.reply);
        if (!client_state.got_reply
            || client_state.got_timeout
            || !server_state.saw_inspect
            || server_state.inspect_calls != (inspect_calls_before + 1u)
            || !same_request(server_state.last_request, request)
            || client.pending_count() != 0u
            || client.response_count() != 1u
            || client.timeout_count() != 0u
            || client.drop_count() != 0u
            || client.last_error() != net::errc::ok
            || server.request_count() != 1u
            || server.reply_count() != 1u
            || server.error_reply_count() != 0u
            || server.last_error() != net::errc::ok
            || !phase_delta_matches(
                before,
                after,
                2u,
                0u,
                2u,
                0u,
                1u,
                1u,
                0u,
                expected_server_queued_replies)
            || client_state.failed
            || client_state.error_calls != 0u
            || server_state.error_calls != 0u
            || !reply_ok) {
            std::fputs(verify_fail, stderr);
            return false;
        }
        return true;
    };

    const auto inspect_request = net::diag::ForwardInspectRequest::ipv4(
        net::Ipv4ForwardingPort::a,
        inspect_ip,
        64u);
    if (!run_query(
            "net lab udp diag forward inspect smoke host send failed\n",
            "net lab udp diag forward inspect smoke host stalled\n",
            "net lab udp diag forward inspect smoke host mismatch\n",
            inspect_request,
            20u,
            1u,
            1u,
            [&](const net::diag::ForwardInspectReply& reply) noexcept {
                return reply.forwarded()
                    && reply.uses_explicit_route()
                    && !reply.emits_icmp_error()
                    && same_reply(
                        reply,
                        net::Ipv4ForwardingDisposition::forwarded,
                        net::Ipv4ForwardingReason::explicit_route,
                        true,
                        net::Ipv4ForwardingPort::b,
                        true,
                        true,
                        inspect_ip,
                        32u,
                        true,
                        router2_a_ip,
                        1u,
                        false,
                        4u,
                        true,
                        3u,
                        inspect_ip,
                        32u,
                        net::Ipv4ForwardingPort::b,
                        true,
                        router2_a_ip,
                        1u);
            })) {
        return 6;
    }

    if (!router1.remove_route(inspect_ip, 32u) || router1.route_count() != 3u) {
        return fail("net lab udp diag forward inspect smoke remove host route failed\n", 7);
    }

    if (!run_query(
            "net lab udp diag forward inspect smoke specific send failed\n",
            "net lab udp diag forward inspect smoke specific stalled\n",
            "net lab udp diag forward inspect smoke specific mismatch\n",
            inspect_request,
            20u,
            0u,
            0u,
            [&](const net::diag::ForwardInspectReply& reply) noexcept {
                return reply.forwarded()
                    && reply.uses_explicit_route()
                    && !reply.emits_icmp_error()
                    && same_reply(
                        reply,
                        net::Ipv4ForwardingDisposition::forwarded,
                        net::Ipv4ForwardingReason::explicit_route,
                        true,
                        net::Ipv4ForwardingPort::a,
                        true,
                        true,
                        inspect_network,
                        24u,
                        false,
                        net::IpAddress{},
                        7u,
                        false,
                        3u,
                        true,
                        2u,
                        inspect_network,
                        24u,
                        net::Ipv4ForwardingPort::a,
                        false,
                        net::IpAddress{},
                        7u);
            })) {
        return 8;
    }

    if (!router1.remove_route(inspect_network, 24u) || router1.route_count() != 2u) {
        return fail("net lab udp diag forward inspect smoke remove specific route failed\n", 9);
    }

    if (!run_query(
            "net lab udp diag forward inspect smoke default send failed\n",
            "net lab udp diag forward inspect smoke default stalled\n",
            "net lab udp diag forward inspect smoke default mismatch\n",
            inspect_request,
            20u,
            0u,
            0u,
            [&](const net::diag::ForwardInspectReply& reply) noexcept {
                return reply.forwarded()
                    && reply.uses_explicit_route()
                    && !reply.emits_icmp_error()
                    && same_reply(
                        reply,
                        net::Ipv4ForwardingDisposition::forwarded,
                        net::Ipv4ForwardingReason::explicit_route,
                        true,
                        net::Ipv4ForwardingPort::b,
                        true,
                        true,
                        default_network,
                        0u,
                        true,
                        router2_a_ip,
                        9u,
                        false,
                        2u,
                        true,
                        1u,
                        default_network,
                        0u,
                        net::Ipv4ForwardingPort::b,
                        true,
                        router2_a_ip,
                        9u);
            })) {
        return 10;
    }

    const auto connected_request = net::diag::ForwardInspectRequest::ipv4(
        net::Ipv4ForwardingPort::a,
        router2_a_ip,
        64u);
    if (!run_query(
            "net lab udp diag forward inspect smoke connected send failed\n",
            "net lab udp diag forward inspect smoke connected stalled\n",
            "net lab udp diag forward inspect smoke connected mismatch\n",
            connected_request,
            20u,
            0u,
            0u,
            [&](const net::diag::ForwardInspectReply& reply) noexcept {
                return reply.forwarded()
                    && reply.uses_connected_prefix()
                    && !reply.emits_icmp_error()
                    && same_reply(
                        reply,
                        net::Ipv4ForwardingDisposition::forwarded,
                        net::Ipv4ForwardingReason::connected_prefix,
                        true,
                        net::Ipv4ForwardingPort::b,
                        true,
                        true,
                        net::IpAddress::ipv4(10, 0, 1, 0),
                        24u,
                        false,
                        net::IpAddress{},
                        0u,
                        true,
                        2u,
                        false,
                        0u,
                        net::IpAddress{},
                        0u,
                        net::Ipv4ForwardingPort::a,
                        false,
                        net::IpAddress{},
                        0u);
            })) {
        return 11;
    }

    if (!router1.remove_route(default_network, 0u) || router1.route_count() != 1u) {
        return fail("net lab udp diag forward inspect smoke remove default route failed\n", 12);
    }

    if (!run_query(
            "net lab udp diag forward inspect smoke no-route send failed\n",
            "net lab udp diag forward inspect smoke no-route stalled\n",
            "net lab udp diag forward inspect smoke no-route mismatch\n",
            inspect_request,
            20u,
            0u,
            0u,
            [&](const net::diag::ForwardInspectReply& reply) noexcept {
                return reply.destination_unreachable()
                    && !reply.has_egress_port()
                    && !reply.has_decision_snapshot()
                    && reply.emits_icmp_error()
                    && same_reply(
                        reply,
                        net::Ipv4ForwardingDisposition::destination_unreachable,
                        net::Ipv4ForwardingReason::no_route,
                        false,
                        net::Ipv4ForwardingPort::a,
                        true,
                        false,
                        net::IpAddress{},
                        0u,
                        false,
                        net::IpAddress{},
                        0u,
                        false,
                        1u,
                        false,
                        0u,
                        net::IpAddress{},
                        0u,
                        net::Ipv4ForwardingPort::a,
                        false,
                        net::IpAddress{},
                        0u);
            })) {
        return 13;
    }

    const auto client_target = client_node.pump().arp().table().lookup(server_ip);
    const auto router1_gateway = router1.arp_b().table().lookup(router2_a_ip);
    const auto router2_gateway = router2.arp_a().table().lookup(router1_b_ip);
    const auto server_target = server_node.pump().arp().table().lookup(client_ip);
    if (!client_target.has_value()
        || !router1_gateway.has_value()
        || !router2_gateway.has_value()
        || !server_target.has_value()
        || !same_mac(client_target.value(), router1_a_mac)
        || !same_mac(router1_gateway.value(), router2_a_mac)
        || !same_mac(router2_gateway.value(), router1_b_mac)
        || !same_mac(server_target.value(), router2_b_mac)) {
        return fail("net lab udp diag forward inspect smoke arp prime mismatch\n", 14);
    }

    std::puts("net lab udp diag forward inspect smoke: ok");
    return 0;
}
