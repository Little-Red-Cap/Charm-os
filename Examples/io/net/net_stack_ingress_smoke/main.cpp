#include <array>
#include <cstdio>

import net.driver;
import net.ether;
import net.packet;
import net.stack;
import util.core;
import util.expected;

namespace {
    struct PayloadProbe {
        std::array<util::u8, 64> bytes{};
        util::usize size{0};
        util::usize calls{0};

        [[nodiscard]] net::Result<void> consume(net::OwnedPacket packet) noexcept {
            const auto view = packet.view();
            if (view.size() > bytes.size()) {
                return util::unexpected(net::errc::buffer_overflow);
            }
            for (util::usize i = 0; i < view.size(); ++i) {
                bytes[i] = view[i];
            }
            size = view.size();
            ++calls;
            return {};
        }
    };

    struct StubLinkDriver {
        net::OwnedPacketSinkRef input_sink{};
        net::PacketPool<2, 64> rx_pool{};
        std::array<util::u8, 64> rx_bytes{};
        util::usize rx_size{0};
        bool rx_ready{false};

        [[nodiscard]] net::NetDriverInfo info() const noexcept {
            return net::NetDriverInfo{
                .mtu = 64,
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
                static_cast<net::PacketPool<2, 64>::Lease&&>(lease.value())
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

    void write_ether_frame(std::array<util::u8, 64>& frame,
                           util::usize& frame_size,
                           net::EtherType type,
                           net::ByteView payload) noexcept {
        static constexpr util::u8 dst[]{
            0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu
        };
        static constexpr util::u8 src[]{
            0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u
        };

        frame_size = 14 + payload.size();
        for (util::usize i = 0; i < 6; ++i) {
            frame[i] = dst[i];
            frame[6 + i] = src[i];
        }
        const auto raw_type = net::ether_type_value(type);
        frame[12] = static_cast<util::u8>((raw_type >> 8) & 0xFFu);
        frame[13] = static_cast<util::u8>(raw_type & 0xFFu);
        for (util::usize i = 0; i < payload.size(); ++i) {
            frame[14 + i] = payload[i];
        }
    }

    bool bytes_eq(const std::array<util::u8, 64>& lhs,
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
}

int main() {
    net::NetIf netif{};
    auto configured = netif.configure(net::NetIfConfig{
        .mtu = 64,
        .mac = net::MacAddress::from_bytes(0x02u, 0x11u, 0x22u, 0x33u, 0x44u, 0x55u),
        .address = net::IpAddress::ipv4(10, 0, 0, 2),
        .capabilities = net::NetIfCapability::rx
            | net::NetIfCapability::tx
            | net::NetIfCapability::broadcast
    });
    if (!configured) {
        std::fputs("stack ingress netif configure failed\n", stderr);
        return 1;
    }

    StubLinkDriver link{};
    net::NetDriver driver{};
    auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
    if (!attached) {
        std::fputs("stack ingress driver attach failed\n", stderr);
        return 2;
    }

    net::Stack stack{};
    PayloadProbe arp{};
    PayloadProbe ipv4{};
    stack.set_arp_sink(net::make_owned_packet_sink_ref(arp));
    stack.set_ipv4_sink(net::make_owned_packet_sink_ref(ipv4));

    auto registered = stack.register_driver(driver);
    if (!registered || stack.netif_count() != 1 || stack.driver_count() != 1 || netif.stack() != &stack) {
        std::fputs("stack ingress register_driver failed\n", stderr);
        return 3;
    }

    auto up = netif.bring_up();
    if (!up || netif.state() != net::NetIfState::up) {
        std::fputs("stack ingress bring_up failed\n", stderr);
        return 4;
    }

    static constexpr util::u8 arp_payload[]{0x00u, 0x01u, 0x08u, 0x00u, 0x06u, 0x04u};
    std::array<util::u8, 64> arp_frame{};
    util::usize arp_frame_size{0};
    write_ether_frame(
        arp_frame,
        arp_frame_size,
        net::EtherType::arp,
        net::ByteView{arp_payload, sizeof(arp_payload)});
    link.queue_rx(net::ByteView{arp_frame.data(), arp_frame_size});

    auto polled = stack.poll_links();
    if (!polled || arp.calls != 1 || ipv4.calls != 0 || link.rx_pool.in_use_count() != 0) {
        std::fputs("stack ingress arp dispatch failed\n", stderr);
        return 5;
    }
    if (!bytes_eq(arp.bytes, arp.size, net::ByteView{arp_payload, sizeof(arp_payload)})) {
        std::fputs("stack ingress arp payload mismatch\n", stderr);
        return 6;
    }

    static constexpr util::u8 ipv4_payload[]{0x45u, 0x00u, 0x00u, 0x2Au, 0x12u, 0x34u};
    std::array<util::u8, 64> ipv4_frame{};
    util::usize ipv4_frame_size{0};
    write_ether_frame(
        ipv4_frame,
        ipv4_frame_size,
        net::EtherType::ipv4,
        net::ByteView{ipv4_payload, sizeof(ipv4_payload)});
    link.queue_rx(net::ByteView{ipv4_frame.data(), ipv4_frame_size});

    polled = stack.poll_links();
    if (!polled || arp.calls != 1 || ipv4.calls != 1 || link.rx_pool.in_use_count() != 0) {
        std::fputs("stack ingress ipv4 dispatch failed\n", stderr);
        return 7;
    }
    if (!bytes_eq(ipv4.bytes, ipv4.size, net::ByteView{ipv4_payload, sizeof(ipv4_payload)})) {
        std::fputs("stack ingress ipv4 payload mismatch\n", stderr);
        return 8;
    }

    static constexpr util::u8 unknown_payload[]{0x60u, 0x00u, 0x00u, 0x00u};
    std::array<util::u8, 64> unknown_frame{};
    util::usize unknown_frame_size{0};
    write_ether_frame(
        unknown_frame,
        unknown_frame_size,
        static_cast<net::EtherType>(0x86DDu),
        net::ByteView{unknown_payload, sizeof(unknown_payload)});
    link.queue_rx(net::ByteView{unknown_frame.data(), unknown_frame_size});

    polled = stack.poll_links();
    if (polled || polled.error() != net::errc::not_supported) {
        std::fputs("stack ingress unknown ether type check failed\n", stderr);
        return 9;
    }

    std::puts("net stack ingress smoke: ok");
    return 0;
}
