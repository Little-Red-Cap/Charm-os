#include <array>
#include <cstdio>

import net.driver;
import net.packet;
import net.stack;
import util.core;
import util.expected;

namespace {
    struct PacketProbe {
        std::array<util::u8, 32> bytes{};
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
        net::PacketPool<2, 32> rx_pool{};
        std::array<util::u8, 32> rx_bytes{};
        util::usize rx_size{0};
        bool rx_ready{false};
        std::array<util::u8, 32> tx_bytes{};
        util::usize tx_size{0};
        util::usize tx_calls{0};

        [[nodiscard]] net::NetDriverInfo info() const noexcept {
            return net::NetDriverInfo{
                .mtu = 12,
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
            auto lease = rx_pool.acquire(2);
            if (!lease) {
                return util::unexpected(lease.error());
            }
            auto appended = lease.value()->append(net::ByteView{rx_bytes.data(), rx_size});
            if (!appended) {
                return util::unexpected(appended.error());
            }
            rx_ready = false;
            return input_sink.consume(net::OwnedPacket{
                static_cast<net::PacketPool<2, 32>::Lease&&>(lease.value())
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

    bool bytes_eq(const std::array<util::u8, 32>& lhs,
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
        .mac = {},
        .address = net::IpAddress::ipv4(10, 0, 0, 2),
        .capabilities = net::NetIfCapability::rx | net::NetIfCapability::tx
    });
    if (!configured) {
        std::fputs("net driver smoke initial netif configure failed\n", stderr);
        return 1;
    }

    PacketProbe rx{};
    netif.set_input_sink(net::make_owned_packet_sink_ref(rx));

    StubLinkDriver link{};
    net::NetDriver driver{};
    auto attached = driver.attach(net::make_net_driver_provider_ref(link), netif);
    if (!attached || driver.state() != net::NetDriverState::attached || !netif.output_sink().valid()) {
        std::fputs("net driver attach failed\n", stderr);
        return 2;
    }
    if (netif.mtu() != 12 || !netif.supports(net::NetIfCapability::broadcast)) {
        std::fputs("net driver attach did not project link info\n", stderr);
        return 3;
    }

    net::Stack stack{};
    auto bound = netif.bind(stack);
    auto up = netif.bring_up();
    if (!bound || !up) {
        std::fputs("net driver smoke netif bringup failed\n", stderr);
        return 4;
    }

    net::PacketBuffer<32> tx_packet{};
    static constexpr util::u8 tx_bytes[]{0x08u, 0x06u, 0x00u, 0x01u};
    auto tx_reset = tx_packet.reset(2);
    auto tx_append = tx_packet.append(net::ByteView{tx_bytes, sizeof(tx_bytes)});
    auto tx_sent = netif.transmit(tx_packet.view());
    if (!tx_reset || !tx_append || !tx_sent || link.tx_calls != 1) {
        std::fputs("net driver transmit path failed\n", stderr);
        return 5;
    }
    if (!bytes_eq(link.tx_bytes, link.tx_size, tx_packet.view().payload)) {
        std::fputs("net driver transmit bytes mismatch\n", stderr);
        return 6;
    }

    static constexpr util::u8 rx_payload[]{0x08u, 0x00u, 0x45u, 0x00u, 0x00u, 0x2Au};
    link.queue_rx(net::ByteView{rx_payload, sizeof(rx_payload)});
    auto polled = driver.poll();
    if (!polled || rx.calls != 1 || link.rx_pool.in_use_count() != 0) {
        std::fputs("net driver poll/rx path failed\n", stderr);
        return 7;
    }
    if (!bytes_eq(rx.bytes, rx.size, net::ByteView{rx_payload, sizeof(rx_payload)})) {
        std::fputs("net driver rx bytes mismatch\n", stderr);
        return 8;
    }

    auto detached = driver.detach();
    if (!detached || driver.state() != net::NetDriverState::detached || netif.output_sink().valid()) {
        std::fputs("net driver detach failed\n", stderr);
        return 9;
    }

    auto detached_tx = netif.transmit(tx_packet.view());
    if (detached_tx || detached_tx.error() != net::errc::not_supported) {
        std::fputs("net driver detach did not clear tx hook\n", stderr);
        return 10;
    }

    std::puts("net driver smoke: ok");
    return 0;
}
