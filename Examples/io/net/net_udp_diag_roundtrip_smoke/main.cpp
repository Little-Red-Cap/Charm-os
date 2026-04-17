#include <array>
#include <cstdio>

import charm.net;
import net.driver;
import net.packet;
import net.protocol.diagnostic_udp;
import util.core;
import util.expected;

namespace {
    template <util::usize Capacity, util::usize QueueDepth = 4>
    struct StubLinkDriver {
        net::OwnedPacketSinkRef input_sink{};
        net::PacketPool<QueueDepth, Capacity> rx_pool{};
        std::array<std::array<util::u8, Capacity>, QueueDepth> rx_frames{};
        std::array<util::usize, QueueDepth> rx_sizes{};
        util::usize rx_head{0};
        util::usize rx_tail{0};
        util::usize rx_count{0};

        std::array<util::u8, Capacity> tx_bytes{};
        util::usize tx_size{0};
        util::usize tx_calls{0};

        net::MacAddress mac{};
        StubLinkDriver* peer{nullptr};

        [[nodiscard]] net::NetDriverInfo info() const noexcept {
            return net::NetDriverInfo{
                .mtu = Capacity,
                .mac = mac,
                .capabilities = net::NetIfCapability::rx
                    | net::NetIfCapability::tx
                    | net::NetIfCapability::broadcast
            };
        }

        [[nodiscard]] net::Result<void> set_input_sink(net::OwnedPacketSinkRef sink) noexcept {
            input_sink = sink;
            return {};
        }

        [[nodiscard]] bool has_rx() const noexcept {
            return rx_count != 0;
        }

        [[nodiscard]] net::Result<void> enqueue_rx(net::PacketView packet) noexcept {
            if (rx_count == QueueDepth || packet.size() > Capacity) {
                return util::unexpected(net::errc::buffer_overflow);
            }

            rx_sizes[rx_tail] = packet.size();
            for (util::usize i = 0; i < packet.size(); ++i) {
                rx_frames[rx_tail][i] = packet[i];
            }
            rx_tail = (rx_tail + 1) % QueueDepth;
            ++rx_count;
            return {};
        }

        [[nodiscard]] net::Result<void> poll() noexcept {
            if (rx_count == 0) {
                return {};
            }
            if (!input_sink.valid()) {
                return util::unexpected(net::errc::bad_state);
            }

            auto lease = rx_pool.acquire();
            if (!lease) {
                return util::unexpected(lease.error());
            }

            auto appended = lease.value()->append(net::ByteView{
                rx_frames[rx_head].data(),
                rx_sizes[rx_head]
            });
            if (!appended) {
                return util::unexpected(appended.error());
            }

            rx_head = (rx_head + 1) % QueueDepth;
            --rx_count;

            return input_sink.consume(net::OwnedPacket{
                static_cast<typename net::PacketPool<QueueDepth, Capacity>::Lease&&>(lease.value())
            });
        }

        [[nodiscard]] net::Result<void> transmit(net::PacketView packet) noexcept {
            if (packet.size() > Capacity) {
                return util::unexpected(net::errc::buffer_overflow);
            }

            tx_size = packet.size();
            for (util::usize i = 0; i < packet.size(); ++i) {
                tx_bytes[i] = packet[i];
            }
            ++tx_calls;

            if (!peer) {
                return {};
            }
            return peer->enqueue_rx(packet);
        }
    };

    template <util::usize Capacity = 192>
    struct Node {
        static constexpr util::usize mtu = Capacity;

        StubLinkDriver<Capacity> link{};
        net::NetIf netif{};
        net::NetDriver driver{};
        net::Stack stack{};
        net::UdpStackPump<Capacity, 4, Capacity, 4, 64, 4> pump{};

        [[nodiscard]] net::Result<void> init(net::MacAddress mac, net::IpAddress address) noexcept {
            link.mac = mac;

            auto configured = netif.configure(net::NetIfConfig{
                .mtu = Capacity,
                .mac = mac,
                .address = address,
                .capabilities = net::NetIfCapability::rx
                    | net::NetIfCapability::tx
                    | net::NetIfCapability::broadcast
            });
            if (!configured) {
                return util::unexpected(configured.error());
            }

            auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
            if (!attached) {
                return util::unexpected(attached.error());
            }

            auto registered = stack.register_driver(driver);
            if (!registered) {
                return util::unexpected(registered.error());
            }

            auto up = netif.bring_up();
            if (!up) {
                return util::unexpected(up.error());
            }

            pump.bind(stack, netif);
            pump.configure(net::UdpStackPumpConfig{
                .egress = net::UdpEgressPumpConfig{
                    .retry_interval_ticks = 1,
                    .max_attempts = 4,
                }
            });
            return {};
        }
    };

    [[nodiscard]] bool same_mac(const net::MacAddress& lhs, const net::MacAddress& rhs) noexcept {
        for (util::usize i = 0; i < lhs.bytes.size(); ++i) {
            if (lhs.bytes[i] != rhs.bytes[i]) {
                return false;
            }
        }
        return true;
    }

