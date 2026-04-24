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
        util::u16 ping_request_id{0};
        util::u16 slow_request_id{0};
        util::u16 count_request_id{0};
        bool got_ping{false};
        bool got_timeout{false};
        bool got_slow_count{false};
        bool got_count{false};
        bool failed{false};
        util::usize error_calls{0};
        net::errc last_error{net::errc::ok};

        static void on_ping(void* ctx,
                            util::u16 request_id,
                            net::diag::udp::Status status,
                            const net::diag::PingReply& response) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->got_ping = request_id == self->ping_request_id
                && status == net::diag::udp::Status::ok
                && response.text[0] == 'p'
                && response.text[1] == 'o'
                && response.text[2] == 'n'
                && response.text[3] == 'g';
            if (!self->got_ping) {
                self->failed = true;
            }
        }

        static void on_timeout(void* ctx, util::u16 request_id) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->got_timeout = request_id == self->slow_request_id;
            if (!self->got_timeout) {
                self->failed = true;
            }
        }

        static void on_slow_count(void* ctx,
                                  util::u16 request_id,
                                  net::diag::udp::Status status,
                                  const net::diag::CounterValue& response) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->got_slow_count = request_id == self->slow_request_id
                && status == net::diag::udp::Status::ok
                && response.value == 42u;
            if (!self->got_slow_count) {
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
        bool saw_ping{false};
        bool saw_slow_count{false};
        bool saw_count{false};
        util::usize error_calls{0};
        net::errc last_error{net::errc::ok};

        static net::diag::udp::Status on_ping(void* ctx,
                                              const net::diag::PingRequest& request,
                                              net::diag::PingReply& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return net::diag::udp::Status::internal_error;
            }

            self->saw_ping = request.text[0] == 'p'
                && request.text[1] == 'i'
                && request.text[2] == 'n'
                && request.text[3] == 'g';
            if (!self->saw_ping) {
                return net::diag::udp::Status::bad_request;
            }

            response.text[0] = 'p';
            response.text[1] = 'o';
            response.text[2] = 'n';
            response.text[3] = 'g';
            return net::diag::udp::Status::ok;
        }

        static net::diag::udp::Status on_slow_count(void* ctx,
                                                    const net::diag::CounterValue& request,
                                                    net::diag::CounterValue& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return net::diag::udp::Status::internal_error;
            }

            self->saw_slow_count = request.value == 41u;
            if (!self->saw_slow_count) {
                return net::diag::udp::Status::bad_request;
            }

            response.value = static_cast<util::u16>(request.value + 1u);
            return net::diag::udp::Status::ok;
        }

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
                         "udp diag forward late reply client service error=%d\n",
                         static_cast<int>(client_progress.error()));
            return false;
        }

        auto router1_progress = router1.service(1);
        if (!router1_progress) {
            std::fprintf(stderr,
                         "udp diag forward late reply router1 service error=%d\n",
                         static_cast<int>(router1_progress.error()));
            return false;
        }

        auto router2_progress = router2.service(1);
        if (!router2_progress) {
            std::fprintf(stderr,
                         "udp diag forward late reply router2 service error=%d\n",
                         static_cast<int>(router2_progress.error()));
            return false;
        }

        auto server_progress = server_node.service(1);
        if (!server_progress) {
            std::fprintf(stderr,
                         "udp diag forward late reply server service error=%d\n",
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
    [[nodiscard]] bool drive_until_timeout(Client& client,
                                           util::usize target_timeout_count,
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

            if (client.timeout_count() == target_timeout_count
                && client.pending_count() == 0u) {
                return true;
            }
        }
        return false;
    }

    template <typename Client>
    [[nodiscard]] bool drive_until_network_idle(Client& client,
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

            if (network_idle(
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

    template <typename Client>
    [[nodiscard]] bool drive_until_settled(Client& client,
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
        return fail("net lab udp diag forward late reply smoke client init failed\n", 1);
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
        net::IpAddress::ipv4(10, 0, 2, 0),
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
        return fail("net lab udp diag forward late reply smoke router1 init failed\n", 2);
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
        return fail("net lab udp diag forward late reply smoke router2 init failed\n", 3);
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
        return fail("net lab udp diag forward late reply smoke server init failed\n", 4);
    }

    net::diag::udp::EndpointClient<16, 4> client{client_local, server_peer};
    ClientState client_state{};
    client.set_error_handler(&ClientState::on_error, &client_state);

    net::diag::udp::EndpointServer<16> server{server_local};
    ServerState server_state{};
    server.set_error_handler(&ServerState::on_error, &server_state);

    auto bound_client = client.bind(client_node.pump());
    auto bound_server = server.bind(server_node.pump());
    auto registered_ping = server.on_ping(&ServerState::on_ping, &server_state);
    auto registered_slow_count = server.on_slow_count(&ServerState::on_slow_count, &server_state);
    auto registered_count = server.on_count(&ServerState::on_count, &server_state);
    if (!bound_client
        || !bound_server
        || !registered_ping
        || !registered_slow_count
        || !registered_count
        || !client_node.pump().has_udp_binding(client.local_endpoint().port)
        || !server_node.pump().has_udp_binding(server.local_endpoint().port)
        || client_node.pump().udp_binding_count() != 1u
        || server_node.pump().udp_binding_count() != 1u) {
        return fail("net lab udp diag forward late reply smoke bind failed\n", 5);
    }

    auto ping = client.ping(
        net::diag::PingRequest{{'p', 'i', 'n', 'g'}},
        client_link.now_ticks(),
        64,
        &ClientState::on_ping,
        nullptr,
        &client_state);
    if (!ping
        || client.pending_count() != 1u
        || client.request_count() != 1u
        || client.queued_count() != 1u) {
        return fail("net lab udp diag forward late reply smoke prime ping send failed\n", 6);
    }
    client_state.ping_request_id = ping.value();

    if (!drive_until_settled(
            client,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link)) {
        return fail("net lab udp diag forward late reply smoke prime ping stalled\n", 7);
    }

    const auto client_target = client_node.pump().arp().table().lookup(server_ip);
    const auto router1_gateway = router1.arp_b().table().lookup(router2_a_ip);
    const auto router2_gateway = router2.arp_a().table().lookup(router1_b_ip);
    const auto server_target = server_node.pump().arp().table().lookup(client_ip);
    if (!client_target
        || !router1_gateway
        || !router2_gateway
        || !server_target
        || !same_mac(client_target.value(), router1_a_mac)
        || !same_mac(router1_gateway.value(), router2_a_mac)
        || !same_mac(router2_gateway.value(), router1_b_mac)
        || !same_mac(server_target.value(), router2_b_mac)
        || !client_state.got_ping
        || !server_state.saw_ping
        || client.pending_count() != 0u
        || client.response_count() != 1u
        || client.timeout_count() != 0u
        || client.drop_count() != 0u
        || client.last_error() != net::errc::ok
        || server.request_count() != 1u
        || server.reply_count() != 1u
        || server.error_reply_count() != 0u
        || server.queued_reply_count() != 1u
        || server.last_error() != net::errc::ok
        || router1.forwarded_count() != 2u
        || router2.forwarded_count() != 2u
        || router1.destination_unreachable_count() != 0u
        || router2.destination_unreachable_count() != 0u
        || router1.ttl_expired_count() != 0u
        || router2.ttl_expired_count() != 0u
        || router1.proxy_arp_reply_count() != 1u
        || router2.proxy_arp_reply_count() != 1u
        || client_state.failed
        || client_state.error_calls != 0u
        || server_state.error_calls != 0u) {
        return fail("net lab udp diag forward late reply smoke prime ping failed\n", 8);
    }

    client_link.set_latency_b_to_a(24u);
    client_state.got_timeout = false;
    client_state.got_slow_count = false;
    server_state.saw_slow_count = false;

    const auto forwarded_before_late = router1.forwarded_count();
    const auto response_before_late = client.response_count();
    const auto timeout_before_late = client.timeout_count();
    const auto drop_before_late = client.drop_count();
    const auto server_requests_before_late = server.request_count();
    const auto server_replies_before_late = server.reply_count();

    auto slow = client.query_slow_count(
        net::diag::CounterValue{41u},
        client_link.now_ticks(),
        12,
        &ClientState::on_slow_count,
        &ClientState::on_timeout,
        &client_state);
    if (!slow
        || client.pending_count() != 1u
        || client.request_count() != 2u
        || client.queued_count() != 1u) {
        return fail("net lab udp diag forward late reply smoke late send failed\n", 9);
    }
    client_state.slow_request_id = slow.value();

    if (!drive_until_timeout(
            client,
            timeout_before_late + 1u,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link,
            48)) {
        return fail("net lab udp diag forward late reply smoke timeout stalled\n", 10);
    }

    if (!client_state.got_timeout
        || client_state.got_slow_count
        || !server_state.saw_slow_count
        || client.pending_count() != 0u
        || client.response_count() != response_before_late
        || client.timeout_count() != (timeout_before_late + 1u)
        || client.drop_count() != drop_before_late
        || client.last_error() != net::errc::ok
        || server.request_count() != (server_requests_before_late + 1u)
        || server.reply_count() != (server_replies_before_late + 1u)
        || server.error_reply_count() != 0u
        || server.queued_reply_count() != 1u
        || server.last_error() != net::errc::ok
        || router1.forwarded_count() != (forwarded_before_late + 2u)
        || router2.forwarded_count() != (forwarded_before_late + 2u)
        || router1.destination_unreachable_count() != 0u
        || router2.destination_unreachable_count() != 0u
        || router1.ttl_expired_count() != 0u
        || router2.ttl_expired_count() != 0u
        || router1.proxy_arp_reply_count() != 1u
        || router2.proxy_arp_reply_count() != 1u
        || client_link.stats_b_to_a().pending != 1u
        || client_link.idle()
        || !middle_link.idle()
        || !server_link.idle()
        || network_idle(
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
        return fail("net lab udp diag forward late reply smoke timeout mismatch\n", 11);
    }

    client_link.set_latency_b_to_a(1u);

    if (!drive_until_network_idle(
            client,
            client_node,
            router1,
            router2,
            server_node,
            client_link,
            middle_link,
            server_link,
            48)) {
        return fail("net lab udp diag forward late reply smoke late drain stalled\n", 12);
    }

    if (client_state.got_slow_count
        || client.response_count() != response_before_late
        || client.timeout_count() != (timeout_before_late + 1u)
        || client.drop_count() != (drop_before_late + 1u)
        || client.last_error() != net::errc::ok
        || server.request_count() != (server_requests_before_late + 1u)
        || server.reply_count() != (server_replies_before_late + 1u)
        || server.queued_reply_count() != 1u
        || router1.forwarded_count() != (forwarded_before_late + 2u)
        || router2.forwarded_count() != (forwarded_before_late + 2u)
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
        return fail("net lab udp diag forward late reply smoke late drain mismatch\n", 13);
    }

    client_state.got_count = false;
    server_state.saw_count = false;
    const auto forwarded_before_recovery = router1.forwarded_count();
    const auto response_before_recovery = client.response_count();
    const auto timeout_before_recovery = client.timeout_count();
    const auto drop_before_recovery = client.drop_count();
    const auto server_requests_before_recovery = server.request_count();
    const auto server_replies_before_recovery = server.reply_count();

    auto count = client.query_count(
        client_link.now_ticks(),
        20,
        &ClientState::on_count,
        nullptr,
        &client_state);
    if (!count
        || client.pending_count() != 1u
        || client.request_count() != 3u
        || client.queued_count() != 1u) {
        return fail("net lab udp diag forward late reply smoke recovery send failed\n", 14);
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
        return fail("net lab udp diag forward late reply smoke recovery stalled\n", 15);
    }

    if (!client_state.got_count
        || !server_state.saw_count
        || client.pending_count() != 0u
        || client.response_count() != (response_before_recovery + 1u)
        || client.timeout_count() != timeout_before_recovery
        || client.drop_count() != drop_before_recovery
        || client.last_error() != net::errc::ok
        || server.request_count() != (server_requests_before_recovery + 1u)
        || server.reply_count() != (server_replies_before_recovery + 1u)
        || server.error_reply_count() != 0u
        || server.queued_reply_count() != 1u
        || server.last_error() != net::errc::ok
        || router1.forwarded_count() != (forwarded_before_recovery + 2u)
        || router2.forwarded_count() != (forwarded_before_recovery + 2u)
        || router1.destination_unreachable_count() != 0u
        || router2.destination_unreachable_count() != 0u
        || router1.ttl_expired_count() != 0u
        || router2.ttl_expired_count() != 0u
        || router1.proxy_arp_reply_count() != 1u
        || router2.proxy_arp_reply_count() != 1u
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
        return fail("net lab udp diag forward late reply smoke recovery mismatch\n", 16);
    }

    std::puts("net lab udp diag forward late reply smoke: ok");
    return 0;
}
