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

    struct ServerState {
        bool saw_ping{false};
        bool saw_count{false};
        bool saw_slow_count{false};
        bool saw_meta{false};

        static net::diag::udp::Status on_ping(void* ctx,
                                              const net::diag::PingRequest& request,
                                              net::diag::PingReply& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return net::diag::udp::Status::internal_error;
            }

            static constexpr util::u8 want[]{'p', 'i', 'n', 'g'};
            static constexpr util::u8 pong[]{'p', 'o', 'n', 'g'};
            self->saw_ping = true;
            for (util::usize i = 0; i < 4; ++i) {
                if (request.text[i] != want[i]) {
                    return net::diag::udp::Status::bad_request;
                }
                response.text[i] = pong[i];
            }
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
            response.value = 7;
            return net::diag::udp::Status::ok;
        }

        static net::diag::udp::Status on_meta(void* ctx,
                                              const net::diag::MetaRequest& request,
                                              net::diag::MetaReply& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return net::diag::udp::Status::internal_error;
            }

            static constexpr std::array<util::u8, 2> want_tag{'o', 'k'};
            if (request.code != 0x1234u || request.flags != 0x5Au || request.tag != want_tag) {
                return net::diag::udp::Status::bad_request;
            }

            self->saw_meta = true;
            response.status = 0xA5u;
            response.reflected_code = static_cast<util::u16>(request.code + 1u);
            response.tag = request.tag;
            return net::diag::udp::Status::ok;
        }

        static net::diag::udp::Status on_slow_count(void* ctx,
                                                    const net::diag::CounterValue& request,
                                                    net::diag::CounterValue& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return net::diag::udp::Status::internal_error;
            }

            if (request.value != 41u) {
                return net::diag::udp::Status::bad_request;
            }

            self->saw_slow_count = true;
            response.value = static_cast<util::u16>(request.value + 1u);
            return net::diag::udp::Status::ok;
        }
    };

}

