#include <array>
#include <cstdio>

import net.driver;
import net.ether;
import net.ipv4;
import net.packet;
import net.stack;
import net.udp;
import util.core;
import util.expected;

namespace {
    struct DatagramProbe {
        std::array<util::u8, 96> bytes{};
        util::usize size{0};
        util::usize calls{0};
        net::UdpDatagramInfo info{};

        [[nodiscard]] net::Result<void> consume(const net::UdpDatagramInfo& datagram,
                                                net::OwnedPacket packet) noexcept {
            const auto view = packet.view();
            if (view.size() > bytes.size()) {
                return util::unexpected(net::errc::buffer_overflow);
            }
            for (util::usize i = 0; i < view.size(); ++i) {
                bytes[i] = view[i];
            }
            size = view.size();
            info = datagram;
            ++calls;
            return {};
        }
    };

    struct StubLinkDriver {
        net::OwnedPacketSinkRef input_sink{};
        net::PacketPool<2, 128> rx_pool{};
        std::array<util::u8, 128> rx_bytes{};
        util::usize rx_size{0};
        bool rx_ready{false};

        [[nodiscard]] net::NetDriverInfo info() const noexcept {
            return net::NetDriverInfo{
                .mtu = 128,
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
                static_cast<net::PacketPool<2, 128>::Lease&&>(lease.value())
            });
        }

