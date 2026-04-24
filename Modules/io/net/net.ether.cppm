module;

#include <array>
#include <utility>

export module net.ether;

import net.netif;
import net.packet;
import util.core;
import util.error;
import util.expected;

export namespace net {
    enum class EtherType : util::u16 {
        ipv4 = 0x0800u,
        arp = 0x0806u,
    };

    struct EtherFrameView {
        MacAddress destination{};
        MacAddress source{};
        EtherType type{EtherType::ipv4};
        PacketView payload{};
    };

    [[nodiscard]] constexpr util::usize ether_header_size() noexcept {
        return 14;
    }

    [[nodiscard]] constexpr util::u16 ether_type_value(EtherType type) noexcept {
        return static_cast<util::u16>(type);
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> prepend_ether_header(PacketBuffer<Capacity>& packet,
                                                    MacAddress destination,
                                                    MacAddress source,
                                                    EtherType type) noexcept {
        std::array<util::u8, ether_header_size()> header{};
        for (util::usize i = 0; i < 6; ++i) {
            header[i] = destination.bytes[i];
            header[6 + i] = source.bytes[i];
        }
        const auto raw_type = ether_type_value(type);
        header[12] = static_cast<util::u8>((raw_type >> 8) & 0xFFu);
        header[13] = static_cast<util::u8>(raw_type & 0xFFu);
        return packet.prepend(ByteView{header.data(), header.size()});
    }

    [[nodiscard]] constexpr Result<EtherFrameView> parse_ether_frame(PacketView packet) noexcept {
        if (packet.size() < ether_header_size()) {
            return util::unexpected(errc::invalid_format);
        }

        EtherFrameView frame{};
        frame.destination = MacAddress::from_bytes(
            packet[0], packet[1], packet[2], packet[3], packet[4], packet[5]);
        frame.source = MacAddress::from_bytes(
            packet[6], packet[7], packet[8], packet[9], packet[10], packet[11]);
        frame.type = static_cast<EtherType>(
            (static_cast<util::u16>(packet[12]) << 8) | static_cast<util::u16>(packet[13]));
        frame.payload = packet.subspan(ether_header_size());
        return Result<EtherFrameView>{std::in_place, frame};
    }
}
