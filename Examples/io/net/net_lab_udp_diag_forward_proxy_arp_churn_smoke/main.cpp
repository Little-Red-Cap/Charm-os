#include <cstdio>

import charm.net;
import net.protocol.diagnostic_udp;
import util.core;

namespace {
    using Link = net::lab::DuplexLink<192>;
    using HostPump = net::UdpStackPump<192, 4, 192, 4, 64, 4>;
    using HostNode = net::lab::StackNode<HostPump>;
    using ForwardHop = net::Ipv4ForwardingHop<192, 4, 192, 4>;

    [[nodiscard]] bool same_mac(const net::MacAddress& lhs,
                                const net::MacAddress& rhs) noexcept {
        for (util::usize index = 0; index < lhs.bytes.size(); ++index) {
            if (lhs.bytes[index] != rhs.bytes[index]) {
                return false;
            }
        }
        return true;
    }

    struct ClientState {
        util::u16 timeout_request_id{0};
        util::u16 count_request_id{0};
        bool got_timeout{false};
        bool got_count{false};
        bool failed{false};
        util::usize error_calls{0};
        net::errc last_error{net::errc::ok};

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
                         "udp diag forward proxy arp churn client service error=%d\n",
                         static_cast<int>(client_progress.error()));
            return false;
        }

        auto router1_progress = router1.service(1);
        if (!router1_progress) {
            std::fprintf(stderr,
                         "udp diag forward proxy arp churn router1 service error=%d\n",
                         static_cast<int>(router1_progress.error()));
            return false;
        }

        auto router2_progress = router2.service(1);
        if (!router2_progress) {
            std::fprintf(stderr,
                         "udp diag forward proxy arp churn router2 service error=%d\n",
                         static_cast<int>(router2_progress.error()));
            return false;
        }

        auto server_progress = server_node.service(1);
        if (!server_progress) {
            std::fprintf(stderr,
                         "udp diag forward proxy arp churn server service error=%d\n",
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
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x36u, 0x00u, 0x00u, 0x00u, 0x02u);
    constexpr auto router1_a_mac = net::MacAddress::from_bytes(0x02u, 0x36u, 0x00u, 0x00u, 0x00u, 0x11u);
    constexpr auto router1_b_mac = net::MacAddress::from_bytes(0x02u, 0x36u, 0x00u, 0x00u, 0x00u, 0x12u);
    constexpr auto router2_a_mac = net::MacAddress::from_bytes(0x02u, 0x36u, 0x00u, 0x00u, 0x00u, 0x21u);
    constexpr auto router2_b_mac = net::MacAddress::from_bytes(0x02u, 0x36u, 0x00u, 0x00u, 0x00u, 0x22u);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0x36u, 0x00u, 0x00u, 0x00u, 0x09u);

    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto router1_a_ip = net::IpAddress::ipv4(10, 0, 0, 1);
    constexpr auto router1_b_ip = net::IpAddress::ipv4(10, 0, 1, 1);
    constexpr auto router2_a_ip = net::IpAddress::ipv4(10, 0, 1, 2);
    constexpr auto router2_b_ip = net::IpAddress::ipv4(10, 0, 2, 1);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 2, 9);

    constexpr auto synthetic_ip_a = net::IpAddress::ipv4(10, 0, 2, 250);
    constexpr auto synthetic_ip_b = net::IpAddress::ipv4(10, 0, 2, 251);
    constexpr auto routed_network = net::IpAddress::ipv4(10, 0, 2, 0);

    constexpr auto client_local = net::Endpoint::ipv4_any(9001);
    constexpr auto synthetic_peer_a = net::Endpoint::ipv4(10, 0, 2, 250, 7999);
    constexpr auto synthetic_peer_b = net::Endpoint::ipv4(10, 0, 2, 251, 7999);
    constexpr auto server_peer = net::Endpoint::ipv4(10, 0, 2, 9, 7001);
    constexpr auto server_local = net::Endpoint::ipv4_any(7001);

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
        return fail("net lab udp diag forward proxy arp churn smoke client init failed\n", 1);
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
    auto router1_server_route = router1.set_gateway_route(
        routed_network,
        24u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    if (!router1_a_init
        || !router1_b_init
        || !router1_prefix_a
        || !router1_prefix_b
        || !router1_server_route
        || router1.route_count() != 1u
        || !router1.ready()) {
        return fail("net lab udp diag forward proxy arp churn smoke router1 init failed\n", 2);
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
        return fail("net lab udp diag forward proxy arp churn smoke router2 init failed\n", 3);
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
        return fail("net lab udp diag forward proxy arp churn smoke server init failed\n", 4);
    }

    net::diag::udp::EndpointClient<16, 4> client{client_local, synthetic_peer_a};
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
        return fail("net lab udp diag forward proxy arp churn smoke bind failed\n", 5);
    }

    client_state.got_timeout = false;
    auto request_a = client.query_meta(
        net::diag::MetaRequest{
            .code = 0x1111u,
            .flags = 0xAAu,
            .tag = {'a', '1'},
        },
        client_link.now_ticks(),
        28,
        nullptr,
        &ClientState::on_timeout,
        &client_state);
    if (!request_a
        || client.pending_count() != 1u
        || client.request_count() != 1u
        || client.queued_count() != 1u) {
        return fail("net lab udp diag forward proxy arp churn smoke phase a send failed\n", 6);
    }
    client_state.timeout_request_id = request_a.value();

    if (!drive_until_settled(
            client,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab udp diag forward proxy arp churn smoke phase a stalled\n", 7);
    }

    const auto client_synthetic_a = client_node.pump().arp().table().lookup(synthetic_ip_a);
    const auto router1_gateway = router1.arp_b().table().lookup(router2_a_ip);
    if (!client_state.got_timeout
        || client.pending_count() != 0u
        || client.response_count() != 0u
        || client.timeout_count() != 1u
        || client.drop_count() != 0u
        || client.last_error() != net::errc::ok
        || !client_synthetic_a
        || !same_mac(client_synthetic_a.value(), router1_a_mac)
        || !router1_gateway
        || !same_mac(router1_gateway.value(), router2_a_mac)
        || client_node.pump().arp().failed_count() != 0u
        || router1.proxy_arp_reply_count() != 1u
        || router2.proxy_arp_reply_count() != 0u
        || router1.forwarded_count() != 1u
        || router2.forwarded_count() != 1u
        || router1.destination_unreachable_count() != 0u
        || router2.destination_unreachable_count() != 0u
        || router2.arp_b().request_count() == 0u
        || router2.arp_b().failed_count() == 0u
        || server.request_count() != 0u
        || server.reply_count() != 0u
        || server.queued_reply_count() != 0u
        || server.error_reply_count() != 0u
        || client_state.failed
        || client_state.error_calls != 0u
        || server_state.error_calls != 0u) {
        return fail("net lab udp diag forward proxy arp churn smoke phase a mismatch\n", 8);
    }

    const auto client_arp_failed_before_b = client_node.pump().arp().failed_count();
    const auto client_link_b_to_a_before_b = client_link.stats_b_to_a();
    const auto forwarded_before_b = router1.forwarded_count();
    const auto proxy_before_b = router1.proxy_arp_reply_count();

    if (!router1.remove_route(routed_network, 24u)
        || router1.route_count() != 0u
        || router1.remove_route(routed_network, 24u)) {
        return fail("net lab udp diag forward proxy arp churn smoke delete route failed\n", 9);
    }

    client.configure(client_local, synthetic_peer_b);
    client_state.got_timeout = false;
    auto request_b = client.query_meta(
        net::diag::MetaRequest{
            .code = 0x2222u,
            .flags = 0xBBu,
            .tag = {'b', '2'},
        },
        client_link.now_ticks(),
        28,
        nullptr,
        &ClientState::on_timeout,
        &client_state);
    if (!request_b
        || client.pending_count() != 1u
        || client.request_count() != 2u
        || client.queued_count() != 2u) {
        return fail("net lab udp diag forward proxy arp churn smoke phase b send failed\n", 10);
    }
    client_state.timeout_request_id = request_b.value();

    if (!drive_until_settled(
            client,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab udp diag forward proxy arp churn smoke phase b stalled\n", 11);
    }

    const auto client_synthetic_b = client_node.pump().arp().table().lookup(synthetic_ip_b);
    const auto client_link_b_to_a_after_b = client_link.stats_b_to_a();
    if (!client_state.got_timeout
        || client.pending_count() != 0u
        || client.response_count() != 0u
        || client.timeout_count() != 2u
        || client.drop_count() != 0u
        || client.last_error() != net::errc::ok
        || client_synthetic_b
        || client_node.pump().arp().failed_count() <= client_arp_failed_before_b
        || router1.proxy_arp_reply_count() != proxy_before_b
        || client_link_b_to_a_after_b.delivered != client_link_b_to_a_before_b.delivered
        || router1.forwarded_count() != forwarded_before_b
        || router2.forwarded_count() != 1u
        || router1.destination_unreachable_count() != 0u
        || router2.destination_unreachable_count() != 0u
        || server.request_count() != 0u
        || server.reply_count() != 0u
        || server.queued_reply_count() != 0u
        || server.error_reply_count() != 0u
        || client_state.failed
        || client_state.error_calls != 0u
        || server_state.error_calls != 0u) {
        return fail("net lab udp diag forward proxy arp churn smoke phase b mismatch\n", 12);
    }

    auto restored_route = router1.set_gateway_route(
        routed_network,
        24u,
        net::Ipv4ForwardingPort::b,
        router2_a_ip);
    if (!restored_route || router1.route_count() != 1u) {
        return fail("net lab udp diag forward proxy arp churn smoke restore route failed\n", 13);
    }

    client.configure(client_local, server_peer);
    client_state.got_count = false;
    server_state.saw_count = false;
    const auto forwarded_before_c = router1.forwarded_count();
    const auto response_before_c = client.response_count();
    const auto proxy_before_c_router1 = router1.proxy_arp_reply_count();
    const auto proxy_before_c_router2 = router2.proxy_arp_reply_count();

    auto count = client.query_count(
        client_link.now_ticks(),
        40,
        &ClientState::on_count,
        nullptr,
        &client_state);
    if (!count
        || client.pending_count() != 1u
        || client.request_count() != 3u
        || client.queued_count() != 3u) {
        return fail("net lab udp diag forward proxy arp churn smoke phase c send failed\n", 14);
    }
    client_state.count_request_id = count.value();

    if (!drive_until_settled(
            client,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab udp diag forward proxy arp churn smoke phase c stalled\n", 15);
    }

    const auto client_server = client_node.pump().arp().table().lookup(server_ip);
    const auto server_client = server_node.pump().arp().table().lookup(client_ip);
    const auto router2_client_gateway = router2.arp_a().table().lookup(router1_b_ip);
    if (!client_state.got_count
        || !server_state.saw_count
        || client.pending_count() != 0u
        || client.response_count() != (response_before_c + 1u)
        || client.timeout_count() != 2u
        || client.drop_count() != 0u
        || client.last_error() != net::errc::ok
        || !client_server
        || !same_mac(client_server.value(), router1_a_mac)
        || !server_client
        || !same_mac(server_client.value(), router2_b_mac)
        || !router2_client_gateway
        || !same_mac(router2_client_gateway.value(), router1_b_mac)
        || router1.proxy_arp_reply_count() != (proxy_before_c_router1 + 1u)
        || router2.proxy_arp_reply_count() != (proxy_before_c_router2 + 1u)
        || router1.forwarded_count() != (forwarded_before_c + 2u)
        || router2.forwarded_count() != 3u
        || router1.destination_unreachable_count() != 0u
        || router2.destination_unreachable_count() != 0u
        || server.request_count() != 1u
        || server.reply_count() != 1u
        || server.queued_reply_count() != 1u
        || server.error_reply_count() != 0u
        || server.last_error() != net::errc::ok
        || !network_idle(
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)
        || client_state.failed
        || client_state.error_calls != 0u
        || server_state.error_calls != 0u) {
        return fail("net lab udp diag forward proxy arp churn smoke phase c mismatch\n", 16);
    }

    std::puts("net lab udp diag forward proxy arp churn smoke: ok");
    return 0;
}