        [[nodiscard]] net::Result<void> transmit(net::PacketView) noexcept {
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

    [[nodiscard]] bool bytes_eq(const std::array<util::u8, 96>& lhs,
                                util::usize lhs_size,
                                net::ByteView rhs) noexcept {
        if (lhs_size != rhs.size()) {
            return false;
        }
        for (util::usize i = 0; i < lhs_size; ++i) {
            if (lhs[i] != rhs[i]) {
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
}

int main() {
    constexpr auto local_mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u);
    constexpr auto local_ip = net::IpAddress::ipv4(10, 0, 0, 2);
    constexpr auto peer_ip = net::IpAddress::ipv4(10, 0, 0, 9);
    constexpr auto peer = net::Endpoint{peer_ip, 7000};
    constexpr auto local = net::Endpoint{local_ip, 5000};

    net::NetIf netif{};
    auto configured = netif.configure(net::NetIfConfig{
        .mtu = 128,
        .mac = local_mac,
        .address = local_ip,
        .capabilities = net::NetIfCapability::rx
            | net::NetIfCapability::tx
            | net::NetIfCapability::broadcast
    });
    if (!configured) {
        std::fputs("udp smoke netif configure failed\n", stderr);
        return 1;
    }

    StubLinkDriver link{};
    net::NetDriver driver{};
    auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
    if (!attached) {
        std::fputs("udp smoke driver attach failed\n", stderr);
        return 2;
    }

    net::Stack stack{};
    net::Ipv4Service ipv4{netif};
    net::UdpService<4> udp{};
    DatagramProbe probe{};
    auto bound_port = udp.bind(local.port, net::make_udp_datagram_sink_ref(probe));
    if (!bound_port || udp.binding_count() != 1) {
        std::fputs("udp smoke bind failed\n", stderr);
        return 3;
    }
    ipv4.set_udp_sink(net::make_ipv4_packet_sink_ref(udp));
    stack.set_ipv4_sink(net::make_owned_packet_sink_ref(ipv4));

    auto registered = stack.register_driver(driver);
    if (!registered || stack.driver_count() != 1 || stack.netif_count() != 1) {
        std::fputs("udp smoke stack register failed\n", stderr);
        return 4;
    }

    auto up = netif.bring_up();
    if (!up) {
        std::fputs("udp smoke netif bring_up failed\n", stderr);
        return 5;
    }

    static constexpr util::u8 payload[]{'p', 'i', 'n', 'g'};
    net::PacketBuffer<128> udp_datagram{};
    auto encoded_udp = net::write_udp_ipv4_datagram(
        udp_datagram,
        peer,
        local,
        net::ByteView{payload, sizeof(payload)});
    if (!encoded_udp) {
        std::fputs("udp smoke datagram encode failed\n", stderr);
        return 6;
    }

    auto parsed_udp = net::parse_udp_datagram(udp_datagram.view());
    if (!parsed_udp
        || parsed_udp.value().source_port != peer.port
        || parsed_udp.value().destination_port != local.port
        || parsed_udp.value().length != net::udp_header_size() + sizeof(payload)
        || parsed_udp.value().checksum == 0u) {
        std::fputs("udp smoke datagram parse mismatch\n", stderr);
        return 7;
    }

    net::PacketBuffer<128> ipv4_packet{};
    auto encoded_ipv4 = net::write_ipv4_packet(
        ipv4_packet,
        net::Ipv4PacketSpec{
            .identification = 0x1357u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 48,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer.address,
            .destination = local.address,
        },
        udp_datagram.view().payload);
    if (!encoded_ipv4) {
        std::fputs("udp smoke ipv4 encode failed\n", stderr);
        return 8;
    }

    static constexpr util::u8 ether_padding[]{0x00u, 0x00u, 0x00u, 0x00u};
    net::PacketBuffer<128> frame{};
    auto wrote_frame = write_ether_frame(
        frame,
        net::MacAddress::broadcast(),
        local_mac,
        net::EtherType::ipv4,
        ipv4_packet.view().payload,
        net::ByteView{ether_padding, sizeof(ether_padding)});
    if (!wrote_frame) {
        std::fputs("udp smoke ether encode failed\n", stderr);
        return 9;
    }

    link.queue_rx(frame.view().payload);
    auto polled = stack.poll_links();
    if (!polled || udp.packet_count() != 1 || udp.drop_count() != 0 || probe.calls != 1 || link.rx_pool.in_use_count() != 0) {
        std::fputs("udp smoke dispatch failed\n", stderr);
        return 10;
    }
    if (!bytes_eq(probe.bytes, probe.size, net::ByteView{payload, sizeof(payload)})) {
        std::fputs("udp smoke payload trim mismatch\n", stderr);
        return 11;
    }
    if (!same_ipv4(probe.info.local.address, local.address)
        || probe.info.local.port != local.port
        || !same_ipv4(probe.info.peer.address, peer.address)
        || probe.info.peer.port != peer.port
        || probe.info.length != net::udp_header_size() + sizeof(payload)
        || probe.info.checksum != parsed_udp.value().checksum) {
        std::fputs("udp smoke endpoint metadata mismatch\n", stderr);
        return 12;
    }

    net::PacketBuffer<128> dropped_udp{};
    auto encoded_dropped_udp = net::write_udp_ipv4_datagram(
        dropped_udp,
        peer,
        net::Endpoint{local.address, 6001},
        net::ByteView{payload, sizeof(payload)});
    if (!encoded_dropped_udp) {
        std::fputs("udp smoke drop datagram encode failed\n", stderr);
        return 13;
    }

    net::PacketBuffer<128> dropped_ipv4{};
    auto encoded_dropped_ipv4 = net::write_ipv4_packet(
        dropped_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x2468u,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 48,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer.address,
            .destination = local.address,
        },
        dropped_udp.view().payload);
    if (!encoded_dropped_ipv4) {
        std::fputs("udp smoke drop ipv4 encode failed\n", stderr);
        return 14;
    }

    net::PacketBuffer<128> dropped_frame{};
    auto wrote_dropped_frame = write_ether_frame(
        dropped_frame,
        net::MacAddress::broadcast(),
        local_mac,
        net::EtherType::ipv4,
        dropped_ipv4.view().payload);
    if (!wrote_dropped_frame) {
        std::fputs("udp smoke drop ether encode failed\n", stderr);
        return 15;
    }

    link.queue_rx(dropped_frame.view().payload);
    polled = stack.poll_links();
    if (!polled || udp.packet_count() != 1 || udp.drop_count() != 1 || probe.calls != 1) {
        std::fputs("udp smoke unbound-port drop failed\n", stderr);
        return 16;
    }

    net::PacketBuffer<128> invalid_udp{};
    auto encoded_invalid_udp = net::write_udp_ipv4_datagram(
        invalid_udp,
        peer,
        local,
        net::ByteView{payload, sizeof(payload)});
    if (!encoded_invalid_udp) {
        std::fputs("udp smoke invalid datagram encode failed\n", stderr);
        return 17;
    }
    invalid_udp.mut_view()[6] ^= 0x5Au;

    net::PacketBuffer<128> invalid_ipv4{};
    auto encoded_invalid_ipv4 = net::write_ipv4_packet(
        invalid_ipv4,
        net::Ipv4PacketSpec{
            .identification = 0x369Cu,
            .flags_fragment = net::ipv4_do_not_fragment_flag(),
            .ttl = 48,
            .protocol = net::Ipv4Protocol::udp,
            .source = peer.address,
            .destination = local.address,
        },
        invalid_udp.view().payload);
    if (!encoded_invalid_ipv4) {
        std::fputs("udp smoke invalid ipv4 encode failed\n", stderr);
        return 18;
    }

    net::PacketBuffer<128> invalid_frame{};
    auto wrote_invalid_frame = write_ether_frame(
        invalid_frame,
        net::MacAddress::broadcast(),
        local_mac,
        net::EtherType::ipv4,
        invalid_ipv4.view().payload);
    if (!wrote_invalid_frame) {
        std::fputs("udp smoke invalid ether encode failed\n", stderr);
        return 19;
    }

    link.queue_rx(invalid_frame.view().payload);
    polled = stack.poll_links();
    if (polled || polled.error() != net::errc::invalid_format) {
        std::fputs("udp smoke checksum validation failed\n", stderr);
        return 20;
    }

    std::puts("net udp smoke: ok");
    return 0;
}
