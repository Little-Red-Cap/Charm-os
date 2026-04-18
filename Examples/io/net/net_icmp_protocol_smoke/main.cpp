#include <array>
#include <cstdio>

import charm.net;
import net.driver;
import net.packet;
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
        StubLinkDriver<Capacity> link{};
        net::NetIf netif{};
        net::NetDriver driver{};
        net::Stack stack{};
        net::IcmpStackPump<Capacity, 4, Capacity, 4, 64> pump{};

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
            pump.configure(net::IcmpStackPumpConfig{
                .egress = net::IcmpEgressPumpConfig{
                    .retry_interval_ticks = 2,
                    .max_attempts = 4,
                }
            });
            return {};
        }
    };

    struct ClientState {
        bool got_reply{false};
        bool got_timeout{false};
        bool got_scoped_reply{false};
        bool got_scoped_timeout{false};
        net::errc last_error{net::errc::ok};
        util::usize error_calls{0};
        util::usize reply_calls{0};
        util::usize scoped_reply_calls{0};
        util::usize timeout_calls{0};
        util::usize scoped_timeout_calls{0};
        net::IcmpEchoInfo info{};
        net::IcmpEchoInfo timeout_info{};
        net::IcmpEchoInfo scoped_info{};
        net::IcmpEchoInfo scoped_timeout_info{};
        std::array<util::u8, 16> payload{};
        util::usize payload_size{0};
        std::array<util::u8, 16> scoped_payload{};
        util::usize scoped_payload_size{0};

        [[nodiscard]] static net::Result<void> on_reply(void* ctx,
                                                        const net::IcmpEchoInfo& info,
                                                        net::PacketView packet) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return util::unexpected(net::errc::bad_state);
            }

            if (packet.size() > self->payload.size()) {
                self->last_error = net::errc::buffer_overflow;
                return util::unexpected(self->last_error);
            }
            for (util::usize i = 0; i < packet.size(); ++i) {
                self->payload[i] = packet[i];
            }
            self->payload_size = packet.size();
            self->info = info;
            self->got_reply = true;
            ++self->reply_calls;
            return {};
        }

        [[nodiscard]] static net::Result<void> on_scoped_reply(void* ctx,
                                                               const net::IcmpEchoInfo& info,
                                                               net::PacketView packet) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return util::unexpected(net::errc::bad_state);
            }
            if (packet.size() > self->scoped_payload.size()) {
                self->last_error = net::errc::buffer_overflow;
                return util::unexpected(self->last_error);
            }
            for (util::usize i = 0; i < packet.size(); ++i) {
                self->scoped_payload[i] = packet[i];
            }
            self->scoped_payload_size = packet.size();
            self->scoped_info = info;
            self->got_scoped_reply = true;
            ++self->scoped_reply_calls;
            return {};
        }

        static void on_timeout(void* ctx, const net::IcmpEchoInfo& info) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }
            self->got_timeout = true;
            self->timeout_info = info;
            ++self->timeout_calls;
        }

        static void on_scoped_timeout(void* ctx, const net::IcmpEchoInfo& info) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }
            self->got_scoped_timeout = true;
            self->scoped_timeout_info = info;
            ++self->scoped_timeout_calls;
        }

        static void on_error(void* ctx, net::errc error) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }
            self->last_error = error;
            ++self->error_calls;
        }
    };

    struct ServerState {
        bool saw_request{false};
        net::errc last_error{net::errc::ok};
        util::usize error_calls{0};
        net::IcmpEchoInfo info{};
        std::array<util::u8, 16> payload{};
        util::usize payload_size{0};

        [[nodiscard]] static net::Result<void> on_request(void* ctx,
                                                          const net::IcmpEchoInfo& info,
                                                          net::PacketView packet) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return util::unexpected(net::errc::bad_state);
            }

            if (packet.size() > self->payload.size()) {
                self->last_error = net::errc::buffer_overflow;
                return util::unexpected(self->last_error);
            }
            for (util::usize i = 0; i < packet.size(); ++i) {
                self->payload[i] = packet[i];
            }
            self->payload_size = packet.size();
            self->info = info;
            self->saw_request = true;
            return {};
        }

        static void on_error(void* ctx, net::errc error) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return;
            }
            self->last_error = error;
            ++self->error_calls;
        }
    };

    template <typename ClientNode, typename ServerNode>
    [[nodiscard]] bool drive_until_idle(ClientNode& client_node,
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

            if (client_node.pump.pending_count() == 0
                && server_node.pump.pending_count() == 0
                && !client_node.link.has_rx()
                && !server_node.link.has_rx()) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] bool same_mac(const net::MacAddress& lhs, const net::MacAddress& rhs) noexcept {
        for (util::usize i = 0; i < lhs.bytes.size(); ++i) {
            if (lhs.bytes[i] != rhs.bytes[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool same_ipv4(const net::IpAddress& lhs, const net::IpAddress& rhs) noexcept {
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

    template <util::usize N, util::usize M>
    [[nodiscard]] bool bytes_eq(const util::u8 (&lhs)[N],
                                const std::array<util::u8, M>& rhs,
                                util::usize rhs_size) noexcept {
        if (rhs_size != N) {
            return false;
        }
        for (util::usize i = 0; i < N; ++i) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        return true;
    }
}

int main() {
    constexpr auto client_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto client_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto server_mac = net::MacAddress::from_bytes(0x02u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu);
    constexpr auto server_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    static constexpr util::u8 payload[]{'p', 'i', 'n', 'g'};

    auto fail = [](const char* message, int code) noexcept {
        std::fputs(message, stderr);
        return code;
    };

    net::icmp::echo::Client dry_client{client_ip, server_ip};
    auto dry_run = dry_client.ping(0x0001u, 0x0001u, net::ByteView{payload, sizeof(payload)});
    if (dry_run || dry_run.error() != net::errc::bad_state) {
        return fail("icmp protocol smoke dry-run bad_state check failed\n", 1);
    }

    Node<> client_node{};
    auto client_init = client_node.init(client_mac, client_ip);
    if (!client_init) {
        return fail("icmp protocol smoke client init failed\n", 2);
    }

    Node<> server_node{};
    auto server_init = server_node.init(server_mac, server_ip);
    if (!server_init) {
        return fail("icmp protocol smoke server init failed\n", 3);
    }

    client_node.link.peer = &server_node.link;
    server_node.link.peer = &client_node.link;

    Node<> probe_client_node{};
    auto probe_client_init = probe_client_node.init(client_mac, client_ip);
    if (!probe_client_init) {
        return fail("icmp protocol smoke probe client init failed\n", 4);
    }

    Node<> probe_server_node{};
    auto probe_server_init = probe_server_node.init(server_mac, server_ip);
    if (!probe_server_init) {
        return fail("icmp protocol smoke probe server init failed\n", 5);
    }

    probe_client_node.link.peer = &probe_server_node.link;
    probe_server_node.link.peer = &probe_client_node.link;

    net::icmp::echo::Probe<16> probe{net::IpAddress::ipv4_any(), server_ip};
    net::icmp::echo::AutoReplyServer probe_server{};
    auto bound_probe = probe.bind(probe_client_node.pump);
    auto bound_probe_server = probe_server.bind(probe_server_node.pump);
    if (!bound_probe
        || !bound_probe_server
        || !probe_client_node.pump.has_echo_sink()
        || !probe_server_node.pump.has_echo_sink()) {
        return fail("icmp protocol smoke probe bind failed\n", 6);
    }

    auto probe_ping = probe.ping(net::ByteView{payload, sizeof(payload)}, 8, 20);
    const auto probe_pending_result = probe.result();
    if (!probe_ping
        || probe_ping.value().disposition != net::IcmpSendDisposition::queued
        || !probe.has_pending()
        || probe.pending_count() != 1
        || probe.request_count() != 1
        || probe.queued_count() != 1
        || probe.transmitted_count() != 0
        || probe.error_count() != 0
        || probe.observed_error() != net::errc::ok
        || !probe_pending_result.pending()
        || probe_pending_result.ready()
        || probe_pending_result.ok()
        || probe_pending_result.has_value()
        || probe_pending_result.timed_out()
        || probe_pending_result.cancelled()
        || probe_pending_result.failed()
        || probe_pending_result.error != net::errc::ok
        || probe_pending_result.identifier() != probe_ping.value().info.identifier
        || probe_pending_result.sequence() != probe_ping.value().info.sequence
        || probe_pending_result.has_payload()
        || probe_pending_result.payload_size() != 0
        || probe_pending_result.value_payload().size() != 0
        || probe_client_node.link.tx_calls != 1) {
        return fail("icmp protocol smoke probe send failed\n", 7);
    }

    if (!drive_until_idle(probe_client_node, probe_server_node)) {
        return fail("icmp protocol smoke probe exchange stalled\n", 8);
    }

    auto probe_reply = probe.last_reply_payload();
    const auto probe_reply_result = probe.result();
    const auto probe_reply_summary_payload = probe_reply_result.value_payload();
    if (!probe.has_reply()
        || probe.has_timeout()
        || !probe.has_result()
        || !probe_reply_result.ready()
        || !probe_reply_result.ok()
        || !probe_reply_result.has_value()
        || probe_reply_result.pending()
        || probe_reply_result.timed_out()
        || probe_reply_result.cancelled()
        || probe_reply_result.failed()
        || probe.pending_count() != 0
        || probe.reply_count() != 1
        || probe.timeout_count() != 0
        || probe.drop_count() != 0
        || probe.error_count() != 0
        || probe_reply_result.error != net::errc::ok
        || !same_ipv4(probe.last_reply_info().local, client_ip)
        || !same_ipv4(probe.last_reply_info().peer, server_ip)
        || probe.last_reply_info().identifier != probe_ping.value().info.identifier
        || probe.last_reply_info().sequence != probe_ping.value().info.sequence
        || probe_reply_result.identifier() != probe_ping.value().info.identifier
        || probe_reply_result.sequence() != probe_ping.value().info.sequence
        || !probe_reply_result.has_payload()
        || probe_reply_result.payload_size() != sizeof(payload)
        || probe_reply_summary_payload.size() != sizeof(payload)
        || probe_reply.size() != sizeof(payload)
        || probe_server.request_count() != 1
        || probe_server.reply_count() != 1
        || probe_server.transmitted_count() != 1
        || probe_server.drop_count() != 0) {
        return fail("icmp protocol smoke probe reply mismatch\n", 9);
    }
    for (util::usize index = 0; index < sizeof(payload); ++index) {
        if (probe_reply[index] != payload[index]
            || probe_reply_summary_payload[index] != payload[index]) {
            return fail("icmp protocol smoke probe payload mismatch\n", 10);
        }
    }

    net::icmp::echo::Client client{net::IpAddress::ipv4_any(), server_ip};
    net::icmp::echo::AutoReplyServer server{};
    ClientState client_state{};
    ServerState server_state{};
    client.set_reply_handler(&ClientState::on_reply, &client_state);
    client.set_timeout_handler(&ClientState::on_timeout, &client_state);
    client.set_error_handler(&ClientState::on_error, &client_state);
    server.set_request_handler(&ServerState::on_request, &server_state);
    server.set_error_handler(&ServerState::on_error, &server_state);

    auto bound_client = client.bind(client_node.pump);
    auto bound_server = server.bind(server_node.pump);
    if (!bound_client
        || !bound_server
        || !client_node.pump.has_echo_sink()
        || !server_node.pump.has_echo_sink()) {
        return fail("icmp protocol smoke bind failed\n", 11);
    }

    net::icmp::echo::Client rebound_client{};
    auto rebound = rebound_client.bind(client_node.pump, net::IpAddress::ipv4_any(), server_ip);
    if (!rebound
        || !rebound_client.configured()
        || !same_ipv4(rebound_client.peer_address(), server_ip)) {
        return fail("icmp protocol smoke rebound client bind failed\n", 12);
    }
    auto rebound_restore = client.bind(client_node.pump);
    if (!rebound_restore || !client_node.pump.has_echo_sink()) {
        return fail("icmp protocol smoke rebound restore failed\n", 13);
    }

    auto sent = client.ping(0x1357u, 0x0009u, net::ByteView{payload, sizeof(payload)});
    if (!sent
        || sent.value() != net::IcmpSendDisposition::queued
        || client.request_count() != 1
        || client.queued_count() != 1
        || client.transmitted_count() != 0
        || client.last_error() != net::errc::ok
        || client_state.error_calls != 0
        || client_node.link.tx_calls != 1
        || client_node.pump.pending_count() != 1
        || client_node.pump.arp().request_count() != 1) {
        return fail("icmp protocol smoke initial send failed\n", 14);
    }

    if (!drive_until_idle(client_node, server_node)) {
        return fail("icmp protocol smoke exchange stalled\n", 15);
    }

    auto client_peer_mac = client_node.pump.arp().table().lookup(server_ip);
    auto server_peer_mac = server_node.pump.arp().table().lookup(client_ip);
    if (!client_peer_mac
        || !server_peer_mac
        || !same_mac(client_peer_mac.value(), server_mac)
        || !same_mac(server_peer_mac.value(), client_mac)) {
        return fail("icmp protocol smoke arp resolve mismatch\n", 16);
    }

    if (!client_state.got_reply
        || client_state.error_calls != 0
        || client_state.last_error != net::errc::ok
        || client_state.reply_calls != 1
        || client.reply_count() != 1
        || client.drop_count() != 0
        || client.timeout_count() != 0
        || client.pending_count() != 0
        || client.transmitted_count() != 0
        || !same_ipv4(client_state.info.local, client_ip)
        || !same_ipv4(client_state.info.peer, server_ip)
        || client_state.info.identifier != 0x1357u
        || client_state.info.sequence != 0x0009u
        || !bytes_eq(payload, client_state.payload, client_state.payload_size)) {
        return fail("icmp protocol smoke client reply mismatch\n", 17);
    }

    auto tracked = client.ping(net::ByteView{payload, sizeof(payload)}, 40, 20);
    if (!tracked
        || tracked.value().disposition != net::IcmpSendDisposition::transmitted
        || tracked.value().info.identifier == 0u
        || tracked.value().info.sequence == 0u
        || client.pending_count() != 1
        || client.request_count() != 2
        || client.queued_count() != 1
        || client.transmitted_count() != 1
        || client_state.reply_calls != 1
        || client_node.link.tx_calls != 3) {
        return fail("icmp protocol smoke tracked send failed\n", 18);
    }

    if (!drive_until_idle(client_node, server_node)) {
        return fail("icmp protocol smoke tracked exchange stalled\n", 19);
    }

    if (client.pending_count() != 0
        || client.reply_count() != 2
        || client_state.reply_calls != 2
        || client_state.error_calls != 0
        || client.timeout_count() != 0
        || client.drop_count() != 0
        || client_state.info.identifier != tracked.value().info.identifier
        || client_state.info.sequence != tracked.value().info.sequence
        || !bytes_eq(payload, client_state.payload, client_state.payload_size)
        || server.request_count() != 2
        || server.reply_count() != 2
        || server.transmitted_count() != 2
        || server_state.info.identifier != tracked.value().info.identifier
        || server_state.info.sequence != tracked.value().info.sequence) {
        return fail("icmp protocol smoke tracked reply mismatch\n", 20);
    }

    auto scoped_ping = client.ping(
        net::ByteView{payload, sizeof(payload)},
        60,
        20,
        &ClientState::on_scoped_reply,
        nullptr,
        &client_state);
    if (!scoped_ping
        || scoped_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || client.pending_count() != 1
        || client.request_count() != 3
        || client.transmitted_count() != 2
        || client_state.reply_calls != 2
        || client_state.scoped_reply_calls != 0
        || client_node.link.tx_calls != 4) {
        return fail("icmp protocol smoke scoped send failed\n", 21);
    }

    if (!drive_until_idle(client_node, server_node)) {
        return fail("icmp protocol smoke scoped exchange stalled\n", 22);
    }

    if (client.pending_count() != 0
        || client.reply_count() != 3
        || client_state.reply_calls != 2
        || !client_state.got_scoped_reply
        || client_state.scoped_reply_calls != 1
        || client_state.scoped_info.identifier != scoped_ping.value().info.identifier
        || client_state.scoped_info.sequence != scoped_ping.value().info.sequence
        || !bytes_eq(payload, client_state.scoped_payload, client_state.scoped_payload_size)
        || server.request_count() != 3
        || server.reply_count() != 3
        || server.transmitted_count() != 3
        || server_state.info.identifier != scoped_ping.value().info.identifier
        || server_state.info.sequence != scoped_ping.value().info.sequence) {
        return fail("icmp protocol smoke scoped reply mismatch\n", 23);
    }

    server_node.link.peer = nullptr;
    auto timeout_ping = client.ping(net::ByteView{payload, sizeof(payload)}, 90, 10);
    if (!timeout_ping
        || timeout_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || client.pending_count() != 1
        || client.request_count() != 4
        || client.transmitted_count() != 3
        || client_state.timeout_calls != 0
        || client_node.link.tx_calls != 5) {
        return fail("icmp protocol smoke timeout send failed\n", 24);
    }

    if (!drive_until_idle(client_node, server_node)) {
        return fail("icmp protocol smoke timeout exchange stalled\n", 25);
    }

    if (client.pending_count() != 1
        || client.timeout_count() != 0
        || client_state.got_timeout
        || client_state.timeout_calls != 0
        || server.request_count() != 4
        || server.reply_count() != 4
        || server.transmitted_count() != 4
        || server_state.info.identifier != timeout_ping.value().info.identifier
        || server_state.info.sequence != timeout_ping.value().info.sequence) {
        return fail("icmp protocol smoke timeout precheck failed\n", 26);
    }

    client.tick(99);
    if (client.pending_count() != 1
        || client.timeout_count() != 0
        || client_state.got_timeout
        || client_state.timeout_calls != 0) {
        return fail("icmp protocol smoke timeout early tick failed\n", 27);
    }

    client.tick(100);
    if (client.pending_count() != 0
        || client.timeout_count() != 1
        || !client_state.got_timeout
        || client_state.timeout_calls != 1
        || client_state.error_calls != 0
        || client.last_error() != net::errc::ok
        || client_state.timeout_info.identifier != timeout_ping.value().info.identifier
        || client_state.timeout_info.sequence != timeout_ping.value().info.sequence) {
        return fail("icmp protocol smoke timeout handling failed\n", 28);
    }

    server_node.link.peer = &client_node.link;
    auto late_reply = server_node.pump.send(
        server_ip,
        client_ip,
        net::IcmpType::echo_reply,
        timeout_ping.value().info.identifier,
        timeout_ping.value().info.sequence,
        net::ByteView{payload, sizeof(payload)});
    if (!late_reply
        || late_reply.value() != net::IcmpSendDisposition::transmitted
        || !client_node.link.has_rx()) {
        return fail("icmp protocol smoke late reply send failed\n", 29);
    }

    if (!drive_until_idle(client_node, server_node, 4)) {
        return fail("icmp protocol smoke late reply stalled\n", 30);
    }

    client_node.link.peer = nullptr;
    auto scoped_timeout_ping = client.ping(
        net::ByteView{payload, sizeof(payload)},
        120,
        10,
        nullptr,
        &ClientState::on_scoped_timeout,
        &client_state);
    if (!scoped_timeout_ping
        || scoped_timeout_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || client.pending_count() != 1
        || client.request_count() != 5
        || client.transmitted_count() != 4
        || client_state.scoped_timeout_calls != 0
        || client_node.link.tx_calls != 6) {
        return fail("icmp protocol smoke scoped timeout send failed\n", 31);
    }

    client.tick(129);
    if (client.pending_count() != 1
        || client.timeout_count() != 1
        || client_state.got_scoped_timeout
        || client_state.scoped_timeout_calls != 0) {
        return fail("icmp protocol smoke scoped timeout early tick failed\n", 32);
    }

    client.tick(130);
    if (client.pending_count() != 0
        || client.timeout_count() != 2
        || !client_state.got_scoped_timeout
        || client_state.scoped_timeout_calls != 1
        || client_state.timeout_calls != 1
        || client_state.scoped_timeout_info.identifier != scoped_timeout_ping.value().info.identifier
        || client_state.scoped_timeout_info.sequence != scoped_timeout_ping.value().info.sequence) {
        return fail("icmp protocol smoke scoped timeout mismatch\n", 33);
    }

    auto cancelled_ping = client.ping(net::ByteView{payload, sizeof(payload)}, 150, 30);
    if (!cancelled_ping
        || cancelled_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || client.pending_count() != 1
        || client.request_count() != 6
        || client.transmitted_count() != 5
        || !client.cancel(cancelled_ping.value())
        || client.pending_count() != 0
        || client.cancel(cancelled_ping.value())
        || client_node.link.tx_calls != 7) {
        return fail("icmp protocol smoke cancel send failed\n", 34);
    }

    server_node.link.peer = &client_node.link;
    auto cancelled_reply = server_node.pump.send(
        server_ip,
        client_ip,
        net::IcmpType::echo_reply,
        cancelled_ping.value().info.identifier,
        cancelled_ping.value().info.sequence,
        net::ByteView{payload, sizeof(payload)});
    if (!cancelled_reply
        || cancelled_reply.value() != net::IcmpSendDisposition::transmitted
        || !client_node.link.has_rx()) {
        return fail("icmp protocol smoke cancel late reply send failed\n", 35);
    }

    if (!drive_until_idle(client_node, server_node, 4)) {
        return fail("icmp protocol smoke cancel late reply stalled\n", 36);
    }

    if (!server_state.saw_request
        || server_state.error_calls != 0
        || server_state.last_error != net::errc::ok
        || server.request_count() != 4
        || server.reply_count() != 4
        || server.drop_count() != 0
        || server.transmitted_count() != 4
        || server.queued_count() != 0
        || !same_ipv4(server_state.info.local, server_ip)
        || !same_ipv4(server_state.info.peer, client_ip)
        || server_state.info.identifier != timeout_ping.value().info.identifier
        || server_state.info.sequence != timeout_ping.value().info.sequence
        || !bytes_eq(payload, server_state.payload, server_state.payload_size)) {
        return fail("icmp protocol smoke server request mismatch\n", 37);
    }

    if (client.reply_count() != 3
        || client.drop_count() != 2
        || client.pending_count() != 0
        || client.timeout_count() != 2
        || client_state.reply_calls != 2
        || client_state.scoped_reply_calls != 1
        || client_state.timeout_calls != 1
        || client_state.scoped_timeout_calls != 1
        || client_state.error_calls != 0
        || client.last_error() != net::errc::ok
        || client_state.info.identifier != tracked.value().info.identifier
        || client_state.info.sequence != tracked.value().info.sequence
        || client_node.pump.pending_count() != 0
        || server_node.pump.pending_count() != 0
        || client_node.link.has_rx()
        || server_node.link.has_rx()
        || client_node.link.tx_calls != 7
        || server_node.link.tx_calls != 7) {
        return fail("icmp protocol smoke wire state mismatch\n", 38);
    }

    probe_server_node.link.peer = nullptr;
    auto probe_timeout = probe.ping(net::ByteView{payload, sizeof(payload)}, 40, 10);
    auto probe_busy = probe.ping(net::ByteView{payload, sizeof(payload)}, 41, 10);
    const auto probe_timeout_pending_result = probe.result();
    if (!probe_timeout
        || probe_timeout.value().disposition != net::IcmpSendDisposition::transmitted
        || probe_busy
        || probe_busy.error() != net::errc::busy
        || !probe_timeout_pending_result.pending()
        || probe_timeout_pending_result.ready()
        || probe_timeout_pending_result.ok()
        || probe_timeout_pending_result.has_value()
        || probe_timeout_pending_result.timed_out()
        || probe_timeout_pending_result.cancelled()
        || probe_timeout_pending_result.failed()
        || probe_timeout_pending_result.identifier() != probe_timeout.value().info.identifier
        || probe_timeout_pending_result.sequence() != probe_timeout.value().info.sequence
        || probe_timeout_pending_result.has_payload()
        || probe_timeout_pending_result.payload_size() != 0
        || probe_timeout_pending_result.value_payload().size() != 0
        || probe.pending_count() != 1
        || probe.request_count() != 2
        || probe.transmitted_count() != 1
        || probe.timeout_count() != 0
        || probe.error_count() != 0
        || probe_client_node.link.tx_calls != 3) {
        return fail("icmp protocol smoke probe timeout send failed\n", 39);
    }

    if (!drive_until_idle(probe_client_node, probe_server_node)) {
        return fail("icmp protocol smoke probe timeout exchange stalled\n", 40);
    }

    if (!probe.result().pending()
        || probe.pending_count() != 1
        || probe.has_timeout()
        || probe_server.request_count() != 2
        || probe_server.reply_count() != 2
        || probe_server.transmitted_count() != 2) {
        return fail("icmp protocol smoke probe timeout precheck failed\n", 41);
    }

    probe.tick(49);
    if (!probe.result().pending()
        || probe.pending_count() != 1
        || probe.timeout_count() != 0
        || probe.has_timeout()) {
        return fail("icmp protocol smoke probe timeout early tick failed\n", 42);
    }

    probe.tick(50);
    const auto probe_timeout_result = probe.result();
    if (!probe.has_timeout()
        || probe.has_reply()
        || !probe.has_result()
        || !probe_timeout_result.ready()
        || !probe_timeout_result.timed_out()
        || probe_timeout_result.pending()
        || probe_timeout_result.ok()
        || probe_timeout_result.has_value()
        || probe_timeout_result.cancelled()
        || probe_timeout_result.failed()
        || probe_timeout_result.error != net::errc::ok
        || probe_timeout_result.identifier() != probe_timeout.value().info.identifier
        || probe_timeout_result.sequence() != probe_timeout.value().info.sequence
        || probe_timeout_result.has_payload()
        || probe_timeout_result.payload_size() != 0
        || probe_timeout_result.value_payload().size() != 0
        || probe.pending_count() != 0
        || probe.timeout_count() != 1
        || probe.error_count() != 0) {
        return fail("icmp protocol smoke probe timeout mismatch\n", 43);
    }

    probe_server_node.link.peer = &probe_client_node.link;
    auto probe_late_reply = probe_server_node.pump.send(
        server_ip,
        client_ip,
        net::IcmpType::echo_reply,
        probe_timeout.value().info.identifier,
        probe_timeout.value().info.sequence,
        net::ByteView{payload, sizeof(payload)});
    if (!probe_late_reply
        || probe_late_reply.value() != net::IcmpSendDisposition::transmitted
        || !probe_client_node.link.has_rx()) {
        return fail("icmp protocol smoke probe late reply send failed\n", 44);
    }

    if (!drive_until_idle(probe_client_node, probe_server_node, 4)) {
        return fail("icmp protocol smoke probe late reply stalled\n", 45);
    }

    const auto probe_late_result = probe.result();
    if (probe.drop_count() != 1
        || !probe_late_result.ready()
        || !probe_late_result.timed_out()
        || probe_late_result.ok()
        || probe_late_result.has_value()
        || probe_late_result.cancelled()
        || probe_late_result.failed()
        || probe_late_result.error != net::errc::ok
        || probe_late_result.identifier() != probe_timeout_result.identifier()
        || probe_late_result.sequence() != probe_timeout_result.sequence()
        || probe_late_result.has_payload()
        || probe_late_result.payload_size() != 0
        || probe_late_result.value_payload().size() != 0
        || probe.error_count() != 0) {
        return fail("icmp protocol smoke probe late reply mismatch\n", 46);
    }

    auto probe_cancel = probe.ping(net::ByteView{payload, sizeof(payload)}, 60, 10);
    if (!probe_cancel
        || probe_cancel.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.has_pending()
        || !probe.pending()
        || probe.ready()
        || probe.ok()
        || probe.cancelled()
        || probe.failed()
        || probe.has_value()
        || probe.identifier() != probe_cancel.value().info.identifier
        || probe.sequence() != probe_cancel.value().info.sequence
        || probe.payload_size() != 0
        || probe.has_payload()
        || probe.value_payload().size() != 0
        || probe.pending_count() != 1
        || probe.request_count() != 3
        || probe.transmitted_count() != 2
        || probe.error_count() != 0
        || !probe.cancel()
        || probe.has_pending()
        || probe.pending()
        || !probe.ready()
        || probe.ok()
        || probe.timed_out()
        || !probe.cancelled()
        || probe.failed()
        || probe.has_value()
        || probe.identifier() != probe_cancel.value().info.identifier
        || probe.sequence() != probe_cancel.value().info.sequence
        || probe.payload_size() != 0
        || probe.has_payload()
        || probe.value_payload().size() != 0
        || !probe.has_result()
        || probe.pending_count() != 0
        || probe.cancel()
        || probe.error_count() != 0) {
        return fail("icmp protocol smoke probe cancel mismatch\n", 47);
    }

    if (!drive_until_idle(probe_client_node, probe_server_node, 4)) {
        return fail("icmp protocol smoke probe cancel late reply stalled\n", 48);
    }

    const auto probe_cancel_result = probe.result();
    if (probe.drop_count() != 2
        || !probe_cancel_result.ready()
        || !probe_cancel_result.cancelled()
        || probe_cancel_result.ok()
        || probe_cancel_result.timed_out()
        || probe_cancel_result.failed()
        || probe_cancel_result.has_value()
        || probe_cancel_result.error != net::errc::ok
        || probe_cancel_result.identifier() != probe_cancel.value().info.identifier
        || probe_cancel_result.sequence() != probe_cancel.value().info.sequence
        || probe_cancel_result.payload_size() != 0
        || probe_cancel_result.has_payload()
        || probe_cancel_result.value_payload().size() != 0
        || !probe.ready()
        || !probe.cancelled()
        || probe.failed()
        || probe.has_value()
        || probe.identifier() != probe_cancel.value().info.identifier
        || probe.sequence() != probe_cancel.value().info.sequence
        || probe.payload_size() != 0
        || probe.has_payload()
        || probe.value_payload().size() != 0
        || probe_server.request_count() != 3
        || probe_server.reply_count() != 3
        || probe_server.transmitted_count() != 3
        || probe.error_count() != 0) {
        return fail("icmp protocol smoke probe cancel late reply mismatch\n", 49);
    }

    auto probe_cancel_all = probe.ping(net::ByteView{payload, sizeof(payload)}, 70, 10);
    if (!probe_cancel_all
        || probe_cancel_all.value().disposition != net::IcmpSendDisposition::transmitted
        || !probe.has_pending()
        || !probe.pending()
        || probe.ready()
        || probe.ok()
        || probe.cancelled()
        || probe.failed()
        || probe.has_value()
        || probe.identifier() != probe_cancel_all.value().info.identifier
        || probe.sequence() != probe_cancel_all.value().info.sequence
        || probe.payload_size() != 0
        || probe.has_payload()
        || probe.value_payload().size() != 0
        || probe.pending_count() != 1
        || probe.request_count() != 4
        || probe.transmitted_count() != 3
        || probe.drop_count() != 2
        || probe.error_count() != 0) {
        return fail("icmp protocol smoke probe cancel_all send failed\n", 50);
    }

    probe.cancel_all();
    if (probe.has_pending()
        || probe.pending()
        || !probe.ready()
        || probe.ok()
        || probe.timed_out()
        || !probe.cancelled()
        || probe.failed()
        || probe.has_value()
        || probe.identifier() != probe_cancel_all.value().info.identifier
        || probe.sequence() != probe_cancel_all.value().info.sequence
        || probe.payload_size() != 0
        || probe.has_payload()
        || probe.value_payload().size() != 0
        || !probe.has_result()
        || probe.pending_count() != 0
        || probe.drop_count() != 2
        || probe.error_count() != 0) {
        return fail("icmp protocol smoke probe cancel_all mismatch\n", 51);
    }

    if (!drive_until_idle(probe_client_node, probe_server_node, 4)) {
        return fail("icmp protocol smoke probe cancel_all late reply stalled\n", 52);
    }

    const auto probe_cancel_all_result = probe.result();
    if (probe.drop_count() != 3
        || !probe_cancel_all_result.ready()
        || !probe_cancel_all_result.cancelled()
        || probe_cancel_all_result.ok()
        || probe_cancel_all_result.timed_out()
        || probe_cancel_all_result.failed()
        || probe_cancel_all_result.has_value()
        || probe_cancel_all_result.error != net::errc::ok
        || probe_cancel_all_result.identifier() != probe_cancel_all.value().info.identifier
        || probe_cancel_all_result.sequence() != probe_cancel_all.value().info.sequence
        || probe_cancel_all_result.payload_size() != 0
        || probe_cancel_all_result.has_payload()
        || probe_cancel_all_result.value_payload().size() != 0
        || !probe.ready()
        || !probe.cancelled()
        || probe.failed()
        || probe.has_value()
        || probe.identifier() != probe_cancel_all.value().info.identifier
        || probe.sequence() != probe_cancel_all.value().info.sequence
        || probe.payload_size() != 0
        || probe.has_payload()
        || probe.value_payload().size() != 0
        || probe_server.request_count() != 4
        || probe_server.reply_count() != 4
        || probe_server.transmitted_count() != 4
        || probe_server.drop_count() != 0
        || probe.error_count() != 0) {
        return fail("icmp protocol smoke probe cancel_all late reply mismatch\n", 53);
    }

    net::icmp::echo::Probe<2> overflow_probe{net::IpAddress::ipv4_any(), server_ip};
    auto overflow_bound = overflow_probe.bind(probe_client_node.pump);
    if (!overflow_bound || !probe_client_node.pump.has_echo_sink()) {
        return fail("icmp protocol smoke overflow probe bind failed\n", 54);
    }

    auto overflow_ping = overflow_probe.ping(net::ByteView{payload, sizeof(payload)}, 80, 20);
    if (!overflow_ping
        || overflow_ping.value().disposition != net::IcmpSendDisposition::transmitted
        || !overflow_probe.has_pending()
        || !overflow_probe.pending()
        || overflow_probe.ready()
        || overflow_probe.ok()
        || overflow_probe.timed_out()
        || overflow_probe.cancelled()
        || overflow_probe.failed()
        || overflow_probe.has_value()
        || overflow_probe.identifier() != overflow_ping.value().info.identifier
        || overflow_probe.sequence() != overflow_ping.value().info.sequence
        || overflow_probe.payload_size() != 0
        || overflow_probe.has_payload()
        || overflow_probe.value_payload().size() != 0
        || overflow_probe.pending_count() != 1
        || overflow_probe.request_count() != 1
        || overflow_probe.transmitted_count() != 1
        || overflow_probe.error_count() != 0) {
        return fail("icmp protocol smoke overflow probe send failed\n", 55);
    }

    bool saw_overflow = false;
    for (util::usize step = 0; step < 8; ++step) {
        auto client_progress = probe_client_node.pump.service(1);
        if (!client_progress) {
            if (client_progress.error() != net::errc::buffer_overflow) {
                return fail("icmp protocol smoke overflow probe wrong error\n", 56);
            }
            saw_overflow = true;
            break;
        }

        auto server_progress = probe_server_node.pump.service(1);
        if (!server_progress) {
            return fail("icmp protocol smoke overflow probe server stalled\n", 57);
        }
    }

    const auto overflow_result = overflow_probe.result();
    if (!saw_overflow
        || overflow_probe.has_pending()
        || overflow_probe.pending()
        || !overflow_probe.ready()
        || overflow_probe.ok()
        || overflow_probe.timed_out()
        || overflow_probe.cancelled()
        || !overflow_probe.failed()
        || overflow_probe.has_value()
        || overflow_probe.observed_error() != net::errc::buffer_overflow
        || overflow_probe.identifier() != overflow_ping.value().info.identifier
        || overflow_probe.sequence() != overflow_ping.value().info.sequence
        || overflow_probe.payload_size() != 0
        || overflow_probe.has_payload()
        || overflow_probe.value_payload().size() != 0
        || !overflow_probe.has_result()
        || overflow_probe.pending_count() != 0
        || overflow_probe.reply_count() != 1
        || overflow_probe.timeout_count() != 0
        || overflow_probe.drop_count() != 0
        || overflow_probe.error_count() != 1
        || !overflow_result.ready()
        || !overflow_result.failed()
        || overflow_result.ok()
        || overflow_result.timed_out()
        || overflow_result.cancelled()
        || overflow_result.has_value()
        || overflow_result.error != net::errc::buffer_overflow
        || overflow_result.identifier() != overflow_ping.value().info.identifier
        || overflow_result.sequence() != overflow_ping.value().info.sequence
        || overflow_result.payload_size() != 0
        || overflow_result.has_payload()
        || overflow_result.value_payload().size() != 0
        || probe_server.request_count() != 5
        || probe_server.reply_count() != 5
        || probe_server.transmitted_count() != 5
        || probe_server.drop_count() != 0) {
        return fail("icmp protocol smoke overflow probe mismatch\n", 58);
    }

    std::puts("net icmp protocol smoke: ok");
    return 0;
}
