module;

#include <array>

export module net.ipv4;

import net.netif;
import net.packet;
import util.core;
import util.error;
import util.expected;

export namespace net {
    enum class Ipv4Protocol : util::u8 {
        icmp = 1u,
        tcp = 6u,
        udp = 17u,
    };

    struct Ipv4PacketView {
        util::u8 dscp_ecn{0};
        util::u8 header_length{0};
        util::u16 total_length{0};
        util::u16 identification{0};
        util::u16 flags_fragment{0};
        util::u8 ttl{0};
        Ipv4Protocol protocol{Ipv4Protocol::icmp};
        util::u16 header_checksum{0};
        IpAddress source{};
        IpAddress destination{};
        PacketView options{};
        PacketView payload{};
    };

    struct Ipv4PacketSpec {
        util::u8 dscp_ecn{0};
        util::u16 identification{0};
        util::u16 flags_fragment{0};
        util::u8 ttl{64};
        Ipv4Protocol protocol{Ipv4Protocol::icmp};
        IpAddress source{};
        IpAddress destination{};
        ByteView options{};
    };

    [[nodiscard]] constexpr util::u8 ipv4_version() noexcept {
        return 4u;
    }

    [[nodiscard]] constexpr util::usize ipv4_min_header_size() noexcept {
        return 20u;
    }

    [[nodiscard]] constexpr util::usize ipv4_max_header_size() noexcept {
        return 60u;
    }

    [[nodiscard]] constexpr util::u16 ipv4_do_not_fragment_flag() noexcept {
        return 0x4000u;
    }

    [[nodiscard]] constexpr util::u16 ipv4_more_fragments_flag() noexcept {
        return 0x2000u;
    }

    [[nodiscard]] constexpr util::u16 ipv4_fragment_offset_mask() noexcept {
        return 0x1FFFu;
    }

    [[nodiscard]] constexpr util::u8 ipv4_protocol_value(Ipv4Protocol protocol) noexcept {
        return static_cast<util::u8>(protocol);
    }

    namespace detail {
        [[nodiscard]] constexpr util::u16 load_be16(PacketView packet, util::usize offset) noexcept {
            return static_cast<util::u16>(
                (static_cast<util::u16>(packet[offset]) << 8)
                | static_cast<util::u16>(packet[offset + 1]));
        }

        constexpr void store_be16(util::u8* out, util::u16 value) noexcept {
            out[0] = static_cast<util::u8>((value >> 8) & 0xFFu);
            out[1] = static_cast<util::u8>(value & 0xFFu);
        }

        [[nodiscard]] constexpr bool same_ipv4_address(const IpAddress& lhs, const IpAddress& rhs) noexcept {
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

        [[nodiscard]] constexpr util::u16 fold_checksum(util::u32 sum) noexcept {
            while ((sum >> 16) != 0u) {
                sum = (sum & 0xFFFFu) + (sum >> 16);
            }
            return static_cast<util::u16>(sum);
        }

        [[nodiscard]] constexpr util::u16 compute_header_checksum(PacketView header) noexcept {
            util::u32 sum = 0;
            for (util::usize offset = 0; offset < header.size(); offset += 2) {
                if (offset == 10u) {
                    continue;
                }
                sum += load_be16(header, offset);
            }
            return static_cast<util::u16>(~fold_checksum(sum));
        }
    }