    struct ClientState {
        util::u16 ping_request_id{0};
        util::u16 count_request_id{0};
        util::u16 slow_request_id{0};
        util::u16 meta_request_id{0};
        bool got_ping{false};
        bool got_count{false};
        bool got_slow_count{false};
        bool got_meta_unsupported{false};
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

        static void on_meta(void* ctx,
                            util::u16 request_id,
                            net::diag::udp::Status status,
                            const net::diag::MetaReply& response) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->got_meta_unsupported = request_id == self->meta_request_id
                && status == net::diag::udp::Status::unsupported
                && response.status == 0u
                && response.reflected_code == 0u
                && response.tag[0] == 0u
                && response.tag[1] == 0u;
            if (!self->got_meta_unsupported) {
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
        bool saw_count{false};
        bool saw_slow_count{false};
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

        static void on_error(void* ctx, net::errc error) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return;
            }

            ++self->error_calls;
            self->last_error = error;
        }
    };

    template <typename Client, typename ClientNode, typename ServerNode>
    [[nodiscard]] bool drive_until_idle(Client& client,
                                        ClientNode& client_node,
                                        ServerNode& server_node,
                                        util::usize max_steps = 16) noexcept {
        for (util::usize step = 0; step < max_steps; ++step) {
            auto client_progress = client_node.pump.service(1);
            if (!client_progress) {
                return false;
            }

            auto server_progress = server_node.pump.service(1);
            if (!server_progress) {
                return false;
            }

            if (!client.has_pending()
                && client_node.pump.pending_count() == 0
                && server_node.pump.pending_count() == 0
                && !client_node.link.has_rx()
                && !server_node.link.has_rx()) {
                return true;
            }
        }
        return false;
    }
}

