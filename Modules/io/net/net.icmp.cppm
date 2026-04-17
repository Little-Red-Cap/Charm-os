module;

#include <array>
#include <concepts>

export module net.icmp;

export import net.common;
export import net.ipv4;
export import net.packet;
import net.arp;
import net.ether;
import net.netif;
import util.core;
import util.error;
import util.expected;

export namespace net {
    enum class IcmpType : util::u8 {
        echo_reply = 0u,
        echo_request = 8u,
    };

    struct IcmpPacketView {
        util::u8 type{0};
        util::u8 code{0};
        util::u16 checksum{0};
        PacketView payload{};
    };

    struct IcmpEchoView {
        IcmpType type{IcmpType::echo_request};
        util::u8 code{0};
        util::u16 checksum{0};
        util::u16 identifier{0};
        util::u16 sequence{0};
        PacketView payload{};
    };

    struct IcmpEchoInfo {
        IcmpType type{IcmpType::echo_request};
        IpAddress local{};
        IpAddress peer{};
        util::u16 identifier{0};
        util::u16 sequence{0};
    };

    template <typename T>
    concept IcmpEchoSink = requires(T& t, const IcmpEchoInfo& info, OwnedPacket packet) {
        { t.consume(info, static_cast<OwnedPacket&&>(packet)) } noexcept -> std::same_as<Result<void>>;
    };

    struct IcmpEchoSinkOps {
        Result<void> (*consume)(void*, const IcmpEchoInfo&, OwnedPacket) noexcept;
    };

    struct IcmpEchoSinkRef {
        void* self{nullptr};
        const IcmpEchoSinkOps* ops{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return self != nullptr && ops != nullptr && ops->consume != nullptr;
        }

        [[nodiscard]] Result<void> consume(const IcmpEchoInfo& info, OwnedPacket packet) const noexcept {
            if (!valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            return ops->consume(self, info, static_cast<OwnedPacket&&>(packet));
        }
    };

    template <IcmpEchoSink T>
    inline const IcmpEchoSinkOps* icmp_echo_sink_ops() noexcept {
        static const IcmpEchoSinkOps ops{
            .consume = [](void* self, const IcmpEchoInfo& info, OwnedPacket packet) noexcept {
                return static_cast<T*>(self)->consume(info, static_cast<OwnedPacket&&>(packet));
            }
        };
        return &ops;
    }

    template <IcmpEchoSink T>
    inline IcmpEchoSinkRef make_icmp_echo_sink_ref(T& sink) noexcept {
        return IcmpEchoSinkRef{&sink, icmp_echo_sink_ops<T>()};
    }

    [[nodiscard]] constexpr util::u8 icmp_type_value(IcmpType type) noexcept {
        return static_cast<util::u8>(type);
    }

    [[nodiscard]] constexpr util::usize icmp_common_header_size() noexcept {
        return 4u;
    }

    [[nodiscard]] constexpr util::usize icmp_echo_header_size() noexcept {
        return 8u;
    }

    namespace icmp_detail {
        [[nodiscard]] constexpr util::u16 load_be16(PacketView packet, util::usize offset) noexcept {
            return static_cast<util::u16>(
                (static_cast<util::u16>(packet[offset]) << 8)
                | static_cast<util::u16>(packet[offset + 1]));
        }

        constexpr void store_be16(util::u8* out, util::u16 value) noexcept {
            out[0] = static_cast<util::u8>((value >> 8) & 0xFFu);
            out[1] = static_cast<util::u8>(value & 0xFFu);
        }

        [[nodiscard]] constexpr util::u32 accumulate_checksum(util::u32 sum, ByteView bytes) noexcept {
            util::usize offset = 0;
            while ((offset + 1u) < bytes.size()) {
                sum += static_cast<util::u16>(
                    (static_cast<util::u16>(bytes[offset]) << 8)
                    | static_cast<util::u16>(bytes[offset + 1]));
                offset += 2u;
            }
            if (offset < bytes.size()) {
                sum += static_cast<util::u16>(bytes[offset]) << 8;
            }
            return sum;
        }