int main() {
    constexpr auto local_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto local_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto peer_mac = net::MacAddress::from_bytes(0x02u, 0xAAu, 0xBBu, 0xCCu, 0xDDu, 0xEEu);
    constexpr auto peer_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    constexpr auto local_endpoint = net::Endpoint{local_ip, 9000};
    constexpr auto peer_endpoint = net::Endpoint{peer_ip, 7000};

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
        std::fputs("udp diag smoke netif configure failed\n", stderr);
        return 1;
    }

    StubLinkDriver link{};
    net::NetDriver driver{};
    auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
    if (!attached) {
        std::fputs("udp diag smoke driver attach failed\n", stderr);
        return 2;
    }

    net::Stack stack{};
    auto registered = stack.register_driver(driver);
    if (!registered || stack.driver_count() != 1 || stack.netif_count() != 1) {
        std::fputs("udp diag smoke stack register failed\n", stderr);
        return 3;
    }

    auto up = netif.bring_up();
    if (!up) {
        std::fputs("udp diag smoke netif bring_up failed\n", stderr);
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

    net::diag::udp::EndpointServer<16> server{local_endpoint};
    ServerState server_state{};

    auto ping_route = server.on_ping(&ServerState::on_ping, &server_state);
    auto count_route = server.on_count(&ServerState::on_count, &server_state);
    auto meta_route = server.on_meta(&ServerState::on_meta, &server_state);
    auto slow_count_route = server.on_slow_count(&ServerState::on_slow_count, &server_state);
    auto bound = server.bind(pump);
    if (!ping_route
        || !count_route
        || !meta_route
        || !slow_count_route
        || !bound
        || !pump.has_udp_binding(server.local_endpoint().port)
        || pump.udp_binding_count() != 1) {
        std::fputs("udp diag smoke server bind failed\n", stderr);
        return 5;
    }

    net::PacketBuffer<192> arp_request{};
    auto wrote_arp_request = net::write_arp_ipv4_request_frame(
        arp_request,
        peer_mac,
        peer_ip,
        local_ip);
    if (!wrote_arp_request) {
        std::fputs("udp diag smoke arp request encode failed\n", stderr);
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
        || pump.arp().pending_count() != 0
        || link.rx_pool.in_use_count() != 0) {
        std::fputs("udp diag smoke arp priming failed\n", stderr);
        return 7;
    }

    auto arp_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!arp_frame || arp_frame.value().type != net::EtherType::arp) {
        std::fputs("udp diag smoke arp priming ether parse failed\n", stderr);
        return 8;
    }
    auto arp_reply = net::parse_arp_ipv4_ethernet(arp_frame.value().payload);
    if (!arp_reply
        || arp_reply.value().operation != net::ArpOperation::reply
        || !same_mac(arp_reply.value().sender_mac, local_mac)
        || !same_ipv4(arp_reply.value().sender_ip, local_ip)
        || !same_mac(arp_reply.value().target_mac, peer_mac)
        || !same_ipv4(arp_reply.value().target_ip, peer_ip)) {
        std::fputs("udp diag smoke arp priming reply mismatch\n", stderr);
        return 9;
    }

    auto cached_peer = pump.arp().table().lookup(peer_ip);
    if (!cached_peer || !same_mac(cached_peer.value(), peer_mac)) {
        std::fputs("udp diag smoke arp cache seed failed\n", stderr);
        return 10;
    }

    net::PacketBuffer<64> ping_request{};
    auto wrote_ping_request = net::diag::udp::write_request_datagram<net::diag::PingOp>(
        ping_request,
        0x1001u,
        net::diag::PingRequest{{'p', 'i', 'n', 'g'}});
    if (!wrote_ping_request) {
        std::fputs("udp diag smoke ping request encode failed\n", stderr);
        return 11;
    }

    net::PacketBuffer<128> ping_udp{};
    auto wrote_ping_udp = net::write_udp_ipv4_datagram(
        ping_udp,
        peer_endpoint,
        local_endpoint,
        ping_request.view().payload);
    if (!wrote_ping_udp) {
        std::fputs("udp diag smoke ping udp encode failed\n", stderr);
        return 12;
    }

    net::PacketBuffer<160> ping_ipv4{};
    auto wrote_ping_ipv4 = net::write_ipv4_packet(
        ping_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x2101u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 51,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer_ip,
            .destination = local_ip,
        },
        ping_udp.view().payload);
    if (!wrote_ping_ipv4) {
        std::fputs("udp diag smoke ping ipv4 encode failed\n", stderr);
        return 13;
    }

    static constexpr util::u8 ping_padding[]{0x00u, 0x00u};
    net::PacketBuffer<192> ping_frame{};
    auto wrote_ping_frame = write_ether_frame(
        ping_frame,
        net::MacAddress::broadcast(),
        peer_mac,
        net::EtherType::ipv4,
        ping_ipv4.view().payload,
        net::ByteView{ping_padding, sizeof(ping_padding)});
    if (!wrote_ping_frame) {
        std::fputs("udp diag smoke ping ether encode failed\n", stderr);
        return 14;
    }

    link.queue_rx(ping_frame.view().payload);
    auto ping_progress = pump.service(0);
    if (!ping_progress
        || ping_progress.value().ipv4_delivered != 1
        || ping_progress.value().udp_delivered != 1
        || ping_progress.value().egress_flushed != 0
        || link.tx_calls != 2
        || server.request_count() != 1
        || server.reply_count() != 1
        || server.error_reply_count() != 0
        || server.queued_reply_count() != 0
        || server.drop_count() != 0
        || !server_state.saw_ping) {
        std::fputs("udp diag smoke ping service failed\n", stderr);
        return 15;
    }

    auto ping_reply_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    if (!ping_reply_frame
        || ping_reply_frame.value().type != net::EtherType::ipv4
        || !same_mac(ping_reply_frame.value().destination, peer_mac)
        || !same_mac(ping_reply_frame.value().source, local_mac)) {
        std::fputs("udp diag smoke ping reply ether mismatch\n", stderr);
        return 16;
    }

    auto ping_reply_ipv4 = net::parse_ipv4_packet(ping_reply_frame.value().payload);
    if (!ping_reply_ipv4
        || ping_reply_ipv4.value().protocol != net::Ipv4Protocol::udp
        || !same_ipv4(ping_reply_ipv4.value().source, local_ip)
        || !same_ipv4(ping_reply_ipv4.value().destination, peer_ip)) {
        std::fputs("udp diag smoke ping reply ipv4 mismatch\n", stderr);
        return 17;
    }

    auto ping_reply_udp = net::parse_udp_datagram(ping_reply_ipv4.value().payload);
    if (!ping_reply_udp
        || ping_reply_udp.value().source_port != local_endpoint.port
        || ping_reply_udp.value().destination_port != peer_endpoint.port) {
        std::fputs("udp diag smoke ping reply udp mismatch\n", stderr);
        return 18;
    }

    auto ping_reply_diag = net::diag::udp::parse_datagram(ping_reply_udp.value().payload);
    if (!ping_reply_diag
        || ping_reply_diag.value().header.kind != net::diag::udp::Kind::response
        || ping_reply_diag.value().header.opcode != net::diag::PingOp::opcode
        || ping_reply_diag.value().header.request_id != 0x1001u
        || ping_reply_diag.value().header.status != net::diag::udp::Status::ok) {
        std::fputs("udp diag smoke ping reply diag header mismatch\n", stderr);
        return 19;
    }

    auto ping_reply = net::diag::udp::decode_response<net::diag::PingOp>(ping_reply_diag.value());
    if (!ping_reply
        || ping_reply.value().text[0] != 'p'
        || ping_reply.value().text[1] != 'o'
        || ping_reply.value().text[2] != 'n'
        || ping_reply.value().text[3] != 'g') {
        std::fputs("udp diag smoke ping reply payload mismatch\n", stderr);
        return 20;
    }

    net::PacketBuffer<64> count_request{};
    auto wrote_count_request = net::diag::udp::write_request_datagram<net::diag::CountOp>(
        count_request,
        0x1002u,
        net::EmptyMessage{});
    if (!wrote_count_request) {
        std::fputs("udp diag smoke count request encode failed\n", stderr);
        return 21;
    }

    net::PacketBuffer<128> count_udp{};
    auto wrote_count_udp = net::write_udp_ipv4_datagram(
        count_udp,
        peer_endpoint,
        local_endpoint,
        count_request.view().payload);
    if (!wrote_count_udp) {
        std::fputs("udp diag smoke count udp encode failed\n", stderr);
        return 22;
    }

    net::PacketBuffer<160> count_ipv4{};
    auto wrote_count_ipv4 = net::write_ipv4_packet(
        count_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x2102u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 50,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer_ip,
            .destination = local_ip,
        },
        count_udp.view().payload);
    if (!wrote_count_ipv4) {
        std::fputs("udp diag smoke count ipv4 encode failed\n", stderr);
        return 23;
    }

    net::PacketBuffer<192> count_frame{};
    auto wrote_count_frame = write_ether_frame(
        count_frame,
        net::MacAddress::broadcast(),
        peer_mac,
        net::EtherType::ipv4,
        count_ipv4.view().payload);
    if (!wrote_count_frame) {
        std::fputs("udp diag smoke count ether encode failed\n", stderr);
        return 24;
    }

    link.queue_rx(count_frame.view().payload);
    auto count_progress = pump.service(0);
    if (!count_progress
        || count_progress.value().ipv4_delivered != 1
        || count_progress.value().udp_delivered != 1
        || link.tx_calls != 3
        || server.request_count() != 2
        || server.reply_count() != 2
        || !server_state.saw_count) {
        std::fputs("udp diag smoke count service failed\n", stderr);
        return 25;
    }

    auto count_reply_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    auto count_reply_ipv4 = net::parse_ipv4_packet(count_reply_frame.value().payload);
    auto count_reply_udp = net::parse_udp_datagram(count_reply_ipv4.value().payload);
    auto count_reply_diag = net::diag::udp::parse_datagram(count_reply_udp.value().payload);
    auto count_reply = net::diag::udp::decode_response<net::diag::CountOp>(count_reply_diag.value());
    if (!count_reply_diag
        || count_reply_diag.value().header.request_id != 0x1002u
        || count_reply_diag.value().header.status != net::diag::udp::Status::ok
        || !count_reply
        || count_reply.value().value != 7) {
        std::fputs("udp diag smoke count reply mismatch\n", stderr);
        return 26;
    }

    net::PacketBuffer<64> meta_request{};
    auto wrote_meta_request = net::diag::udp::write_request_datagram<net::diag::MetaOp>(
        meta_request,
        0x1003u,
        net::diag::MetaRequest{
            .code = 0x1234u,
            .flags = 0x5Au,
            .tag = {'o', 'k'},
        });
    if (!wrote_meta_request) {
        std::fputs("udp diag smoke meta request encode failed\n", stderr);
        return 27;
    }

    net::PacketBuffer<128> meta_udp{};
    auto wrote_meta_udp = net::write_udp_ipv4_datagram(
        meta_udp,
        peer_endpoint,
        local_endpoint,
        meta_request.view().payload);
    if (!wrote_meta_udp) {
        std::fputs("udp diag smoke meta udp encode failed\n", stderr);
        return 28;
    }

    net::PacketBuffer<160> meta_ipv4{};
    auto wrote_meta_ipv4 = net::write_ipv4_packet(
        meta_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x2103u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 49,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer_ip,
            .destination = local_ip,
        },
        meta_udp.view().payload);
    if (!wrote_meta_ipv4) {
        std::fputs("udp diag smoke meta ipv4 encode failed\n", stderr);
        return 29;
    }

    net::PacketBuffer<192> meta_frame{};
    auto wrote_meta_frame = write_ether_frame(
        meta_frame,
        net::MacAddress::broadcast(),
        peer_mac,
        net::EtherType::ipv4,
        meta_ipv4.view().payload);
    if (!wrote_meta_frame) {
        std::fputs("udp diag smoke meta ether encode failed\n", stderr);
        return 30;
    }

    link.queue_rx(meta_frame.view().payload);
    auto meta_progress = pump.service(0);
    if (!meta_progress
        || meta_progress.value().ipv4_delivered != 1
        || meta_progress.value().udp_delivered != 1
        || link.tx_calls != 4
        || server.request_count() != 3
        || server.reply_count() != 3
        || !server_state.saw_meta) {
        std::fputs("udp diag smoke meta service failed\n", stderr);
        return 31;
    }

    auto meta_reply_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    auto meta_reply_ipv4 = net::parse_ipv4_packet(meta_reply_frame.value().payload);
    auto meta_reply_udp = net::parse_udp_datagram(meta_reply_ipv4.value().payload);
    auto meta_reply_diag = net::diag::udp::parse_datagram(meta_reply_udp.value().payload);
    auto meta_reply = net::diag::udp::decode_response<net::diag::MetaOp>(meta_reply_diag.value());
    if (!meta_reply_diag
        || meta_reply_diag.value().header.request_id != 0x1003u
        || meta_reply_diag.value().header.status != net::diag::udp::Status::ok
        || !meta_reply
        || meta_reply.value().status != 0xA5u
        || meta_reply.value().reflected_code != 0x1235u
        || meta_reply.value().tag != std::array<util::u8, 2>{'o', 'k'}) {
        std::fputs("udp diag smoke meta reply mismatch\n", stderr);
        return 32;
    }

    net::PacketBuffer<64> slow_request{};
    auto wrote_slow_request = net::diag::udp::write_request_datagram<net::diag::SlowCountOp>(
        slow_request,
        0x1004u,
        net::diag::CounterValue{41});
    if (!wrote_slow_request) {
        std::fputs("udp diag smoke slow request encode failed\n", stderr);
        return 33;
    }

    net::PacketBuffer<128> slow_udp{};
    auto wrote_slow_udp = net::write_udp_ipv4_datagram(
        slow_udp,
        peer_endpoint,
        local_endpoint,
        slow_request.view().payload);
    if (!wrote_slow_udp) {
        std::fputs("udp diag smoke slow udp encode failed\n", stderr);
        return 34;
    }

    net::PacketBuffer<160> slow_ipv4{};
    auto wrote_slow_ipv4 = net::write_ipv4_packet(
        slow_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x2104u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 48,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer_ip,
            .destination = local_ip,
        },
        slow_udp.view().payload);
    if (!wrote_slow_ipv4) {
        std::fputs("udp diag smoke slow ipv4 encode failed\n", stderr);
        return 35;
    }

    net::PacketBuffer<192> slow_frame{};
    auto wrote_slow_frame = write_ether_frame(
        slow_frame,
        net::MacAddress::broadcast(),
        peer_mac,
        net::EtherType::ipv4,
        slow_ipv4.view().payload);
    if (!wrote_slow_frame) {
        std::fputs("udp diag smoke slow ether encode failed\n", stderr);
        return 36;
    }

    link.queue_rx(slow_frame.view().payload);
    auto slow_progress = pump.service(0);
    if (!slow_progress
        || slow_progress.value().ipv4_delivered != 1
        || slow_progress.value().udp_delivered != 1
        || link.tx_calls != 5
        || server.request_count() != 4
        || server.reply_count() != 4
        || !server_state.saw_slow_count) {
        std::fputs("udp diag smoke slow service failed\n", stderr);
        return 37;
    }

    auto slow_reply_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    auto slow_reply_ipv4 = net::parse_ipv4_packet(slow_reply_frame.value().payload);
    auto slow_reply_udp = net::parse_udp_datagram(slow_reply_ipv4.value().payload);
    auto slow_reply_diag = net::diag::udp::parse_datagram(slow_reply_udp.value().payload);
    auto slow_reply = net::diag::udp::decode_response<net::diag::SlowCountOp>(slow_reply_diag.value());
    if (!slow_reply_diag
        || slow_reply_diag.value().header.request_id != 0x1004u
        || slow_reply_diag.value().header.status != net::diag::udp::Status::ok
        || !slow_reply
        || slow_reply.value().value != 42u) {
        std::fputs("udp diag smoke slow reply mismatch\n", stderr);
        return 38;
    }

    net::PacketBuffer<64> unsupported_request{};
    auto wrote_unsupported_request = net::diag::udp::write_raw_datagram(
        unsupported_request,
        net::diag::udp::Kind::request,
        0x7Fu,
        0x1005u,
        net::diag::udp::Status::ok,
        {});
    if (!wrote_unsupported_request) {
        std::fputs("udp diag smoke unsupported request encode failed\n", stderr);
        return 39;
    }

    net::PacketBuffer<128> unsupported_udp{};
    auto wrote_unsupported_udp = net::write_udp_ipv4_datagram(
        unsupported_udp,
        peer_endpoint,
        local_endpoint,
        unsupported_request.view().payload);
    if (!wrote_unsupported_udp) {
        std::fputs("udp diag smoke unsupported udp encode failed\n", stderr);
        return 40;
    }

    net::PacketBuffer<160> unsupported_ipv4{};
    auto wrote_unsupported_ipv4 = net::write_ipv4_packet(
        unsupported_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x2105u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 47,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer_ip,
            .destination = local_ip,
        },
        unsupported_udp.view().payload);
    if (!wrote_unsupported_ipv4) {
        std::fputs("udp diag smoke unsupported ipv4 encode failed\n", stderr);
        return 41;
    }

    net::PacketBuffer<192> unsupported_frame{};
    auto wrote_unsupported_frame = write_ether_frame(
        unsupported_frame,
        net::MacAddress::broadcast(),
        peer_mac,
        net::EtherType::ipv4,
        unsupported_ipv4.view().payload);
    if (!wrote_unsupported_frame) {
        std::fputs("udp diag smoke unsupported ether encode failed\n", stderr);
        return 42;
    }

    link.queue_rx(unsupported_frame.view().payload);
    auto unsupported_progress = pump.service(0);
    if (!unsupported_progress
        || unsupported_progress.value().ipv4_delivered != 1
        || unsupported_progress.value().udp_delivered != 1
        || link.tx_calls != 6
        || server.request_count() != 5
        || server.reply_count() != 5
        || server.error_reply_count() != 1) {
        std::fputs("udp diag smoke unsupported service failed\n", stderr);
        return 43;
    }

    auto unsupported_reply_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    auto unsupported_reply_ipv4 = net::parse_ipv4_packet(unsupported_reply_frame.value().payload);
    auto unsupported_reply_udp = net::parse_udp_datagram(unsupported_reply_ipv4.value().payload);
    auto unsupported_reply_diag = net::diag::udp::parse_datagram(unsupported_reply_udp.value().payload);
    if (!unsupported_reply_diag
        || unsupported_reply_diag.value().header.opcode != 0x7Fu
        || unsupported_reply_diag.value().header.request_id != 0x1005u
        || unsupported_reply_diag.value().header.status != net::diag::udp::Status::unsupported
        || !unsupported_reply_diag.value().payload.empty()) {
        std::fputs("udp diag smoke unsupported reply mismatch\n", stderr);
        return 44;
    }

    static constexpr util::u8 malformed_payload[]{'b', 'a', 'd'};
    net::PacketBuffer<64> malformed_request{};
    auto wrote_malformed_request = net::diag::udp::write_raw_datagram(
        malformed_request,
        net::diag::udp::Kind::request,
        net::diag::PingOp::opcode,
        0x1006u,
        net::diag::udp::Status::ok,
        net::ByteView{malformed_payload, sizeof(malformed_payload)});
    if (!wrote_malformed_request) {
        std::fputs("udp diag smoke malformed request encode failed\n", stderr);
        return 45;
    }

    net::PacketBuffer<128> malformed_udp{};
    auto wrote_malformed_udp = net::write_udp_ipv4_datagram(
        malformed_udp,
        peer_endpoint,
        local_endpoint,
        malformed_request.view().payload);
    if (!wrote_malformed_udp) {
        std::fputs("udp diag smoke malformed udp encode failed\n", stderr);
        return 46;
    }

    net::PacketBuffer<160> malformed_ipv4{};
    auto wrote_malformed_ipv4 = net::write_ipv4_packet(
        malformed_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x2106u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 46,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer_ip,
            .destination = local_ip,
        },
        malformed_udp.view().payload);
    if (!wrote_malformed_ipv4) {
        std::fputs("udp diag smoke malformed ipv4 encode failed\n", stderr);
        return 47;
    }

    net::PacketBuffer<192> malformed_frame{};
    auto wrote_malformed_frame = write_ether_frame(
        malformed_frame,
        net::MacAddress::broadcast(),
        peer_mac,
        net::EtherType::ipv4,
        malformed_ipv4.view().payload);
    if (!wrote_malformed_frame) {
        std::fputs("udp diag smoke malformed ether encode failed\n", stderr);
        return 48;
    }

    link.queue_rx(malformed_frame.view().payload);
    auto malformed_progress = pump.service(0);
    if (!malformed_progress
        || malformed_progress.value().ipv4_delivered != 1
        || malformed_progress.value().udp_delivered != 1
        || link.tx_calls != 7
        || server.request_count() != 6
        || server.reply_count() != 6
        || server.error_reply_count() != 2
        || server.drop_count() != 0
        || server.last_error() != net::errc::ok) {
        std::fputs("udp diag smoke malformed service failed\n", stderr);
        return 49;
    }

    auto malformed_reply_frame = net::parse_ether_frame(net::PacketView{
        net::ByteView{link.tx_bytes.data(), link.tx_size},
        0,
        0
    });
    auto malformed_reply_ipv4 = net::parse_ipv4_packet(malformed_reply_frame.value().payload);
    auto malformed_reply_udp = net::parse_udp_datagram(malformed_reply_ipv4.value().payload);
    auto malformed_reply_diag = net::diag::udp::parse_datagram(malformed_reply_udp.value().payload);
    if (!malformed_reply_diag
        || malformed_reply_diag.value().header.opcode != net::diag::PingOp::opcode
        || malformed_reply_diag.value().header.request_id != 0x1006u
        || malformed_reply_diag.value().header.status != net::diag::udp::Status::bad_request
        || !malformed_reply_diag.value().payload.empty()) {
        std::fputs("udp diag smoke malformed reply mismatch\n", stderr);
        return 50;
    }

    std::puts("net udp diag smoke: ok");
    return 0;
}
