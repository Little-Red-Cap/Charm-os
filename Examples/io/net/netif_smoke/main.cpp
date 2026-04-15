#include <array>
#include <cstdio>

import net.netif;
import net.packet;
import net.stack;
import util.core;
import util.expected;

namespace {
    struct PacketProbe {
        std::array<util::u8, 32> bytes{};
        util::usize size{0};
        util::usize calls{0};

        [[nodiscard]] net::Result<void> consume(net::PacketView packet) noexcept {
            if (packet.size() > bytes.size()) {
                return util::unexpected(net::errc::buffer_overflow);
            }
            for (util::usize i = 0; i < packet.size(); ++i) {
                bytes[i] = packet[i];
            }
            size = packet.size();
            ++calls;
            return {};
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
        .mtu = 8,
        .mac = net::MacAddress::from_bytes(0x02u, 0x00u, 0x00u, 0x00u, 0x00u, 0x01u),
        .address = net::IpAddress::ipv4(192, 168, 10, 2),
        .capabilities = net::NetIfCapability::rx
            | net::NetIfCapability::tx
            | net::NetIfCapability::broadcast
    });
    if (!configured || netif.state() != net::NetIfState::detached || netif.mtu() != 8) {
        std::fputs("netif configure failed\n", stderr);
        return 1;
    }

    net::Stack stack{};
    auto bound = netif.bind(stack);
    if (!bound || !netif.bound() || netif.stack() != &stack || netif.state() != net::NetIfState::down) {
        std::fputs("netif bind failed\n", stderr);
        return 2;
    }

    net::PacketBuffer<16> packet{};
    static constexpr util::u8 payload[]{0x08u, 0x00u, 0xAAu, 0x55u};
    auto reset = packet.reset(2);
    auto append = packet.append(net::ByteView{payload, sizeof(payload)});
    if (!reset || !append) {
        std::fputs("packet prepare failed\n", stderr);
        return 3;
    }

    auto down_input = netif.deliver_input(packet.view());
    if (down_input || down_input.error() != net::errc::bad_state) {
        std::fputs("netif down-state check failed\n", stderr);
        return 4;
    }

    PacketProbe rx{};
    PacketProbe tx{};
    netif.set_input_sink(net::make_packet_sink_ref(rx));
    netif.set_output_sink(net::make_packet_sink_ref(tx));

    auto up = netif.bring_up();
    if (!up || netif.state() != net::NetIfState::up) {
        std::fputs("netif bring_up failed\n", stderr);
        return 5;
    }

    auto delivered = netif.deliver_input(packet.view());
    auto transmitted = netif.transmit(packet.view());
    if (!delivered || !transmitted || rx.calls != 1 || tx.calls != 1) {
        std::fputs("netif packet hooks failed\n", stderr);
        return 6;
    }
    if (!bytes_eq(rx.bytes, rx.size, packet.view().payload)
        || !bytes_eq(tx.bytes, tx.size, packet.view().payload)) {
        std::fputs("netif packet payload mismatch\n", stderr);
        return 7;
    }

    net::PacketBuffer<16> oversized{};
    static constexpr util::u8 large_payload[]{
        1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u
    };
    auto oversized_reset = oversized.reset();
    auto oversized_append = oversized.append(net::ByteView{large_payload, sizeof(large_payload)});
    auto oversized_tx = netif.transmit(oversized.view());
    if (!oversized_reset || !oversized_append
        || oversized_tx
        || oversized_tx.error() != net::errc::invalid_arg) {
        std::fputs("netif mtu check failed\n", stderr);
        return 8;
    }

    netif.bring_down();
    if (netif.state() != net::NetIfState::down) {
        std::fputs("netif bring_down failed\n", stderr);
        return 9;
    }

    netif.unbind();
    if (netif.bound() || netif.stack() != nullptr || netif.state() != net::NetIfState::detached) {
        std::fputs("netif unbind failed\n", stderr);
        return 10;
    }

    std::puts("net netif smoke: ok");
    return 0;
}