    [[nodiscard]] constexpr Result<Ipv4PacketView> parse_ipv4_packet(PacketView packet) noexcept {
        if (packet.size() < ipv4_min_header_size()) {
            return util::unexpected(errc::invalid_format);
        }

        const auto version = static_cast<util::u8>(packet[0] >> 4);
        const auto ihl_words = static_cast<util::u8>(packet[0] & 0x0Fu);
        if (version != ipv4_version() || ihl_words < 5u) {
            return util::unexpected(errc::invalid_format);
        }

        const auto header_length = static_cast<util::usize>(ihl_words) * 4u;
        if (header_length > packet.size() || header_length > ipv4_max_header_size()) {
            return util::unexpected(errc::invalid_format);
        }

        const auto total_length = detail::load_be16(packet, 2);
        if (total_length < header_length || total_length > packet.size()) {
            return util::unexpected(errc::invalid_format);
        }

        const auto flags_fragment = detail::load_be16(packet, 6);
        if ((flags_fragment & 0x8000u) != 0u) {
            return util::unexpected(errc::invalid_format);
        }
        if ((flags_fragment & (ipv4_more_fragments_flag() | ipv4_fragment_offset_mask())) != 0u) {
            return util::unexpected(errc::not_supported);
        }

        const auto header = packet.subspan(0, header_length);
        const auto expected_checksum = detail::compute_header_checksum(header);
        const auto actual_checksum = detail::load_be16(header, 10);
        if (actual_checksum != expected_checksum) {
            return util::unexpected(errc::invalid_format);
        }

        return Result<Ipv4PacketView>{std::in_place, Ipv4PacketView{
            .dscp_ecn = packet[1],
            .header_length = static_cast<util::u8>(header_length),
            .total_length = total_length,
            .identification = detail::load_be16(packet, 4),
            .flags_fragment = flags_fragment,
            .ttl = packet[8],
            .protocol = static_cast<Ipv4Protocol>(packet[9]),
            .header_checksum = actual_checksum,
            .source = IpAddress::ipv4(packet[12], packet[13], packet[14], packet[15]),
            .destination = IpAddress::ipv4(packet[16], packet[17], packet[18], packet[19]),
            .options = packet.subspan(ipv4_min_header_size(), header_length - ipv4_min_header_size()),
            .payload = packet.subspan(header_length, total_length - header_length),
        }};
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_ipv4_packet(PacketBuffer<Capacity>& packet,
                                                 const Ipv4PacketSpec& spec,
                                                 ByteView payload) noexcept {
        if (!spec.source.is_ipv4() || !spec.destination.is_ipv4()) {
            return util::unexpected(errc::not_supported);
        }
        if (spec.ttl == 0u) {
            return util::unexpected(errc::invalid_arg);
        }
        if ((spec.flags_fragment & 0x8000u) != 0u) {
            return util::unexpected(errc::invalid_arg);
        }
        if ((spec.flags_fragment & (ipv4_more_fragments_flag() | ipv4_fragment_offset_mask())) != 0u) {
            return util::unexpected(errc::not_supported);
        }
        if ((spec.options.size() % 4u) != 0u) {
            return util::unexpected(errc::invalid_arg);
        }

        const auto header_length = ipv4_min_header_size() + spec.options.size();
        if (header_length > ipv4_max_header_size()) {
            return util::unexpected(errc::invalid_arg);
        }

        const auto total_length = header_length + payload.size();
        if (total_length > 0xFFFFu) {
            return util::unexpected(errc::buffer_overflow);
        }

        auto reset = packet.reset();
        if (!reset) {
            return util::unexpected(reset.error());
        }

        std::array<util::u8, ipv4_max_header_size()> header{};
        header[0] = static_cast<util::u8>((ipv4_version() << 4) | (header_length / 4u));
        header[1] = spec.dscp_ecn;
        detail::store_be16(header.data() + 2, static_cast<util::u16>(total_length));
        detail::store_be16(header.data() + 4, spec.identification);
        detail::store_be16(header.data() + 6, spec.flags_fragment);
        header[8] = spec.ttl;
        header[9] = ipv4_protocol_value(spec.protocol);
        for (util::usize i = 0; i < 4; ++i) {
            header[12 + i] = spec.source.bytes[i];
            header[16 + i] = spec.destination.bytes[i];
        }
        for (util::usize i = 0; i < spec.options.size(); ++i) {
            header[ipv4_min_header_size() + i] = spec.options[i];
        }

        const auto checksum = detail::compute_header_checksum(PacketView{
            ByteView{header.data(), header_length},
            0,
            0
        });
        detail::store_be16(header.data() + 10, checksum);

        auto appended_header = packet.append(ByteView{header.data(), header_length});
        if (!appended_header) {
            return util::unexpected(appended_header.error());
        }

        auto appended_payload = packet.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }
        return {};
    }

    class Ipv4Service {
    public:
        Ipv4Service() noexcept = default;

        explicit Ipv4Service(NetIf& netif) noexcept
            : netif_(&netif) {}

        void bind(NetIf& netif) noexcept {
            netif_ = &netif;
        }

        void set_icmp_sink(OwnedPacketSinkRef sink) noexcept {
            icmp_sink_ = sink;
        }

        void set_udp_sink(OwnedPacketSinkRef sink) noexcept {
            udp_sink_ = sink;
        }

        void set_tcp_sink(OwnedPacketSinkRef sink) noexcept {
            tcp_sink_ = sink;
        }

        [[nodiscard]] util::usize packet_count() const noexcept {
            return packet_count_;
        }

        [[nodiscard]] util::usize drop_count() const noexcept {
            return drop_count_;
        }

        [[nodiscard]] Result<void> consume(OwnedPacket packet) noexcept {
            const auto parsed = parse_ipv4_packet(packet.view());
            if (!parsed) {
                return util::unexpected(parsed.error());
            }

            if (netif_ != nullptr
                && netif_->address().is_ipv4()
                && !detail::same_ipv4_address(parsed.value().destination, netif_->address())) {
                ++drop_count_;
                return {};
            }

            auto trimmed = packet.trim_front(parsed.value().header_length);
            if (!trimmed) {
                return util::unexpected(trimmed.error());
            }

            switch (parsed.value().protocol) {
                case Ipv4Protocol::icmp:
                    if (!icmp_sink_.valid()) {
                        return util::unexpected(errc::not_supported);
                    }
                    ++packet_count_;
                    return icmp_sink_.consume(static_cast<OwnedPacket&&>(packet));
                case Ipv4Protocol::udp:
                    if (!udp_sink_.valid()) {
                        return util::unexpected(errc::not_supported);
                    }
                    ++packet_count_;
                    return udp_sink_.consume(static_cast<OwnedPacket&&>(packet));
                case Ipv4Protocol::tcp:
                    if (!tcp_sink_.valid()) {
                        return util::unexpected(errc::not_supported);
                    }
                    ++packet_count_;
                    return tcp_sink_.consume(static_cast<OwnedPacket&&>(packet));
                default:
                    return util::unexpected(errc::not_supported);
            }
        }

    private:
        NetIf* netif_{nullptr};
        OwnedPacketSinkRef icmp_sink_{};
        OwnedPacketSinkRef udp_sink_{};
        OwnedPacketSinkRef tcp_sink_{};
        util::usize packet_count_{0};
        util::usize drop_count_{0};
    };
}