int main() {
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    constexpr auto client_local = net::Endpoint::ipv4_any(9001);
    constexpr auto server_local = net::Endpoint::ipv4_any(7001);
    constexpr auto server_peer = net::Endpoint::ipv4(10, 0, 0, 9, 7001);

    auto fail = [](const char* message, int code) noexcept {
        std::fputs(message, stderr);
        return code;
    };

    Node<> client_node{};
    auto client_init = client_node.init(client_mac, client_ip);
    if (!client_init) {
        return fail("udp diag roundtrip smoke client init failed\n", 1);
    }

    Node<> server_node{};
    auto server_init = server_node.init(server_mac, server_ip);
    if (!server_init) {
        return fail("udp diag roundtrip smoke server init failed\n", 2);
    }

    client_node.link.peer = &server_node.link;
    server_node.link.peer = &client_node.link;

    net::diag::udp::Client<16, 4> client{};
    ClientState client_state{};
    client.set_error_handler(&ClientState::on_error, &client_state);

    net::diag::udp::Server<16> server{};
    ServerState server_state{};
    server.set_error_handler(&ServerState::on_error, &server_state);

    auto bound_client = net::bind_udp_protocol(client_node.pump, client_local, client);
    auto bound_server = net::bind_udp_protocol(server_node.pump, server_local, server);
    auto registered_ping = server.on_ping(&ServerState::on_ping, &server_state);
    auto registered_count = server.on_count(&ServerState::on_count, &server_state);
    auto registered_slow_count = server.on_slow_count(&ServerState::on_slow_count, &server_state);
    if (!bound_client
        || !bound_server
        || !registered_ping
        || !registered_count
        || !registered_slow_count
        || !client_node.pump.has_udp_binding(client_local.port)
        || !server_node.pump.has_udp_binding(server_local.port)
        || client_node.pump.udp_binding_count() != 1
        || server_node.pump.udp_binding_count() != 1) {
        return fail("udp diag roundtrip smoke bind failed\n", 3);
    }

    auto ping = client.ping(
        client_local,
        server_peer,
        net::diag::PingRequest{{'p', 'i', 'n', 'g'}},
        0,
        50,
        &ClientState::on_ping,
        nullptr,
        &client_state);
    if (!ping
        || client.pending_count() != 1
        || client.request_count() != 1
        || client.queued_count() != 1) {
        return fail("udp diag roundtrip smoke ping send failed\n", 4);
    }
    client_state.ping_request_id = ping.value();

    if (!drive_until_idle(client, client_node, server_node)) {
        return fail("udp diag roundtrip smoke ping exchange stalled\n", 5);
    }

    auto client_peer_mac = client_node.pump.arp().table().lookup(server_ip);
    auto server_peer_mac = server_node.pump.arp().table().lookup(client_ip);
    if (!client_peer_mac
        || !server_peer_mac
        || !same_mac(client_peer_mac.value(), server_mac)
        || !same_mac(server_peer_mac.value(), client_mac)
        || !client_state.got_ping
        || !server_state.saw_ping
        || client.pending_count() != 0
        || client.response_count() != 1
        || server.request_count() != 1
        || server.reply_count() != 1
        || server.error_reply_count() != 0
        || client_state.failed
        || client_state.error_calls != 0
        || server_state.error_calls != 0) {
        return fail("udp diag roundtrip smoke ping exchange failed\n", 6);
    }

    auto count = client.query_count(
        client_local,
        server_peer,
        10,
        50,
        &ClientState::on_count,
        nullptr,
        &client_state);
    if (!count
        || client.pending_count() != 1
        || client.request_count() != 2
        || client.queued_count() != 1) {
        return fail("udp diag roundtrip smoke count send failed\n", 7);
    }
    client_state.count_request_id = count.value();

    if (!drive_until_idle(client, client_node, server_node)) {
        return fail("udp diag roundtrip smoke count exchange stalled\n", 8);
    }

    if (!client_state.got_count
        || !server_state.saw_count
        || client.pending_count() != 0
        || client.response_count() != 2
        || server.request_count() != 2
        || server.reply_count() != 2
        || server.error_reply_count() != 0
        || client_state.failed
        || client_state.error_calls != 0
        || server_state.error_calls != 0) {
        return fail("udp diag roundtrip smoke count exchange failed\n", 9);
    }

    auto slow = client.query_slow_count(
        client_local,
        server_peer,
        net::diag::CounterValue{41},
        15,
        50,
        &ClientState::on_slow_count,
        nullptr,
        &client_state);
    if (!slow
        || client.pending_count() != 1
        || client.request_count() != 3
        || client.queued_count() != 1) {
        return fail("udp diag roundtrip smoke slow send failed\n", 10);
    }
    client_state.slow_request_id = slow.value();

    if (!drive_until_idle(client, client_node, server_node)) {
        return fail("udp diag roundtrip smoke slow exchange stalled\n", 11);
    }

    if (!client_state.got_slow_count
        || !server_state.saw_slow_count
        || client.pending_count() != 0
        || client.response_count() != 3
        || server.request_count() != 3
        || server.reply_count() != 3
        || server.error_reply_count() != 0
        || client_state.failed
        || client_state.error_calls != 0
        || server_state.error_calls != 0) {
        return fail("udp diag roundtrip smoke slow exchange failed\n", 12);
    }

    auto meta = client.query_meta(
        client_local,
        server_peer,
        net::diag::MetaRequest{
            .code = 0x1234u,
            .flags = 0x5Au,
            .tag = {'o', 'k'},
        },
        20,
        50,
        &ClientState::on_meta,
        nullptr,
        &client_state);
    if (!meta
        || client.pending_count() != 1
        || client.request_count() != 4
        || client.queued_count() != 1) {
        return fail("udp diag roundtrip smoke meta send failed\n", 13);
    }
    client_state.meta_request_id = meta.value();

    if (!drive_until_idle(client, client_node, server_node)) {
        return fail("udp diag roundtrip smoke meta exchange stalled\n", 14);
    }

    if (!client_state.got_meta_unsupported) {
        return fail("udp diag roundtrip smoke meta unsupported callback mismatch\n", 15);
    }
    if (client.pending_count() != 0) {
        return fail("udp diag roundtrip smoke meta left client pending\n", 16);
    }
    if (client.response_count() != 4) {
        return fail("udp diag roundtrip smoke meta client response count mismatch\n", 17);
    }
    if (client.last_error() != net::errc::ok) {
        return fail("udp diag roundtrip smoke meta client last_error mismatch\n", 18);
    }
    if (server.request_count() != 4) {
        return fail("udp diag roundtrip smoke meta server request count mismatch\n", 19);
    }
    if (server.reply_count() != 4) {
        return fail("udp diag roundtrip smoke meta server reply count mismatch\n", 20);
    }
    if (server.error_reply_count() != 1) {
        return fail("udp diag roundtrip smoke meta server error reply count mismatch\n", 21);
    }
    if (server.queued_reply_count() != 0) {
        return fail("udp diag roundtrip smoke meta server queued reply count mismatch\n", 22);
    }
    if (server.last_error() != net::errc::ok) {
        return fail("udp diag roundtrip smoke meta server last_error mismatch\n", 23);
    }
    if (client_node.pump.pending_count() != 0 || server_node.pump.pending_count() != 0) {
        return fail("udp diag roundtrip smoke meta left egress pending\n", 24);
    }
    if (client_node.link.has_rx() || server_node.link.has_rx()) {
        return fail("udp diag roundtrip smoke meta left wire packets queued\n", 25);
    }
    if (client_node.link.tx_calls < 5 || client_node.link.tx_calls > 6) {
        return fail("udp diag roundtrip smoke meta client tx count out of range\n", 26);
    }
    if (server_node.link.tx_calls < 5 || server_node.link.tx_calls > 6) {
        return fail("udp diag roundtrip smoke meta server tx count out of range\n", 27);
    }
    if (client_state.failed) {
        return fail("udp diag roundtrip smoke meta client callback flagged failure\n", 28);
    }
    if (client_state.error_calls != 0) {
        return fail("udp diag roundtrip smoke meta client error handler called\n", 29);
    }
    if (server_state.error_calls != 0) {
        return fail("udp diag roundtrip smoke meta server error handler called\n", 30);
    }

    std::puts("net udp diag roundtrip smoke: ok");
    return 0;
}
