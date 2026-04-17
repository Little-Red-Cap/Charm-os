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
        net::errc last_error{net::errc::ok};
        util::usize error_calls{0};
        net::IcmpEchoInfo info{};
        std::array<util::u8, 16> payload{};
        util::usize payload_size{0};

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
            return {};
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

    net::icmp::echo::Client client{net::IpAddress::ipv4_any(), server_ip};
    net::icmp::echo::AutoReplyServer server{};
    ClientState client_state{};
    ServerState server_state{};
    client.set_reply_handler(&ClientState::on_reply, &client_state);
    client.set_error_handler(&ClientState::on_error, &client_state);
    server.set_request_handler(&ServerState::on_request, &server_state);
    server.set_error_handler(&ServerState::on_error, &server_state);

    auto bound_client = net::bind_icmp_protocol(client_node.pump, client);
    auto bound_server = net::bind_icmp_protocol(server_node.pump, server);
    if (!bound_client
        || !bound_server
        || !client_node.pump.has_echo_sink()
        || !server_node.pump.has_echo_sink()) {
        return fail("icmp protocol smoke bind failed\n", 4);
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
        return fail("icmp protocol smoke initial send failed\n", 5);
    }

    if (!drive_until_idle(client_node, server_node)) {
        return fail("icmp protocol smoke exchange stalled\n", 6);
    }

    auto client_peer_mac = client_node.pump.arp().table().lookup(server_ip);
    auto server_peer_mac = server_node.pump.arp().table().lookup(client_ip);
    if (!client_peer_mac
        || !server_peer_mac
        || !same_mac(client_peer_mac.value(), server_mac)
        || !same_mac(server_peer_mac.value(), client_mac)) {
        return fail("icmp protocol smoke arp resolve mismatch\n", 7);
    }

    if (!client_state.got_reply
        || client_state.error_calls != 0
        || client_state.last_error != net::errc::ok
        || client.reply_count() != 1
        || client.drop_count() != 0
        || client.transmitted_count() != 0
        || !same_ipv4(client_state.info.local, client_ip)
        || !same_ipv4(client_state.info.peer, server_ip)
        || client_state.info.identifier != 0x1357u
        || client_state.info.sequence != 0x0009u
        || !bytes_eq(payload, client_state.payload, client_state.payload_size)) {
        return fail("icmp protocol smoke client reply mismatch\n", 8);
    }

    if (!server_state.saw_request
        || server_state.error_calls != 0
        || server_state.last_error != net::errc::ok
        || server.request_count() != 1
        || server.reply_count() != 1
        || server.drop_count() != 0
        || server.transmitted_count() != 1
        || server.queued_count() != 0
        || !same_ipv4(server_state.info.local, server_ip)
        || !same_ipv4(server_state.info.peer, client_ip)
        || server_state.info.identifier != 0x1357u
        || server_state.info.sequence != 0x0009u
        || !bytes_eq(payload, server_state.payload, server_state.payload_size)) {
        return fail("icmp protocol smoke server request mismatch\n", 9);
    }

    if (client_node.pump.pending_count() != 0
        || server_node.pump.pending_count() != 0
        || client_node.link.has_rx()
        || server_node.link.has_rx()
        || client_node.link.tx_calls != 2
        || server_node.link.tx_calls != 2) {
        return fail("icmp protocol smoke wire state mismatch\n", 10);
    }

    std::puts("net icmp protocol smoke: ok");
    return 0;
}
