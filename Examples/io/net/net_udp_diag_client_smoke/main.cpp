#include <array>
#include <cstdio>

import charm.net;
import net.arp;
import net.driver;
import net.ether;
import net.ipv4;
import net.packet;
import net.protocol.diagnostic_udp;
import util.core;
import util.expected;

namespace {
    struct StubLinkDriver {
        net::OwnedPacketSinkRef input_sink{};
        net::PacketPool<2, 192> rx_pool{};
        std::array<util::u8, 192> rx_bytes{};
        util::usize rx_size{0};
        bool rx_ready{false};

        std::array<util::u8, 192> tx_bytes{};
        util::usize tx_size{0};
        util::usize tx_calls{0};

        [[nodiscard]] net::NetDriverInfo info() const noexcept {
            return net::NetDriverInfo{
                .mtu = 192,
                .mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u),
                .capabilities = net::NetIfCapability::rx
                    | net::NetIfCapability::tx
                    | net::NetIfCapability::broadcast
            };
        }

        [[nodiscard]] net::Result<void> set_input_sink(net::OwnedPacketSinkRef sink) noexcept {
            input_sink = sink;
            return {};
        }

        [[nodiscard]] net::Result<void> poll() noexcept {
            if (!rx_ready) {
                return {};
            }
            if (!input_sink.valid()) {
                return util::unexpected(net::errc::bad_state);
            }

            auto lease = rx_pool.acquire();
            if (!lease) {
                return util::unexpected(lease.error());
            }
            auto appended = lease.value()->append(net::ByteView{rx_bytes.data(), rx_size});
            if (!appended) {
                return util::unexpected(appended.error());
            }

            rx_ready = false;
            return input_sink.consume(net::OwnedPacket{
                static_cast<net::PacketPool<2, 192>::Lease&&>(lease.value())
            });
        }

        [[nodiscard]] net::Result<void> transmit(net::PacketView packet) noexcept {
            if (packet.size() > tx_bytes.size()) {
                return util::unexpected(net::errc::buffer_overflow);
            }
            for (util::usize i = 0; i < packet.size(); ++i) {
                tx_bytes[i] = packet[i];
            }
            tx_size = packet.size();
            ++tx_calls;
            return {};
        }

        void queue_rx(net::ByteView packet) noexcept {
            rx_size = packet.size();
            rx_ready = true;
            for (util::usize i = 0; i < packet.size(); ++i) {
                rx_bytes[i] = packet[i];
            }
        }
    };

    template <util::usize Capacity>
    [[nodiscard]] net::Result<void> write_ether_frame(net::PacketBuffer<Capacity>& packet,
                                                      net::MacAddress destination,
                                                      net::MacAddress source,
                                                      net::EtherType type,
                                                      net::ByteView payload,
                                                      net::ByteView padding = {}) noexcept {
        auto reset = packet.reset();
        if (!reset) {
            return util::unexpected(reset.error());
        }

        std::array<util::u8, 14> header{};
        for (util::usize i = 0; i < 6; ++i) {
            header[i] = destination.bytes[i];
            header[6 + i] = source.bytes[i];
        }
        const auto raw_type = net::ether_type_value(type);
        header[12] = static_cast<util::u8>((raw_type >> 8) & 0xFFu);
        header[13] = static_cast<util::u8>(raw_type & 0xFFu);

        auto appended_header = packet.append(net::ByteView{header.data(), header.size()});
        if (!appended_header) {
            return util::unexpected(appended_header.error());
        }

        auto appended_payload = packet.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }

        auto appended_padding = packet.append(padding);
        if (!appended_padding) {
            return util::unexpected(appended_padding.error());
        }
        return {};
    }

    [[nodiscard]] bool same_ipv4(const net::IpAddress& lhs, const net::IpAddress& rhs) noexcept {
        return net::is_same_ipv4_address(lhs, rhs);
    }

    [[nodiscard]] bool same_mac(const net::MacAddress& lhs, const net::MacAddress& rhs) noexcept {
        return net::is_same_mac(lhs, rhs);
    }

    struct ClientState {
        util::u16 ping_request_id{0};
        util::u16 count_request_id{0};
        util::u16 meta_request_id{0};
        util::u16 slow_request_id{0};
        bool got_ping{false};
        bool got_count_error{false};
        bool got_timeout{false};
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

            self->got_count_error = request_id == self->count_request_id
                && status == net::diag::udp::Status::bad_request
                && response.value == 0;
            if (!self->got_count_error) {
                self->failed = true;
            }
        }

        static void on_timeout(void* ctx, util::u16 request_id) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->got_timeout = request_id == self->meta_request_id;
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

}

