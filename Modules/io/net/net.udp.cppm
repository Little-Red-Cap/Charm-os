module;

#include <array>
#include <concepts>

export module net.udp;

import net.arp;
import net.common;
import net.ether;
import net.ipv4;
import net.netif;
import net.packet;
import util.core;
import util.error;
import util.expected;

export namespace net {
    struct UdpDatagramView {
        util::u16 source_port{0};
        util::u16 destination_port{0};
        util::u16 length{0};
        util::u16 checksum{0};
        PacketView payload{};
    };

    struct UdpDatagramInfo {
        Endpoint local{};
        Endpoint peer{};
        util::u16 length{0};
        util::u16 checksum{0};
    };

    enum class UdpSendDisposition : util::u8 {
        transmitted,
        queued,
    };

    using UdpSendFn = Result<UdpSendDisposition> (*)(void* ctx,
                                                     Endpoint local,
                                                     const Endpoint& peer,
                                                     ByteView payload) noexcept;

    struct UdpEgressProgress {
        util::usize arp_retried{0};
        util::usize arp_timed_out{0};
        util::usize flushed{0};
        util::usize dropped{0};
    };

    template <typename T>
    concept UdpDatagramSink = requires(T& t, const UdpDatagramInfo& info, OwnedPacket packet) {
        { t.consume(info, static_cast<OwnedPacket&&>(packet)) } noexcept -> std::same_as<Result<void>>;
    };

    struct UdpDatagramSinkOps {
        Result<void> (*consume)(void*, const UdpDatagramInfo&, OwnedPacket) noexcept;
    };

    struct UdpDatagramSinkRef {
        void* self{nullptr};
        const UdpDatagramSinkOps* ops{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return self != nullptr && ops != nullptr && ops->consume != nullptr;
        }

        [[nodiscard]] Result<void> consume(const UdpDatagramInfo& info, OwnedPacket packet) const noexcept {
            if (!valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            return ops->consume(self, info, static_cast<OwnedPacket&&>(packet));
        }
    };

    template <UdpDatagramSink T>
    inline const UdpDatagramSinkOps* udp_datagram_sink_ops() noexcept {
        static const UdpDatagramSinkOps ops{
            .consume = [](void* self, const UdpDatagramInfo& info, OwnedPacket packet) noexcept {
                return static_cast<T*>(self)->consume(info, static_cast<OwnedPacket&&>(packet));
            }
        };
        return &ops;
    }

    template <UdpDatagramSink T>
    inline UdpDatagramSinkRef make_udp_datagram_sink_ref(T& sink) noexcept {
        return UdpDatagramSinkRef{&sink, udp_datagram_sink_ops<T>()};
    }

    class UdpSenderRef {
    public:
        constexpr UdpSenderRef() noexcept = default;

        static constexpr UdpSenderRef raw(UdpSendFn send, void* ctx) noexcept {
            return UdpSenderRef{send, ctx};
        }

        template <typename Sender>
            requires(
                requires(Sender& value, Endpoint local, const Endpoint& peer, ByteView payload) {
                    { value.send(local, peer, payload) } noexcept -> std::same_as<Result<UdpSendDisposition>>;
                } ||
                requires(Sender& value, Endpoint local, const Endpoint& peer, ByteView payload) {
                    { value(local, peer, payload) } noexcept -> std::same_as<Result<UdpSendDisposition>>;
                })
        static constexpr UdpSenderRef bind(Sender& sender) noexcept {
            return UdpSenderRef{&invoke<Sender>, &sender};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return send_ != nullptr;
        }

        [[nodiscard]] Result<UdpSendDisposition> send(Endpoint local,
                                                      const Endpoint& peer,
                                                      ByteView payload) const noexcept {
            if (!send_) {
                return util::unexpected(errc::bad_state);
            }
            return send_(ctx_, local, peer, payload);
        }

    private:
        constexpr UdpSenderRef(UdpSendFn send, void* ctx) noexcept
            : send_(send),
              ctx_(ctx) {
        }

