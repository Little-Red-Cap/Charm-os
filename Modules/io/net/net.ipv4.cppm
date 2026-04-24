module;

#include <array>
#include <concepts>

export module net.ipv4;

import net.arp;
import net.ether;
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

    template <typename T>
    concept Ipv4PacketSink = requires(T& t, const Ipv4PacketView& header, OwnedPacket packet) {
        { t.consume(header, static_cast<OwnedPacket&&>(packet)) } noexcept -> std::same_as<Result<void>>;
    };

    struct Ipv4PacketSinkOps {
        Result<void> (*consume)(void*, const Ipv4PacketView&, OwnedPacket) noexcept;
    };

    struct Ipv4PacketSinkRef {
        void* self{nullptr};
        const Ipv4PacketSinkOps* ops{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return self != nullptr && ops != nullptr && ops->consume != nullptr;
        }

        [[nodiscard]] Result<void> consume(const Ipv4PacketView& header, OwnedPacket packet) const noexcept {
            if (!valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            return ops->consume(self, header, static_cast<OwnedPacket&&>(packet));
        }
    };

    template <Ipv4PacketSink T>
    inline const Ipv4PacketSinkOps* ipv4_packet_sink_ops() noexcept {
        static const Ipv4PacketSinkOps ops{
            .consume = [](void* self, const Ipv4PacketView& header, OwnedPacket packet) noexcept {
                return static_cast<T*>(self)->consume(header, static_cast<OwnedPacket&&>(packet));
            }
        };
        return &ops;
    }

    template <Ipv4PacketSink T>
    inline Ipv4PacketSinkRef make_ipv4_packet_sink_ref(T& sink) noexcept {
        return Ipv4PacketSinkRef{&sink, ipv4_packet_sink_ops<T>()};
    }

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

    namespace ipv4_detail {
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

    [[nodiscard]] constexpr Result<Ipv4PacketView> parse_ipv4_packet_prefix(PacketView packet) noexcept {
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

        const auto total_length = ipv4_detail::load_be16(packet, 2);
        if (total_length < header_length) {
            return util::unexpected(errc::invalid_format);
        }

        const auto flags_fragment = ipv4_detail::load_be16(packet, 6);
        if ((flags_fragment & 0x8000u) != 0u) {
            return util::unexpected(errc::invalid_format);
        }
        if ((flags_fragment & (ipv4_more_fragments_flag() | ipv4_fragment_offset_mask())) != 0u) {
            return util::unexpected(errc::not_supported);
        }

        const auto header = packet.subspan(0, header_length);
        const auto expected_checksum = ipv4_detail::compute_header_checksum(header);
        const auto actual_checksum = ipv4_detail::load_be16(header, 10);
        if (actual_checksum != expected_checksum) {
            return util::unexpected(errc::invalid_format);
        }

        return Result<Ipv4PacketView>{std::in_place, Ipv4PacketView{
            .dscp_ecn = packet[1],
            .header_length = static_cast<util::u8>(header_length),
            .total_length = total_length,
            .identification = ipv4_detail::load_be16(packet, 4),
            .flags_fragment = flags_fragment,
            .ttl = packet[8],
            .protocol = static_cast<Ipv4Protocol>(packet[9]),
            .header_checksum = actual_checksum,
            .source = IpAddress::ipv4(packet[12], packet[13], packet[14], packet[15]),
            .destination = IpAddress::ipv4(packet[16], packet[17], packet[18], packet[19]),
            .options = packet.subspan(ipv4_min_header_size(), header_length - ipv4_min_header_size()),
            .payload = packet.subspan(header_length),
        }};
    }

    [[nodiscard]] constexpr Result<Ipv4PacketView> parse_ipv4_packet(PacketView packet) noexcept {
        const auto parsed = parse_ipv4_packet_prefix(packet);
        if (!parsed) {
            return util::unexpected(parsed.error());
        }

        if (parsed.value().total_length > packet.size()) {
            return util::unexpected(errc::invalid_format);
        }

        auto full = parsed.value();
        full.payload = packet.subspan(full.header_length, full.total_length - full.header_length);
        return Result<Ipv4PacketView>{std::in_place, full};
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> prepend_ipv4_header(PacketBuffer<Capacity>& packet,
                                                   const Ipv4PacketSpec& spec) noexcept {
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

        const auto total_length = header_length + packet.size();
        if (total_length > 0xFFFFu) {
            return util::unexpected(errc::buffer_overflow);
        }

        std::array<util::u8, ipv4_max_header_size()> header{};
        header[0] = static_cast<util::u8>((ipv4_version() << 4) | (header_length / 4u));
        header[1] = spec.dscp_ecn;
        ipv4_detail::store_be16(header.data() + 2, static_cast<util::u16>(total_length));
        ipv4_detail::store_be16(header.data() + 4, spec.identification);
        ipv4_detail::store_be16(header.data() + 6, spec.flags_fragment);
        header[8] = spec.ttl;
        header[9] = ipv4_protocol_value(spec.protocol);
        for (util::usize i = 0; i < 4; ++i) {
            header[12 + i] = spec.source.bytes[i];
            header[16 + i] = spec.destination.bytes[i];
        }
        for (util::usize i = 0; i < spec.options.size(); ++i) {
            header[ipv4_min_header_size() + i] = spec.options[i];
        }

        const auto checksum = ipv4_detail::compute_header_checksum(PacketView{
            ByteView{header.data(), header_length},
            0,
            0
        });
        ipv4_detail::store_be16(header.data() + 10, checksum);
        return packet.prepend(ByteView{header.data(), header_length});
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_ipv4_packet(PacketBuffer<Capacity>& packet,
                                                 const Ipv4PacketSpec& spec,
                                                 ByteView payload) noexcept {
        auto reset = packet.reset(ipv4_min_header_size() + spec.options.size());
        if (!reset) {
            return util::unexpected(reset.error());
        }

        auto appended_payload = packet.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }
        return prepend_ipv4_header(packet, spec);
    }

    enum class Ipv4SendDisposition : util::u8 {
        transmitted,
        queued,
    };

    struct Ipv4EgressProgress {
        util::usize arp_retried{0};
        util::usize arp_timed_out{0};
        util::usize flushed{0};
        util::usize dropped{0};
    };

    [[nodiscard]] Result<void> rewrite_ipv4_ttl(MutPacketView packet,
                                                util::u8 ttl) noexcept {
        if (ttl == 0u) {
            return util::unexpected(errc::invalid_arg);
        }

        const auto parsed = parse_ipv4_packet_prefix(packet);
        if (!parsed) {
            return util::unexpected(parsed.error());
        }
        if (parsed.value().total_length > packet.size()) {
            return util::unexpected(errc::invalid_format);
        }

        auto header = packet.subspan(0, parsed.value().header_length);
        header[8] = ttl;
        header[10] = 0u;
        header[11] = 0u;

        const auto checksum = ipv4_detail::compute_header_checksum(static_cast<PacketView>(header));
        ipv4_detail::store_be16(header.data() + 10, checksum);
        return {};
    }

    namespace ipv4_detail {
        [[nodiscard]] Result<IpAddress> normalize_ipv4_egress_target(
            const Ipv4PacketView& packet,
            IpAddress next_hop) noexcept {
            if (next_hop.is_unspecified() || next_hop.is_any()) {
                next_hop = packet.destination;
            }
            if (!next_hop.is_ipv4() || next_hop.is_any()) {
                return util::unexpected(errc::invalid_arg);
            }
            return Result<IpAddress>{std::in_place, next_hop};
        }
    }

    template <util::usize TxCapacity>
    [[nodiscard]] Result<void> send_ipv4_datagram_resolved(NetIf& netif,
                                                           MacAddress peer_mac,
                                                           ByteView packet) noexcept {
        const auto parsed = parse_ipv4_packet(PacketView{packet, 0, 0});
        if (!parsed) {
            return util::unexpected(parsed.error());
        }

        PacketBuffer<TxCapacity> frame{};
        auto reset = frame.reset(ether_header_size());
        if (!reset) {
            return util::unexpected(reset.error());
        }

        auto appended_ipv4 = frame.append(packet.subspan(0, parsed.value().total_length));
        if (!appended_ipv4) {
            return util::unexpected(appended_ipv4.error());
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
    [[nodiscard]] Result<void> send_ipv4_datagram(NetIf& netif,
                                                  const ArpTable<ArpCapacity>& arp,
                                                  ByteView packet,
                                                  IpAddress next_hop = {}) noexcept {
        const auto parsed = parse_ipv4_packet(PacketView{packet, 0, 0});
        if (!parsed) {
            return util::unexpected(parsed.error());
        }

        const auto target = ipv4_detail::normalize_ipv4_egress_target(parsed.value(), next_hop);
        if (!target) {
            return util::unexpected(target.error());
        }

        if (target.value().is_ipv4_limited_broadcast()) {
            if (!netif.supports(NetIfCapability::broadcast)) {
                return util::unexpected(errc::not_supported);
            }
            return send_ipv4_datagram_resolved<TxCapacity>(
                netif,
                MacAddress::broadcast(),
                packet.subspan(0, parsed.value().total_length));
        }

        const auto resolved = arp.lookup(target.value());
        if (!resolved) {
            return util::unexpected(resolved.error());
        }

        return send_ipv4_datagram_resolved<TxCapacity>(
            netif,
            resolved.value(),
            packet.subspan(0, parsed.value().total_length));
    }

    template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
    [[nodiscard]] Result<void> send_ipv4_datagram(NetIf& netif,
                                                  ArpService<ArpCapacity, ArpTxCapacity>& arp,
                                                  ByteView packet,
                                                  IpAddress next_hop = {}) noexcept {
        const auto parsed = parse_ipv4_packet(PacketView{packet, 0, 0});
        if (!parsed) {
            return util::unexpected(parsed.error());
        }

        const auto target = ipv4_detail::normalize_ipv4_egress_target(parsed.value(), next_hop);
        if (!target) {
            return util::unexpected(target.error());
        }

        if (target.value().is_ipv4_limited_broadcast()) {
            if (!netif.supports(NetIfCapability::broadcast)) {
                return util::unexpected(errc::not_supported);
            }
            return send_ipv4_datagram_resolved<TxCapacity>(
                netif,
                MacAddress::broadcast(),
                packet.subspan(0, parsed.value().total_length));
        }

        const auto resolved = arp.lookup_or_request(target.value());
        if (!resolved) {
            return util::unexpected(resolved.error());
        }

        return send_ipv4_datagram_resolved<TxCapacity>(
            netif,
            resolved.value(),
            packet.subspan(0, parsed.value().total_length));
    }

    template <util::usize TxCapacity>
    [[nodiscard]] Result<void> send_ipv4_resolved(NetIf& netif,
                                                  MacAddress peer_mac,
                                                  const Ipv4PacketSpec& spec,
                                                  ByteView payload) noexcept {
        PacketBuffer<TxCapacity> packet{};
        auto encoded = write_ipv4_packet(packet, spec, payload);
        if (!encoded) {
            return util::unexpected(encoded.error());
        }
        return send_ipv4_datagram_resolved<TxCapacity>(netif, peer_mac, packet.view().payload);
    }

    template <util::usize TxCapacity, util::usize ArpCapacity>
    [[nodiscard]] Result<void> send_ipv4(NetIf& netif,
                                         const ArpTable<ArpCapacity>& arp,
                                         const Ipv4PacketSpec& spec,
                                         ByteView payload,
                                         IpAddress next_hop = {}) noexcept {
        PacketBuffer<TxCapacity> packet{};
        auto encoded = write_ipv4_packet(packet, spec, payload);
        if (!encoded) {
            return util::unexpected(encoded.error());
        }
        return send_ipv4_datagram<TxCapacity>(netif, arp, packet.view().payload, next_hop);
    }

    template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
    [[nodiscard]] Result<void> send_ipv4(NetIf& netif,
                                         ArpService<ArpCapacity, ArpTxCapacity>& arp,
                                         const Ipv4PacketSpec& spec,
                                         ByteView payload,
                                         IpAddress next_hop = {}) noexcept {
        PacketBuffer<TxCapacity> packet{};
        auto encoded = write_ipv4_packet(packet, spec, payload);
        if (!encoded) {
            return util::unexpected(encoded.error());
        }
        return send_ipv4_datagram<TxCapacity>(netif, arp, packet.view().payload, next_hop);
    }

    template <util::usize PendingCapacity, util::usize PacketCapacity>
    class Ipv4EgressQueue {
    public:
        [[nodiscard]] util::usize pending_count() const noexcept {
            util::usize count = 0;
            for (const auto& entry : entries_) {
                if (entry.used) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] util::usize queued_count() const noexcept {
            return queued_count_;
        }

        [[nodiscard]] util::usize flushed_count() const noexcept {
            return flushed_count_;
        }

        [[nodiscard]] util::usize dropped_count() const noexcept {
            return dropped_count_;
        }

        template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
        [[nodiscard]] Result<Ipv4SendDisposition> send(
            NetIf& netif,
            ArpService<ArpCapacity, ArpTxCapacity>& arp,
            ByteView packet,
            IpAddress next_hop = {}) noexcept {
            auto sent = send_ipv4_datagram<TxCapacity>(netif, arp, packet, next_hop);
            if (sent) {
                return Result<Ipv4SendDisposition>{std::in_place, Ipv4SendDisposition::transmitted};
            }
            if (sent.error() != errc::again) {
                return util::unexpected(sent.error());
            }

            auto* entry = allocate_entry();
            if (entry == nullptr) {
                return util::unexpected(errc::buffer_overflow);
            }

            auto stored = store_entry(*entry, packet, next_hop);
            if (!stored) {
                *entry = {};
                return util::unexpected(stored.error());
            }

            ++queued_count_;
            return Result<Ipv4SendDisposition>{std::in_place, Ipv4SendDisposition::queued};
        }

        template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
        [[nodiscard]] Result<Ipv4SendDisposition> send(
            NetIf& netif,
            ArpService<ArpCapacity, ArpTxCapacity>& arp,
            const Ipv4PacketSpec& spec,
            ByteView payload,
            IpAddress next_hop = {}) noexcept {
            PacketBuffer<PacketCapacity> packet{};
            auto encoded = write_ipv4_packet(packet, spec, payload);
            if (!encoded) {
                return util::unexpected(encoded.error());
            }
            return send<TxCapacity>(netif, arp, packet.view().payload, next_hop);
        }

        template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
        [[nodiscard]] Result<util::usize> flush(
            NetIf& netif,
            ArpService<ArpCapacity, ArpTxCapacity>& arp) noexcept {
            util::usize flushed = 0;
            for (auto& entry : entries_) {
                if (!entry.used) {
                    continue;
                }

                auto sent = send_ipv4_datagram<TxCapacity>(
                    netif,
                    arp,
                    ByteView{entry.packet.data(), entry.packet_size},
                    entry.next_hop);
                if (sent) {
                    entry = {};
                    ++flushed;
                    ++flushed_count_;
                    continue;
                }
                if (sent.error() == errc::again) {
                    continue;
                }
                if (sent.error() == errc::timeout) {
                    entry = {};
                    ++dropped_count_;
                    continue;
                }
                return util::unexpected(sent.error());
            }
            return Result<util::usize>{std::in_place, flushed};
        }

        template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
        [[nodiscard]] Result<Ipv4EgressProgress> service(
            NetIf& netif,
            ArpService<ArpCapacity, ArpTxCapacity>& arp,
            util::usize elapsed_ticks = 1,
            util::usize retry_interval_ticks = 1,
            util::usize max_attempts = static_cast<util::usize>(-1)) noexcept {
            arp.advance_ticks(elapsed_ticks);

            auto arp_progress = arp.service_pending(retry_interval_ticks, max_attempts);
            if (!arp_progress) {
                return util::unexpected(arp_progress.error());
            }

            const auto dropped_before = dropped_count_;
            auto flushed = flush<TxCapacity>(netif, arp);
            if (!flushed) {
                return util::unexpected(flushed.error());
            }

            return Result<Ipv4EgressProgress>{std::in_place, Ipv4EgressProgress{
                .arp_retried = arp_progress.value().retried,
                .arp_timed_out = arp_progress.value().timed_out,
                .flushed = flushed.value(),
                .dropped = dropped_count_ - dropped_before,
            }};
        }

    private:
        struct PendingEntry {
            bool used{false};
            IpAddress next_hop{};
            util::usize packet_size{0};
            std::array<util::u8, PacketCapacity> packet{};
        };

        [[nodiscard]] PendingEntry* allocate_entry() noexcept {
            for (auto& entry : entries_) {
                if (entry.used) {
                    continue;
                }
                entry = {};
                entry.used = true;
                return &entry;
            }
            return nullptr;
        }

        [[nodiscard]] Result<void> store_entry(PendingEntry& entry,
                                               ByteView packet,
                                               IpAddress next_hop) noexcept {
            if (packet.size() > PacketCapacity) {
                return util::unexpected(errc::buffer_overflow);
            }

            entry.next_hop = next_hop;
            entry.packet_size = packet.size();
            for (util::usize i = 0; i < packet.size(); ++i) {
                entry.packet[i] = packet[i];
            }
            return {};
        }

        std::array<PendingEntry, PendingCapacity> entries_{};
        util::usize queued_count_{0};
        util::usize flushed_count_{0};
        util::usize dropped_count_{0};
    };

    class Ipv4Service {
    public:
        Ipv4Service() noexcept = default;

        explicit Ipv4Service(NetIf& netif) noexcept
            : netif_(&netif) {}

        void bind(NetIf& netif) noexcept {
            netif_ = &netif;
        }

        void set_icmp_sink(Ipv4PacketSinkRef sink) noexcept {
            icmp_sink_ = sink;
        }

        void set_udp_sink(Ipv4PacketSinkRef sink) noexcept {
            udp_sink_ = sink;
        }

        void set_tcp_sink(Ipv4PacketSinkRef sink) noexcept {
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
            const auto& datagram = parsed.value();

            if (netif_ != nullptr
                && netif_->address().is_ipv4()
                && !ipv4_detail::same_ipv4_address(datagram.destination, netif_->address())
                && !(netif_->supports(NetIfCapability::broadcast)
                     && datagram.destination.is_ipv4_limited_broadcast())) {
                ++drop_count_;
                return {};
            }

            auto trimmed = packet.trim_front(datagram.header_length);
            if (!trimmed) {
                return util::unexpected(trimmed.error());
            }

            if (packet.view().size() < datagram.payload.size()) {
                return util::unexpected(errc::invalid_format);
            }
            const auto excess_tail = packet.view().size() - datagram.payload.size();
            if (excess_tail != 0u) {
                auto trimmed_back = packet.trim_back(excess_tail);
                if (!trimmed_back) {
                    return util::unexpected(trimmed_back.error());
                }
            }

            switch (datagram.protocol) {
                case Ipv4Protocol::icmp:
                    if (!icmp_sink_.valid()) {
                        return util::unexpected(errc::not_supported);
                    }
                    ++packet_count_;
                    return icmp_sink_.consume(datagram, static_cast<OwnedPacket&&>(packet));
                case Ipv4Protocol::udp:
                    if (!udp_sink_.valid()) {
                        return util::unexpected(errc::not_supported);
                    }
                    ++packet_count_;
                    return udp_sink_.consume(datagram, static_cast<OwnedPacket&&>(packet));
                case Ipv4Protocol::tcp:
                    if (!tcp_sink_.valid()) {
                        return util::unexpected(errc::not_supported);
                    }
                    ++packet_count_;
                    return tcp_sink_.consume(datagram, static_cast<OwnedPacket&&>(packet));
                default:
                    return util::unexpected(errc::not_supported);
            }
        }

    private:
        NetIf* netif_{nullptr};
        Ipv4PacketSinkRef icmp_sink_{};
        Ipv4PacketSinkRef udp_sink_{};
        Ipv4PacketSinkRef tcp_sink_{};
        util::usize packet_count_{0};
        util::usize drop_count_{0};
    };
}