int main() {
    constexpr auto local_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto local_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto peer_mac = net::MacAddress::from_bytes(0x02u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu);
    constexpr auto peer_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    constexpr auto local = net::Endpoint::ipv4_any(9001);
    constexpr auto local_wire = net::Endpoint::ipv4(10, 0, 0, 2, 9001);
    constexpr auto peer = net::Endpoint::ipv4(10, 0, 0, 9, 7001);

    net::NetIf netif{};
    auto configured = netif.configure(net::NetIfConfig{
        .mtu = 192,
        .mac = local_mac,
        .address = local_ip,
        .capabilities = net::NetIfCapability::rx
            | net::NetIfCapability::tx
            | net::NetIfCapability::broadcast
    });
    if (!configured) {
        std::fputs("udp diag client smoke netif configure failed\n", stderr);
        return 1;
    }

    StubLinkDriver link{};
    net::NetDriver driver{};
    auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
    if (!attached) {
        std::fputs("udp diag client smoke driver attach failed\n", stderr);
        return 2;
    }

    net::Stack stack{};
    auto registered = stack.register_driver(driver);
    if (!registered || stack.driver_count() != 1 || stack.netif_count() != 1) {
        std::fputs("udp diag client smoke stack register failed\n", stderr);
        return 3;
    }

    auto up = netif.bring_up();
    if (!up) {
        std::fputs("udp diag client smoke netif bring_up failed\n", stderr);
        return 4;
    }

    net::UdpStackPump<192, 4, 192, 4, 64, 4> pump{
        stack,
        netif,
        net::UdpStackPumpConfig{
            .egress = net::UdpEgressPumpConfig{
                .retry_interval_ticks = 2,
                .max_attempts = 2,
            }
        }
    };

    net::diag::udp::EndpointClient<16, 4> client{local, peer};
    ClientState client_state{};
    client.set_error_handler(&ClientState::on_error, &client_state);

    auto bound = client.bind(pump);
    if (!bound || !pump.has_udp_binding(client.local_endpoint().port) || pump.udp_binding_count() != 1) {
        std::fputs("udp diag client smoke bind failed\n", stderr);
        return 5;
    }

    net::PacketBuffer<192> arp_request{};
    auto wrote_arp_request = net::write_arp_ipv4_request_frame(
        arp_request,
        peer_mac,
        peer_ip,
        local_ip);
    if (!wrote_arp_request) {
        std::fputs("udp diag client smoke arp request encode failed\n", stderr);
        return 6;
    }

    link.queue_rx(arp_request.view().payload);
    auto primed = pump.service(0);
    if (!primed
        || !primed.value().polled_links
        || primed.value().ipv4_delivered != 0
        || primed.value().udp_delivered != 0
        || primed.value().egress_flushed != 0
        || link.tx_calls != 1
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("udp diag client smoke arp priming failed\n", stderr);
        return 7;
    }

    auto arp_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    auto cached_peer = pump.arp().table().lookup(peer_ip);
    if (!arp_frame
        || arp_frame.value().type != net::EtherType::arp
        || !cached_peer
        || !same_mac(cached_peer.value(), peer_mac)) {
        std::fputs("udp diag client smoke arp cache seed failed\n", stderr);
        return 8;
    }

    auto ping = client.ping(
        net::diag::PingRequest{{'p', 'i', 'n', 'g'}},
        10,
        50,
        &ClientState::on_ping,
        nullptr,
        &client_state);
    if (!ping
        || client.pending_count() != 1
        || client.request_count() != 1
        || client.queued_count() != 0
        || link.tx_calls != 2) {
        std::fputs("udp diag client smoke ping send failed\n", stderr);
        return 9;
    }
    client_state.ping_request_id = ping.value();

    auto ping_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!ping_frame
        || ping_frame.value().type != net::EtherType::ipv4
        || !same_mac(ping_frame.value().destination, peer_mac)
        || !same_mac(ping_frame.value().source, local_mac)) {
        std::fputs("udp diag client smoke ping ether mismatch\n", stderr);
        return 10;
    }

    auto ping_ipv4 = net::parse_ipv4_packet(ping_frame.value().payload);
    if (!ping_ipv4
        || ping_ipv4.value().protocol != net::Ipv4Protocol::udp
        || !same_ipv4(ping_ipv4.value().source, local_ip)
        || !same_ipv4(ping_ipv4.value().destination, peer_ip)) {
        std::fputs("udp diag client smoke ping ipv4 mismatch\n", stderr);
        return 11;
    }

    auto ping_udp = net::parse_udp_datagram(ping_ipv4.value().payload);
    auto ping_diag = net::diag::udp::parse_datagram(ping_udp.value().payload);
    auto ping_request = net::diag::udp::decode_request<net::diag::PingOp>(ping_diag.value());
    if (!ping_udp
        || ping_udp.value().source_port != local_wire.port
        || ping_udp.value().destination_port != peer.port
        || !ping_diag
        || ping_diag.value().header.kind != net::diag::udp::Kind::request
        || ping_diag.value().header.opcode != net::diag::PingOp::opcode
        || ping_diag.value().header.request_id != client_state.ping_request_id
        || !ping_request
        || ping_request.value().text[0] != 'p'
        || ping_request.value().text[1] != 'i'
        || ping_request.value().text[2] != 'n'
        || ping_request.value().text[3] != 'g') {
        std::fputs("udp diag client smoke ping request wire mismatch\n", stderr);
        return 12;
    }

    net::PacketBuffer<64> ping_reply_datagram{};
    auto wrote_ping_reply = net::diag::udp::write_response_datagram<net::diag::PingOp>(
        ping_reply_datagram,
        client_state.ping_request_id,
        net::diag::udp::Status::ok,
        net::diag::PingReply{{'p', 'o', 'n', 'g'}});
    if (!wrote_ping_reply) {
        std::fputs("udp diag client smoke ping reply encode failed\n", stderr);
        return 13;
    }

    net::PacketBuffer<128> ping_reply_udp{};
    auto wrote_ping_reply_udp = net::write_udp_ipv4_datagram(
        ping_reply_udp,
        peer,
        local_wire,
        ping_reply_datagram.view().payload);
    if (!wrote_ping_reply_udp) {
        std::fputs("udp diag client smoke ping reply udp encode failed\n", stderr);
        return 14;
    }

    net::PacketBuffer<160> ping_reply_ipv4{};
    auto wrote_ping_reply_ipv4 = net::write_ipv4_packet(
        ping_reply_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x3101u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 48,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer.address,
            .destination = local_wire.address,
        },
        ping_reply_udp.view().payload);
    if (!wrote_ping_reply_ipv4) {
        std::fputs("udp diag client smoke ping reply ipv4 encode failed\n", stderr);
        return 15;
    }

    net::PacketBuffer<192> ping_reply_frame{};
    auto wrote_ping_reply_frame = write_ether_frame(
        ping_reply_frame,
        net::MacAddress::broadcast(),
        peer_mac,
        net::EtherType::ipv4,
        ping_reply_ipv4.view().payload);
    if (!wrote_ping_reply_frame) {
        std::fputs("udp diag client smoke ping reply ether encode failed\n", stderr);
        return 16;
    }

    link.queue_rx(ping_reply_frame.view().payload);
    auto ping_progress = pump.service(0);
    if (!ping_progress
        || ping_progress.value().ipv4_delivered != 1
        || ping_progress.value().udp_delivered != 1
        || client.pending_count() != 0
        || client.response_count() != 1
        || !client_state.got_ping
        || client_state.failed) {
        std::fputs("udp diag client smoke ping reply handling failed\n", stderr);
        return 17;
    }

    auto count = client.query_count(
        20,
        40,
        &ClientState::on_count,
        nullptr,
        &client_state);
    if (!count
        || client.pending_count() != 1
        || client.request_count() != 2
        || link.tx_calls != 3) {
        std::fputs("udp diag client smoke count send failed\n", stderr);
        return 18;
    }
    client_state.count_request_id = count.value();

    net::PacketBuffer<64> count_error_datagram{};
    auto wrote_count_error = net::diag::udp::write_error_datagram(
        count_error_datagram,
        net::diag::CountOp::opcode,
        client_state.count_request_id,
        net::diag::udp::Status::bad_request);
    if (!wrote_count_error) {
        std::fputs("udp diag client smoke count error encode failed\n", stderr);
        return 19;
    }

    net::PacketBuffer<128> count_error_udp{};
    auto wrote_count_error_udp = net::write_udp_ipv4_datagram(
        count_error_udp,
        peer,
        local_wire,
        count_error_datagram.view().payload);
    if (!wrote_count_error_udp) {
        std::fputs("udp diag client smoke count error udp encode failed\n", stderr);
        return 20;
    }

    net::PacketBuffer<160> count_error_ipv4{};
    auto wrote_count_error_ipv4 = net::write_ipv4_packet(
        count_error_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x3102u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 47,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer.address,
            .destination = local_wire.address,
        },
        count_error_udp.view().payload);
    if (!wrote_count_error_ipv4) {
        std::fputs("udp diag client smoke count error ipv4 encode failed\n", stderr);
        return 21;
    }

    net::PacketBuffer<192> count_error_frame{};
    auto wrote_count_error_frame = write_ether_frame(
        count_error_frame,
        net::MacAddress::broadcast(),
        peer_mac,
        net::EtherType::ipv4,
        count_error_ipv4.view().payload);
    if (!wrote_count_error_frame) {
        std::fputs("udp diag client smoke count error ether encode failed\n", stderr);
        return 22;
    }

    link.queue_rx(count_error_frame.view().payload);
    auto count_progress = pump.service(0);
    if (!count_progress
        || count_progress.value().ipv4_delivered != 1
        || count_progress.value().udp_delivered != 1
        || client.pending_count() != 0
        || client.response_count() != 2
        || !client_state.got_count_error
        || client_state.failed) {
        std::fputs("udp diag client smoke count error handling failed\n", stderr);
        return 23;
    }

    auto meta = client.query_meta(
        net::diag::MetaRequest{
            .code = 0x1234u,
            .flags = 0x5Au,
            .tag = {'o', 'k'},
        },
        30,
        15,
        nullptr,
        &ClientState::on_timeout,
        &client_state);
    if (!meta
        || client.pending_count() != 1
        || client.request_count() != 3
        || link.tx_calls != 4) {
        std::fputs("udp diag client smoke meta send failed\n", stderr);
        return 24;
    }
    client_state.meta_request_id = meta.value();

    client.tick(44);
    if (client.timeout_count() != 0 || client.pending_count() != 1 || client_state.got_timeout) {
        std::fputs("udp diag client smoke timeout precheck failed\n", stderr);
        return 25;
    }

    client.tick(45);
    if (client.timeout_count() != 1
        || client.pending_count() != 0
        || !client_state.got_timeout
        || client_state.failed) {
        std::fputs("udp diag client smoke timeout handling failed\n", stderr);
        return 26;
    }

    auto slow = client.query_slow_count(
        net::diag::CounterValue{41},
        50,
        20,
        nullptr,
        nullptr,
        nullptr);
    if (!slow
        || client.pending_count() != 1
        || client.request_count() != 4
        || link.tx_calls != 5
        || !client.cancel_request(slow.value())
        || client.pending_count() != 0) {
        std::fputs("udp diag client smoke cancel request failed\n", stderr);
        return 27;
    }
    client_state.slow_request_id = slow.value();

    net::PacketBuffer<64> stray_response_datagram{};
    auto wrote_stray_response = net::diag::udp::write_response_datagram<net::diag::SlowCountOp>(
        stray_response_datagram,
        client_state.slow_request_id,
        net::diag::udp::Status::ok,
        net::diag::CounterValue{42});
    if (!wrote_stray_response) {
        std::fputs("udp diag client smoke stray response encode failed\n", stderr);
        return 28;
    }

    net::PacketBuffer<128> stray_response_udp{};
    auto wrote_stray_response_udp = net::write_udp_ipv4_datagram(
        stray_response_udp,
        peer,
        local_wire,
        stray_response_datagram.view().payload);
    if (!wrote_stray_response_udp) {
        std::fputs("udp diag client smoke stray response udp encode failed\n", stderr);
        return 29;
    }

    net::PacketBuffer<160> stray_response_ipv4{};
    auto wrote_stray_response_ipv4 = net::write_ipv4_packet(
        stray_response_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x3103u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 46,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer.address,
            .destination = local_wire.address,
        },
        stray_response_udp.view().payload);
    if (!wrote_stray_response_ipv4) {
        std::fputs("udp diag client smoke stray response ipv4 encode failed\n", stderr);
        return 30;
    }

    net::PacketBuffer<192> stray_response_frame{};
    auto wrote_stray_response_frame = write_ether_frame(
        stray_response_frame,
        net::MacAddress::broadcast(),
        peer_mac,
        net::EtherType::ipv4,
        stray_response_ipv4.view().payload);
    if (!wrote_stray_response_frame) {
        std::fputs("udp diag client smoke stray response ether encode failed\n", stderr);
        return 31;
    }

    link.queue_rx(stray_response_frame.view().payload);
    auto stray_progress = pump.service(0);
    if (!stray_progress
        || stray_progress.value().ipv4_delivered != 1
        || stray_progress.value().udp_delivered != 1
        || client.drop_count() != 1
        || client_state.error_calls != 0
        || client.last_error() != net::errc::ok) {
        std::fputs("udp diag client smoke stray response handling failed\n", stderr);
        return 32;
    }

    std::puts("net udp diag client smoke: ok");
    return 0;
}