        template <typename Sender>
        static Result<UdpSendDisposition> invoke(void* ctx,
                                                 Endpoint local,
                                                 const Endpoint& peer,
                                                 ByteView payload) noexcept {
            auto* sender = static_cast<Sender*>(ctx);
            if (!sender) {
                return util::unexpected(errc::bad_state);
            }
            if constexpr (requires(Sender& value,
                                   Endpoint source,
                                   const Endpoint& destination,
                                   ByteView bytes) {
                              {
                                  value.send(source, destination, bytes)
                              } noexcept -> std::same_as<Result<UdpSendDisposition>>;
                          }) {
                return sender->send(local, peer, payload);
            } else {
                return (*sender)(local, peer, payload);
            }
        }

        UdpSendFn send_{nullptr};
        void* ctx_{nullptr};
    };

    [[nodiscard]] constexpr util::usize udp_header_size() noexcept {
        return 8u;
    }

    namespace udp_detail {
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

        [[nodiscard]] constexpr util::u16 compute_udp_checksum_ipv4(const Ipv4PacketView& ipv4,
                                                                    PacketView datagram) noexcept {
            util::u32 sum = 0;
            sum = accumulate_checksum(sum, ByteView{ipv4.source.bytes.data(), 4});
            sum = accumulate_checksum(sum, ByteView{ipv4.destination.bytes.data(), 4});
            sum += ipv4_protocol_value(Ipv4Protocol::udp);
            sum += static_cast<util::u16>(datagram.size());
            sum += load_be16(datagram, 0);
            sum += load_be16(datagram, 2);
            sum += load_be16(datagram, 4);
            sum = accumulate_checksum(sum, datagram.payload.subspan(udp_header_size()));
            return static_cast<util::u16>(~fold_checksum(sum));
        }

        [[nodiscard]] constexpr bool is_concrete_ipv4_endpoint(const Endpoint& endpoint) noexcept {
            return endpoint.address.is_ipv4() && !endpoint.address.is_any();
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

        [[nodiscard]] constexpr bool is_ipv4_limited_broadcast_endpoint(
            const Endpoint& endpoint) noexcept {
            return endpoint.address.is_ipv4_limited_broadcast() && endpoint.port != 0u;
        }

        [[nodiscard]] Result<Endpoint> normalize_udp_ipv4_local(const NetIf& netif, Endpoint local) noexcept {
            if (local.port == 0u) {
                return util::unexpected(errc::invalid_arg);
            }

            if (local.address.is_unspecified() || local.address.is_any()) {
                local.address = netif.address();
            }
            if (!local.address.is_ipv4() || !same_ipv4_address(local.address, netif.address())) {
                return util::unexpected(errc::invalid_arg);
            }
            return Result<Endpoint>{std::in_place, local};
        }
    }