        [[nodiscard]] constexpr util::u16 fold_checksum(util::u32 sum) noexcept {
            while ((sum >> 16) != 0u) {
                sum = (sum & 0xFFFFu) + (sum >> 16);
            }
            return static_cast<util::u16>(sum);
        }

        [[nodiscard]] constexpr util::u16 compute_icmp_checksum(PacketView packet) noexcept {
            const auto sum = accumulate_checksum(0u, ByteView{packet.data(), packet.size()});
            return static_cast<util::u16>(~fold_checksum(sum));
        }

        [[nodiscard]] Result<IpAddress> normalize_icmp_ipv4_local(const NetIf& netif,
                                                                  IpAddress local) noexcept {
            if (local.is_unspecified() || local.is_any()) {
                local = netif.address();
            }
            if (!local.is_ipv4() || !is_same_ipv4_address(local, netif.address())) {
                return util::unexpected(errc::invalid_arg);
            }
            return Result<IpAddress>{std::in_place, local};
        }
    }

    [[nodiscard]] constexpr Result<IcmpPacketView> parse_icmp_packet(PacketView packet) noexcept {
        if (packet.size() < icmp_common_header_size()) {
            return util::unexpected(errc::invalid_format);
        }

        if (icmp_detail::compute_icmp_checksum(packet) != 0u) {
            return util::unexpected(errc::invalid_format);
        }

        return Result<IcmpPacketView>{std::in_place, IcmpPacketView{
            .type = packet[0],
            .code = packet[1],
            .checksum = icmp_detail::load_be16(packet, 2),
            .payload = packet.subspan(icmp_common_header_size()),
        }};
    }

    [[nodiscard]] constexpr Result<IcmpEchoView> decode_icmp_echo(
        const IcmpPacketView& packet) noexcept {
        if (packet.type != icmp_type_value(IcmpType::echo_request)
            && packet.type != icmp_type_value(IcmpType::echo_reply)) {
            return util::unexpected(errc::not_supported);
        }
        if (packet.code != 0u || packet.payload.size() < 4u) {
            return util::unexpected(errc::invalid_format);
        }

        return Result<IcmpEchoView>{std::in_place, IcmpEchoView{
            .type = static_cast<IcmpType>(packet.type),
            .code = packet.code,
            .checksum = packet.checksum,
            .identifier = icmp_detail::load_be16(packet.payload, 0),
            .sequence = icmp_detail::load_be16(packet.payload, 2),
            .payload = packet.payload.subspan(4),
        }};
    }