    [[nodiscard]] constexpr Result<UdpDatagramView> parse_udp_datagram(PacketView packet) noexcept {
        if (packet.size() < udp_header_size()) {
            return util::unexpected(errc::invalid_format);
        }

        const auto length = udp_detail::load_be16(packet, 4);
        if (length < udp_header_size() || length > packet.size()) {
            return util::unexpected(errc::invalid_format);
        }

        return Result<UdpDatagramView>{std::in_place, UdpDatagramView{
            .source_port = udp_detail::load_be16(packet, 0),
            .destination_port = udp_detail::load_be16(packet, 2),
            .length = length,
            .checksum = udp_detail::load_be16(packet, 6),
            .payload = packet.subspan(udp_header_size(), length - udp_header_size()),
        }};
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> prepend_udp_ipv4_header(PacketBuffer<Capacity>& packet,
                                                       const Endpoint& local,
                                                       const Endpoint& peer) noexcept {
        if (!udp_detail::is_concrete_ipv4_endpoint(local)
            || !udp_detail::is_concrete_ipv4_endpoint(peer)
            || local.port == 0u
            || peer.port == 0u) {
            return util::unexpected(errc::invalid_arg);
        }

        const auto datagram_size = udp_header_size() + packet.size();
        if (datagram_size > 0xFFFFu) {
            return util::unexpected(errc::buffer_overflow);
        }

        std::array<util::u8, udp_header_size()> header{};
        udp_detail::store_be16(header.data(), local.port);
        udp_detail::store_be16(header.data() + 2, peer.port);
        udp_detail::store_be16(header.data() + 4, static_cast<util::u16>(datagram_size));

        auto prepended_header = packet.prepend(ByteView{header.data(), header.size()});
        if (!prepended_header) {
            return util::unexpected(prepended_header.error());
        }

        const auto checksum = udp_detail::compute_udp_checksum_ipv4(
            Ipv4PacketView{
                .protocol = Ipv4Protocol::udp,
                .source = local.address,
                .destination = peer.address,
            },
            packet.view());
        const auto wire_checksum = checksum == 0u ? static_cast<util::u16>(0xFFFFu) : checksum;
        auto out = packet.mut_view();
        udp_detail::store_be16(out.data() + 6, wire_checksum);
        return {};
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_udp_ipv4_datagram(PacketBuffer<Capacity>& packet,
                                                       const Endpoint& local,
                                                       const Endpoint& peer,
                                                       ByteView payload) noexcept {
        auto reset = packet.reset(udp_header_size());
        if (!reset) {
            return util::unexpected(reset.error());
        }

        auto appended_payload = packet.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }
        return prepend_udp_ipv4_header(packet, local, peer);
    }

    template <util::usize TxCapacity>
    [[nodiscard]] Result<void> send_udp_ipv4_resolved(NetIf& netif,
                                                      MacAddress peer_mac,
                                                      Endpoint local,
                                                      const Endpoint& peer,
                                                      ByteView payload,
                                                      util::u8 ttl,
                                                      util::u16 identification,
                                                      util::u8 dscp_ecn) noexcept {
        PacketBuffer<TxCapacity> frame{};
        auto reset = frame.reset(ether_header_size() + ipv4_min_header_size() + udp_header_size());
        if (!reset) {
            return util::unexpected(reset.error());
        }

        auto appended_payload = frame.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }

        auto prepended_udp = prepend_udp_ipv4_header(frame, local, peer);
        if (!prepended_udp) {
            return util::unexpected(prepended_udp.error());
        }

        auto prepended_ipv4 = prepend_ipv4_header(frame, Ipv4PacketSpec{
            .dscp_ecn = dscp_ecn,
            .identification = identification,
            .flags_fragment = ipv4_do_not_fragment_flag(),
            .ttl = ttl,
            .protocol = Ipv4Protocol::udp,
            .source = local.address,
            .destination = peer.address,
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
    [[nodiscard]] Result<void> send_udp_ipv4(NetIf& netif,
                                             const ArpTable<ArpCapacity>& arp,
                                             Endpoint local,
                                             const Endpoint& peer,
                                             ByteView payload,
                                             util::u8 ttl = 64,
                                             util::u16 identification = 0,
                                             util::u8 dscp_ecn = 0) noexcept {
        if (!peer.address.is_ipv4() || peer.address.is_any() || peer.port == 0u) {
            return util::unexpected(errc::invalid_arg);
        }

        const auto normalized_local = udp_detail::normalize_udp_ipv4_local(netif, local);
        if (!normalized_local) {
            return util::unexpected(normalized_local.error());
        }

        if (udp_detail::is_ipv4_limited_broadcast_endpoint(peer)) {
            if (!netif.supports(NetIfCapability::broadcast)) {
                return util::unexpected(errc::not_supported);
            }
            return send_udp_ipv4_resolved<TxCapacity>(
                netif,
                MacAddress::broadcast(),
                normalized_local.value(),
                peer,
                payload,
                ttl,
                identification,
                dscp_ecn);
        }

        const auto resolved = arp.lookup(peer.address);
        if (!resolved) {
            return util::unexpected(resolved.error());
        }

        return send_udp_ipv4_resolved<TxCapacity>(
            netif,
            resolved.value(),
            normalized_local.value(),
            peer,
            payload,
            ttl,
            identification,
            dscp_ecn);
    }

    template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
    [[nodiscard]] Result<void> send_udp_ipv4(NetIf& netif,
                                             ArpService<ArpCapacity, ArpTxCapacity>& arp,
                                             Endpoint local,
                                             const Endpoint& peer,
                                             ByteView payload,
                                             util::u8 ttl = 64,
                                             util::u16 identification = 0,
                                             util::u8 dscp_ecn = 0) noexcept {
        if (!peer.address.is_ipv4() || peer.address.is_any() || peer.port == 0u) {
            return util::unexpected(errc::invalid_arg);
        }

        const auto normalized_local = udp_detail::normalize_udp_ipv4_local(netif, local);
        if (!normalized_local) {
            return util::unexpected(normalized_local.error());
        }

        if (udp_detail::is_ipv4_limited_broadcast_endpoint(peer)) {
            if (!netif.supports(NetIfCapability::broadcast)) {
                return util::unexpected(errc::not_supported);
            }
            return send_udp_ipv4_resolved<TxCapacity>(
                netif,
                MacAddress::broadcast(),
                normalized_local.value(),
                peer,
                payload,
                ttl,
                identification,
                dscp_ecn);
        }

        const auto resolved = arp.lookup_or_request(peer.address);
        if (!resolved) {
            return util::unexpected(resolved.error());
        }

        return send_udp_ipv4_resolved<TxCapacity>(
            netif,
            resolved.value(),
            normalized_local.value(),
            peer,
            payload,
            ttl,
            identification,
            dscp_ecn);
    }

    template <util::usize PendingCapacity, util::usize PayloadCapacity>
    class UdpEgressQueue {
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
        [[nodiscard]] Result<UdpSendDisposition> send(NetIf& netif,
                                                      ArpService<ArpCapacity, ArpTxCapacity>& arp,
                                                      Endpoint local,
                                                      const Endpoint& peer,
                                                      ByteView payload,
                                                      util::u8 ttl = 64,
                                                      util::u16 identification = 0,
                                                      util::u8 dscp_ecn = 0) noexcept {
            auto sent = send_udp_ipv4<TxCapacity>(
                netif,
                arp,
                local,
                peer,
                payload,
                ttl,
                identification,
                dscp_ecn);
            if (sent) {
                return Result<UdpSendDisposition>{std::in_place, UdpSendDisposition::transmitted};
            }
            if (sent.error() != errc::again) {
                return util::unexpected(sent.error());
            }

            auto* entry = allocate_entry();
            if (entry == nullptr) {
                return util::unexpected(errc::buffer_overflow);
            }

            auto stored = store_entry(*entry, local, peer, payload, ttl, identification, dscp_ecn);
            if (!stored) {
                *entry = {};
                return util::unexpected(stored.error());
            }

            ++queued_count_;
            return Result<UdpSendDisposition>{std::in_place, UdpSendDisposition::queued};
        }

        template <util::usize TxCapacity, util::usize ArpCapacity, util::usize ArpTxCapacity>
        [[nodiscard]] Result<util::usize> flush(NetIf& netif,
                                                ArpService<ArpCapacity, ArpTxCapacity>& arp) noexcept {
            util::usize flushed = 0;
            for (auto& entry : entries_) {
                if (!entry.used) {
                    continue;
                }

                auto sent = send_udp_ipv4<TxCapacity>(
                    netif,
                    arp,
                    entry.local,
                    entry.peer,
                    ByteView{entry.payload.data(), entry.payload_size},
                    entry.ttl,
                    entry.identification,
                    entry.dscp_ecn);
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
        [[nodiscard]] Result<UdpEgressProgress> service(NetIf& netif,
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

            return Result<UdpEgressProgress>{std::in_place, UdpEgressProgress{
                .arp_retried = arp_progress.value().retried,
                .arp_timed_out = arp_progress.value().timed_out,
                .flushed = flushed.value(),
                .dropped = dropped_count_ - dropped_before,
            }};
        }

    private:
        struct PendingEntry {
            bool used{false};
            Endpoint local{};
            Endpoint peer{};
            util::u8 ttl{64};
            util::u16 identification{0};
            util::u8 dscp_ecn{0};
            util::usize payload_size{0};
            std::array<util::u8, PayloadCapacity> payload{};
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
                                               Endpoint local,
                                               Endpoint peer,
                                               ByteView payload,
                                               util::u8 ttl,
                                               util::u16 identification,
                                               util::u8 dscp_ecn) noexcept {
            if (payload.size() > PayloadCapacity) {
                return util::unexpected(errc::buffer_overflow);
            }

            entry.local = local;
            entry.peer = peer;
            entry.ttl = ttl;
            entry.identification = identification;
            entry.dscp_ecn = dscp_ecn;
            entry.payload_size = payload.size();
            for (util::usize i = 0; i < payload.size(); ++i) {
                entry.payload[i] = payload[i];
            }
            return {};
        }

        std::array<PendingEntry, PendingCapacity> entries_{};
        util::usize queued_count_{0};
        util::usize flushed_count_{0};
        util::usize dropped_count_{0};
    };

    template <util::usize Capacity>
    class UdpService {
    public:
        [[nodiscard]] Result<void> bind(util::u16 local_port, UdpDatagramSinkRef sink) noexcept {
            if (local_port == 0u || !sink.valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            if (find_binding(local_port) != invalid_index()) {
                return util::unexpected(errc::exist);
            }

            for (auto& binding : bindings_) {
                if (binding.used) {
                    continue;
                }
                binding.used = true;
                binding.local_port = local_port;
                binding.sink = sink;
                return {};
            }
            return util::unexpected(errc::buffer_overflow);
        }

        template <UdpDatagramSink T>
        [[nodiscard]] Result<void> bind(util::u16 local_port, T& sink) noexcept {
            return bind(local_port, make_udp_datagram_sink_ref(sink));
        }

        [[nodiscard]] bool has_binding(util::u16 local_port) const noexcept {
            return find_binding(local_port) != invalid_index();
        }

        [[nodiscard]] bool unbind(util::u16 local_port) noexcept {
            const auto binding_index = find_binding(local_port);
            if (binding_index == invalid_index()) {
                return false;
            }

            bindings_[binding_index] = {};
            return true;
        }

        [[nodiscard]] util::usize binding_count() const noexcept {
            util::usize count = 0;
            for (const auto& binding : bindings_) {
                if (binding.used) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] util::usize packet_count() const noexcept {
            return packet_count_;
        }

        [[nodiscard]] util::usize drop_count() const noexcept {
            return drop_count_;
        }

        [[nodiscard]] Result<void> consume(const Ipv4PacketView& ipv4, OwnedPacket packet) noexcept {
            const auto parsed = parse_udp_datagram(packet.view());
            if (!parsed) {
                return util::unexpected(parsed.error());
            }
            const auto& datagram = parsed.value();

            const auto exact_datagram = packet.view().subspan(0, datagram.length);
            if (datagram.checksum != 0u) {
                auto expected = udp_detail::compute_udp_checksum_ipv4(ipv4, exact_datagram);
                if (expected == 0u) {
                    expected = 0xFFFFu;
                }
                if (expected != datagram.checksum) {
                    return util::unexpected(errc::invalid_format);
                }
            }

            const auto binding_index = find_binding(datagram.destination_port);
            if (binding_index == invalid_index()) {
                ++drop_count_;
                return {};
            }

            auto trimmed_front = packet.trim_front(udp_header_size());
            if (!trimmed_front) {
                return util::unexpected(trimmed_front.error());
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

            ++packet_count_;
            return bindings_[binding_index].sink.consume(
                UdpDatagramInfo{
                    .local = Endpoint{ipv4.destination, datagram.destination_port},
                    .peer = Endpoint{ipv4.source, datagram.source_port},
                    .length = datagram.length,
                    .checksum = datagram.checksum,
                },
                static_cast<OwnedPacket&&>(packet));
        }

    private:
        struct Binding {
            bool used{false};
            util::u16 local_port{0};
            UdpDatagramSinkRef sink{};
        };

        static constexpr util::usize invalid_index() noexcept {
            return static_cast<util::usize>(-1);
        }

        [[nodiscard]] util::usize find_binding(util::u16 local_port) const noexcept {
            for (util::usize i = 0; i < bindings_.size(); ++i) {
                if (!bindings_[i].used) {
                    continue;
                }
                if (bindings_[i].local_port == local_port) {
                    return i;
                }
            }
            return invalid_index();
        }

        std::array<Binding, Capacity> bindings_{};
        util::usize packet_count_{0};
        util::usize drop_count_{0};
    };
}