    [[nodiscard]] constexpr Result<IcmpEchoView> parse_icmp_echo_packet(PacketView packet) noexcept {
        const auto parsed = parse_icmp_packet(packet);
        if (!parsed) {
            return util::unexpected(parsed.error());
        }
        return decode_icmp_echo(parsed.value());
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_icmp_packet(PacketBuffer<Capacity>& packet,
                                                 util::u8 type,
                                                 util::u8 code,
                                                 ByteView payload) noexcept {
        if ((icmp_common_header_size() + payload.size()) > Capacity) {
            return util::unexpected(errc::buffer_overflow);
        }

        auto reset = packet.reset();
        if (!reset) {
            return util::unexpected(reset.error());
        }

        std::array<util::u8, icmp_common_header_size()> header{};
        header[0] = type;
        header[1] = code;

        auto appended_header = packet.append(ByteView{header.data(), header.size()});
        if (!appended_header) {
            return util::unexpected(appended_header.error());
        }

        auto appended_payload = packet.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }

        auto checksum = icmp_detail::compute_icmp_checksum(packet.view());
        if (checksum == 0u) {
            checksum = 0xFFFFu;
        }
        auto out = packet.mut_view();
        icmp_detail::store_be16(out.data() + 2, checksum);
        return {};
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> prepend_icmp_echo_header(PacketBuffer<Capacity>& packet,
                                                        IcmpType type,
                                                        util::u16 identifier,
                                                        util::u16 sequence) noexcept {
        std::array<util::u8, icmp_echo_header_size()> header{};
        header[0] = icmp_type_value(type);
        header[1] = 0u;
        icmp_detail::store_be16(header.data() + 4, identifier);
        icmp_detail::store_be16(header.data() + 6, sequence);

        auto prepended_header = packet.prepend(ByteView{header.data(), header.size()});
        if (!prepended_header) {
            return util::unexpected(prepended_header.error());
        }

        auto checksum = icmp_detail::compute_icmp_checksum(packet.view());
        if (checksum == 0u) {
            checksum = 0xFFFFu;
        }
        auto out = packet.mut_view();
        icmp_detail::store_be16(out.data() + 2, checksum);
        return {};
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_icmp_echo_packet(PacketBuffer<Capacity>& packet,
                                                      IcmpType type,
                                                      util::u16 identifier,
                                                      util::u16 sequence,
                                                      ByteView payload) noexcept {
        auto reset = packet.reset(icmp_echo_header_size());
        if (!reset) {
            return util::unexpected(reset.error());
        }

        auto appended_payload = packet.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }

        return prepend_icmp_echo_header(packet, type, identifier, sequence);
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_icmp_echo_request(PacketBuffer<Capacity>& packet,
                                                       util::u16 identifier,
                                                       util::u16 sequence,
                                                       ByteView payload) noexcept {
        return write_icmp_echo_packet(
            packet,
            IcmpType::echo_request,
            identifier,
            sequence,
            payload);
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_icmp_echo_reply(PacketBuffer<Capacity>& packet,
                                                     util::u16 identifier,
                                                     util::u16 sequence,
                                                     ByteView payload) noexcept {
        return write_icmp_echo_packet(
            packet,
            IcmpType::echo_reply,
            identifier,
            sequence,
            payload);
    }

    template <util::usize TxCapacity>
    [[nodiscard]] Result<void> send_icmp_echo_ipv4_resolved(NetIf& netif,
                                                            MacAddress peer_mac,
                                                            IpAddress local,
                                                            IpAddress peer,
                                                            IcmpType type,
                                                            util::u16 identifier,
                                                            util::u16 sequence,
                                                            ByteView payload,
                                                            util::u8 ttl = 64,
                                                            util::u16 identification = 0,
                                                            util::u8 dscp_ecn = 0) noexcept {
        PacketBuffer<TxCapacity> frame{};
        auto reset = frame.reset(ether_header_size() + ipv4_min_header_size() + icmp_echo_header_size());
        if (!reset) {
            return util::unexpected(reset.error());
        }

        auto appended_payload = frame.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }

        auto prepended_icmp = prepend_icmp_echo_header(frame, type, identifier, sequence);
        if (!prepended_icmp) {
            return util::unexpected(prepended_icmp.error());
        }

        auto prepended_ipv4 = prepend_ipv4_header(frame, Ipv4PacketSpec{
            .dscp_ecn = dscp_ecn,
            .identification = identification,
            .flags_fragment = ipv4_do_not_fragment_flag(),
            .ttl = ttl,
            .protocol = Ipv4Protocol::icmp,
            .source = local,
            .destination = peer,
        });
        if (!prepended_ipv4) {
            return util::unexpected(prepended_ipv4.error());
        }

        auto prepended_ether = prepend_ether_header(
            frame,
            peer_mac,
            netif.mac(),
            EtherType::ipv4);
        if (!prepended_ether) {
            return util::unexpected(prepended_ether.error());
        }

        return netif.transmit(frame.view());
    }

    template <util::usize TxCapacity, util::usize ArpCapacity>
    [[nodiscard]] Result<void> send_icmp_echo_ipv4(NetIf& netif,
                                                   const ArpTable<ArpCapacity>& arp,
                                                   IpAddress local,
                                                   IpAddress peer,
                                                   IcmpType type,
                                                   util::u16 identifier,
                                                   util::u16 sequence,
                                                   ByteView payload,
                                                   util::u8 ttl = 64,
                                                   util::u16 identification = 0,
                                                   util::u8 dscp_ecn = 0) noexcept {
        if (!peer.is_ipv4() || peer.is_any()) {
            return util::unexpected(errc::invalid_arg);
        }

        const auto normalized_local = icmp_detail::normalize_icmp_ipv4_local(netif, local);
        if (!normalized_local) {
            return util::unexpected(normalized_local.error());
        }

        const auto resolved = arp.lookup(peer);
        if (!resolved) {
            return util::unexpected(resolved.error());
        }

        return send_icmp_echo_ipv4_resolved<TxCapacity>(
            netif,
            resolved.value(),
            normalized_local.value(),
            peer,
            type,
            identifier,
            sequence,
            payload,
            ttl,
            identification,
            dscp_ecn);
    }

    template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
    [[nodiscard]] Result<void> send_icmp_echo_ipv4(NetIf& netif,
                                                   ArpService<ArpCapacity, ArpTxCapacity>& arp,
                                                   IpAddress local,
                                                   IpAddress peer,
                                                   IcmpType type,
                                                   util::u16 identifier,
                                                   util::u16 sequence,
                                                   ByteView payload,
                                                   util::u8 ttl = 64,
                                                   util::u16 identification = 0,
                                                   util::u8 dscp_ecn = 0) noexcept {
        if (!peer.is_ipv4() || peer.is_any()) {
            return util::unexpected(errc::invalid_arg);
        }

        const auto normalized_local = icmp_detail::normalize_icmp_ipv4_local(netif, local);
        if (!normalized_local) {
            return util::unexpected(normalized_local.error());
        }

        const auto resolved = arp.lookup_or_request(peer);
        if (!resolved) {
            return util::unexpected(resolved.error());
        }

        return send_icmp_echo_ipv4_resolved<TxCapacity>(
            netif,
            resolved.value(),
            normalized_local.value(),
            peer,
            type,
            identifier,
            sequence,
            payload,
            ttl,
            identification,
            dscp_ecn);
    }

    template <util::usize TxCapacity, util::usize ArpCapacity>
    [[nodiscard]] Result<void> send_icmp_echo_request(NetIf& netif,
                                                      const ArpTable<ArpCapacity>& arp,
                                                      IpAddress local,
                                                      IpAddress peer,
                                                      util::u16 identifier,
                                                      util::u16 sequence,
                                                      ByteView payload,
                                                      util::u8 ttl = 64,
                                                      util::u16 ipv4_identification = 0,
                                                      util::u8 dscp_ecn = 0) noexcept {
        return send_icmp_echo_ipv4<TxCapacity>(
            netif,
            arp,
            local,
            peer,
            IcmpType::echo_request,
            identifier,
            sequence,
            payload,
            ttl,
            ipv4_identification,
            dscp_ecn);
    }

    template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
    [[nodiscard]] Result<void> send_icmp_echo_request(NetIf& netif,
                                                      ArpService<ArpCapacity, ArpTxCapacity>& arp,
                                                      IpAddress local,
                                                      IpAddress peer,
                                                      util::u16 identifier,
                                                      util::u16 sequence,
                                                      ByteView payload,
                                                      util::u8 ttl = 64,
                                                      util::u16 ipv4_identification = 0,
                                                      util::u8 dscp_ecn = 0) noexcept {
        return send_icmp_echo_ipv4<TxCapacity>(
            netif,
            arp,
            local,
            peer,
            IcmpType::echo_request,
            identifier,
            sequence,
            payload,
            ttl,
            ipv4_identification,
            dscp_ecn);
    }

    template <util::usize TxCapacity, util::usize ArpCapacity>
    [[nodiscard]] Result<void> send_icmp_echo_reply(NetIf& netif,
                                                    const ArpTable<ArpCapacity>& arp,
                                                    IpAddress local,
                                                    IpAddress peer,
                                                    util::u16 identifier,
                                                    util::u16 sequence,
                                                    ByteView payload,
                                                    util::u8 ttl = 64,
                                                    util::u16 ipv4_identification = 0,
                                                    util::u8 dscp_ecn = 0) noexcept {
        return send_icmp_echo_ipv4<TxCapacity>(
            netif,
            arp,
            local,
            peer,
            IcmpType::echo_reply,
            identifier,
            sequence,
            payload,
            ttl,
            ipv4_identification,
            dscp_ecn);
    }

    template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
    [[nodiscard]] Result<void> send_icmp_echo_reply(NetIf& netif,
                                                    ArpService<ArpCapacity, ArpTxCapacity>& arp,
                                                    IpAddress local,
                                                    IpAddress peer,
                                                    util::u16 identifier,
                                                    util::u16 sequence,
                                                    ByteView payload,
                                                    util::u8 ttl = 64,
                                                    util::u16 ipv4_identification = 0,
                                                    util::u8 dscp_ecn = 0) noexcept {
        return send_icmp_echo_ipv4<TxCapacity>(
            netif,
            arp,
            local,
            peer,
            IcmpType::echo_reply,
            identifier,
            sequence,
            payload,
            ttl,
            ipv4_identification,
            dscp_ecn);
    }

    class IcmpEchoService {
    public:
        void set_sink(IcmpEchoSinkRef sink) noexcept {
            sink_ = sink;
        }

        template <IcmpEchoSink T>
        void set_sink(T& sink) noexcept {
            set_sink(make_icmp_echo_sink_ref(sink));
        }

        [[nodiscard]] bool has_sink() const noexcept {
            return sink_.valid();
        }

        [[nodiscard]] util::usize packet_count() const noexcept {
            return packet_count_;
        }

        [[nodiscard]] util::usize request_count() const noexcept {
            return request_count_;
        }

        [[nodiscard]] util::usize reply_count() const noexcept {
            return reply_count_;
        }

        [[nodiscard]] util::usize drop_count() const noexcept {
            return drop_count_;
        }

        [[nodiscard]] Result<void> consume(const Ipv4PacketView& ipv4, OwnedPacket packet) noexcept {
            const auto parsed = parse_icmp_packet(packet.view());
            if (!parsed) {
                return util::unexpected(parsed.error());
            }

            if (parsed.value().type != icmp_type_value(IcmpType::echo_request)
                && parsed.value().type != icmp_type_value(IcmpType::echo_reply)) {
                ++drop_count_;
                return {};
            }

            const auto echo = decode_icmp_echo(parsed.value());
            if (!echo) {
                return util::unexpected(echo.error());
            }

            auto trimmed_front = packet.trim_front(icmp_echo_header_size());
            if (!trimmed_front) {
                return util::unexpected(trimmed_front.error());
            }

            if (packet.view().size() < echo.value().payload.size()) {
                return util::unexpected(errc::invalid_format);
            }
            const auto excess_tail = packet.view().size() - echo.value().payload.size();
            if (excess_tail != 0u) {
                auto trimmed_back = packet.trim_back(excess_tail);
                if (!trimmed_back) {
                    return util::unexpected(trimmed_back.error());
                }
            }

            if (!sink_.valid()) {
                ++drop_count_;
                return {};
            }

            ++packet_count_;
            if (echo.value().type == IcmpType::echo_request) {
                ++request_count_;
            } else {
                ++reply_count_;
            }

            return sink_.consume(
                IcmpEchoInfo{
                    .type = echo.value().type,
                    .local = ipv4.destination,
                    .peer = ipv4.source,
                    .identifier = echo.value().identifier,
                    .sequence = echo.value().sequence,
                },
                static_cast<OwnedPacket&&>(packet));
        }

    private:
        IcmpEchoSinkRef sink_{};
        util::usize packet_count_{0};
        util::usize request_count_{0};
        util::usize reply_count_{0};
        util::usize drop_count_{0};
    };
}
