module;

#include <array>

export module net.arp;

import net.ether;
import net.netif;
import net.packet;
import util.core;
import util.error;
import util.expected;

export namespace net {
    enum class ArpOperation : util::u16 {
        request = 1,
        reply = 2,
    };

    struct ArpPacketView {
        ArpOperation operation{ArpOperation::request};
        MacAddress sender_mac{};
        IpAddress sender_ip{};
        MacAddress target_mac{};
        IpAddress target_ip{};
    };

    struct ArpRetryProgress {
        util::usize retried{0};
        util::usize timed_out{0};
    };

    [[nodiscard]] constexpr util::u16 arp_hardware_type_ethernet() noexcept {
        return 1;
    }

    [[nodiscard]] constexpr util::u16 arp_protocol_type_ipv4() noexcept {
        return ether_type_value(EtherType::ipv4);
    }

    [[nodiscard]] constexpr util::usize arp_ipv4_ethernet_size() noexcept {
        return 28;
    }

    [[nodiscard]] constexpr bool is_same_ipv4_address(const IpAddress& lhs, const IpAddress& rhs) noexcept {
        if (lhs.family != AddressFamily::ipv4 || rhs.family != AddressFamily::ipv4) {
            return false;
        }
        for (util::usize i = 0; i < 4; ++i) {
            if (lhs.bytes[i] != rhs.bytes[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] constexpr bool is_same_mac(const MacAddress& lhs, const MacAddress& rhs) noexcept {
        for (util::usize i = 0; i < lhs.bytes.size(); ++i) {
            if (lhs.bytes[i] != rhs.bytes[i]) {
                return false;
            }
        }
        return true;
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
    }

    [[nodiscard]] constexpr Result<ArpPacketView> parse_arp_ipv4_ethernet(PacketView packet) noexcept {
        if (packet.size() < arp_ipv4_ethernet_size()) {
            return util::unexpected(errc::invalid_format);
        }

        if (detail::load_be16(packet, 0) != arp_hardware_type_ethernet()
            || detail::load_be16(packet, 2) != arp_protocol_type_ipv4()
            || packet[4] != 6u
            || packet[5] != 4u) {
            return util::unexpected(errc::not_supported);
        }

        const auto raw_op = detail::load_be16(packet, 6);
        if (raw_op != static_cast<util::u16>(ArpOperation::request)
            && raw_op != static_cast<util::u16>(ArpOperation::reply)) {
            return util::unexpected(errc::invalid_format);
        }

        return Result<ArpPacketView>{std::in_place, ArpPacketView{
            .operation = static_cast<ArpOperation>(raw_op),
            .sender_mac = MacAddress::from_bytes(
                packet[8], packet[9], packet[10], packet[11], packet[12], packet[13]),
            .sender_ip = IpAddress::ipv4(packet[14], packet[15], packet[16], packet[17]),
            .target_mac = MacAddress::from_bytes(
                packet[18], packet[19], packet[20], packet[21], packet[22], packet[23]),
            .target_ip = IpAddress::ipv4(packet[24], packet[25], packet[26], packet[27]),
        }};
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_arp_ipv4_ethernet_frame(PacketBuffer<Capacity>& packet,
                                                             ArpOperation operation,
                                                             MacAddress ethernet_destination,
                                                             MacAddress sender_mac,
                                                             IpAddress sender_ip,
                                                             MacAddress target_mac,
                                                             IpAddress target_ip) noexcept {
        if (!sender_ip.is_ipv4() || !target_ip.is_ipv4()) {
            return util::unexpected(errc::not_supported);
        }

        auto reset = packet.reset();
        if (!reset) {
            return util::unexpected(reset.error());
        }

        std::array<util::u8, 14> ether_header{};
        for (util::usize i = 0; i < 6; ++i) {
            ether_header[i] = ethernet_destination.bytes[i];
            ether_header[6 + i] = sender_mac.bytes[i];
        }
        detail::store_be16(ether_header.data() + 12, ether_type_value(EtherType::arp));

        std::array<util::u8, arp_ipv4_ethernet_size()> arp_payload{};
        detail::store_be16(arp_payload.data(), arp_hardware_type_ethernet());
        detail::store_be16(arp_payload.data() + 2, arp_protocol_type_ipv4());
        arp_payload[4] = 6u;
        arp_payload[5] = 4u;
        detail::store_be16(arp_payload.data() + 6, static_cast<util::u16>(operation));
        for (util::usize i = 0; i < 6; ++i) {
            arp_payload[8 + i] = sender_mac.bytes[i];
            arp_payload[18 + i] = target_mac.bytes[i];
        }
        for (util::usize i = 0; i < 4; ++i) {
            arp_payload[14 + i] = sender_ip.bytes[i];
            arp_payload[24 + i] = target_ip.bytes[i];
        }

        auto appended_ether = packet.append(ByteView{ether_header.data(), ether_header.size()});
        if (!appended_ether) {
            return util::unexpected(appended_ether.error());
        }
        auto appended_arp = packet.append(ByteView{arp_payload.data(), arp_payload.size()});
        if (!appended_arp) {
            return util::unexpected(appended_arp.error());
        }
        return {};
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_arp_ipv4_request_frame(PacketBuffer<Capacity>& packet,
                                                            MacAddress sender_mac,
                                                            IpAddress sender_ip,
                                                            IpAddress target_ip) noexcept {
        return write_arp_ipv4_ethernet_frame(
            packet,
            ArpOperation::request,
            MacAddress::broadcast(),
            sender_mac,
            sender_ip,
            {},
            target_ip);
    }

    template <util::usize TxCapacity>
    [[nodiscard]] Result<void> send_arp_ipv4_request(NetIf& netif, IpAddress target_ip) noexcept {
        const auto local_ip = netif.address();
        if (!local_ip.is_ipv4() || local_ip.is_any()) {
            return util::unexpected(errc::invalid_arg);
        }
        if (!target_ip.is_ipv4() || target_ip.is_any()) {
            return util::unexpected(errc::invalid_arg);
        }

        PacketBuffer<TxCapacity> request{};
        auto encoded = write_arp_ipv4_request_frame(
            request,
            netif.mac(),
            local_ip,
            target_ip);
        if (!encoded) {
            return util::unexpected(encoded.error());
        }
        return netif.transmit(request.view());
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_arp_ipv4_reply_frame(PacketBuffer<Capacity>& packet,
                                                          MacAddress destination_mac,
                                                          MacAddress sender_mac,
                                                          IpAddress sender_ip,
                                                          MacAddress target_mac,
                                                          IpAddress target_ip) noexcept {
        return write_arp_ipv4_ethernet_frame(
            packet,
            ArpOperation::reply,
            destination_mac,
            sender_mac,
            sender_ip,
            target_mac,
            target_ip);
    }

    template <util::usize Capacity>
    class ArpTable {
    public:
        struct Entry {
            bool used{false};
            IpAddress ip{};
            MacAddress mac{};
        };

        [[nodiscard]] Result<void> remember(IpAddress ip, MacAddress mac) noexcept {
            if (!ip.is_ipv4()) {
                return util::unexpected(errc::not_supported);
            }
            if (mac.is_zero()) {
                return util::unexpected(errc::invalid_arg);
            }

            for (auto& entry : entries_) {
                if (!entry.used) {
                    continue;
                }
                if (!is_same_ipv4_address(entry.ip, ip)) {
                    continue;
                }
                entry.mac = mac;
                return {};
            }

            for (auto& entry : entries_) {
                if (entry.used) {
                    continue;
                }
                entry.used = true;
                entry.ip = ip;
                entry.mac = mac;
                return {};
            }
            return util::unexpected(errc::buffer_overflow);
        }

        [[nodiscard]] Result<MacAddress> lookup(IpAddress ip) const noexcept {
            if (!ip.is_ipv4()) {
                return util::unexpected(errc::not_supported);
            }
            for (const auto& entry : entries_) {
                if (!entry.used) {
                    continue;
                }
                if (!is_same_ipv4_address(entry.ip, ip)) {
                    continue;
                }
                return Result<MacAddress>{std::in_place, entry.mac};
            }
            return util::unexpected(errc::noent);
        }

        [[nodiscard]] util::usize entry_count() const noexcept {
            util::usize count = 0;
            for (const auto& entry : entries_) {
                if (entry.used) {
                    ++count;
                }
            }
            return count;
        }

    private:
        std::array<Entry, Capacity> entries_{};
    };

    template <util::usize TableCapacity, util::usize TxCapacity>
    class ArpService {
    public:
        ArpService() noexcept = default;

        explicit ArpService(NetIf& netif) noexcept
            : netif_(&netif) {}

        void bind(NetIf& netif) noexcept {
            netif_ = &netif;
        }

        [[nodiscard]] const ArpTable<TableCapacity>& table() const noexcept {
            return table_;
        }

        [[nodiscard]] util::usize pending_count() const noexcept {
            util::usize count = 0;
            for (const auto& pending : pending_) {
                if (pending.used && !pending.failed) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] util::usize failed_count() const noexcept {
            util::usize count = 0;
            for (const auto& pending : pending_) {
                if (pending.used && pending.failed) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] util::usize pending_attempts(IpAddress ip) const noexcept {
            const auto index = find_entry(ip);
            if (index == invalid_index()) {
                return 0;
            }
            return pending_[index].attempts;
        }

        [[nodiscard]] util::usize request_count() const noexcept {
            return request_count_;
        }

        [[nodiscard]] util::usize reply_count() const noexcept {
            return reply_count_;
        }

        [[nodiscard]] util::usize tick_count() const noexcept {
            return tick_count_;
        }

        void advance_ticks(util::usize ticks = 1) noexcept {
            tick_count_ += ticks;
        }

        [[nodiscard]] Result<util::usize> retry_pending_requests(
            util::usize max_attempts = static_cast<util::usize>(-1)) noexcept {
            if (netif_ == nullptr) {
                return util::unexpected(errc::bad_state);
            }
            if (max_attempts == 0u) {
                return util::unexpected(errc::invalid_arg);
            }

            util::usize retried = 0;
            for (auto& pending : pending_) {
                if (!pending.used || pending.failed) {
                    continue;
                }
                if (pending.attempts >= max_attempts) {
                    pending.failed = true;
                    continue;
                }
                auto requested = issue_pending_request(pending);
                if (!requested) {
                    return util::unexpected(requested.error());
                }
                ++retried;
            }
            return Result<util::usize>{std::in_place, retried};
        }

        [[nodiscard]] Result<ArpRetryProgress> service_pending(
            util::usize retry_interval_ticks,
            util::usize max_attempts = static_cast<util::usize>(-1)) noexcept {
            if (netif_ == nullptr) {
                return util::unexpected(errc::bad_state);
            }

            ArpRetryProgress progress{};
            for (auto& pending : pending_) {
                if (!pending.used || pending.failed) {
                    continue;
                }

                const auto elapsed = tick_count_ - pending.last_request_tick;
                if (elapsed < retry_interval_ticks) {
                    continue;
                }
                if (pending.attempts >= max_attempts) {
                    pending.failed = true;
                    ++progress.timed_out;
                    continue;
                }

                auto requested = issue_pending_request(pending);
                if (!requested) {
                    return util::unexpected(requested.error());
                }
                ++progress.retried;
            }
            return Result<ArpRetryProgress>{std::in_place, progress};
        }

        [[nodiscard]] Result<MacAddress> lookup_or_request(IpAddress ip) noexcept {
            if (netif_ == nullptr) {
                return util::unexpected(errc::bad_state);
            }
            if (!ip.is_ipv4() || ip.is_any()) {
                return util::unexpected(errc::invalid_arg);
            }

            const auto cached = table_.lookup(ip);
            if (cached) {
                clear_resolution(ip);
                return cached;
            }
            if (cached.error() != errc::noent) {
                return util::unexpected(cached.error());
            }

            if (find_failed(ip) != invalid_index()) {
                return util::unexpected(errc::timeout);
            }
            if (find_pending(ip) != invalid_index()) {
                return util::unexpected(errc::again);
            }

            const auto slot = reserve_pending_slot();
            if (!slot) {
                return util::unexpected(slot.error());
            }

            pending_[slot.value()] = PendingEntry{
                .used = true,
                .ip = ip,
            };
            auto requested = issue_pending_request(pending_[slot.value()]);
            if (!requested) {
                pending_[slot.value()] = {};
                return util::unexpected(requested.error());
            }
            return util::unexpected(errc::again);
        }

        [[nodiscard]] Result<void> consume(OwnedPacket packet) noexcept {
            if (netif_ == nullptr) {
                return util::unexpected(errc::bad_state);
            }

            const auto parsed = parse_arp_ipv4_ethernet(packet.view());
            if (!parsed) {
                return util::unexpected(parsed.error());
            }

            auto remembered = table_.remember(parsed.value().sender_ip, parsed.value().sender_mac);
            if (!remembered) {
                return util::unexpected(remembered.error());
            }
            clear_resolution(parsed.value().sender_ip);

            if (parsed.value().operation != ArpOperation::request) {
                return {};
            }
            if (!is_same_ipv4_address(parsed.value().target_ip, netif_->address())) {
                return {};
            }

            PacketBuffer<TxCapacity> reply{};
            auto encoded = write_arp_ipv4_reply_frame(
                reply,
                parsed.value().sender_mac,
                netif_->mac(),
                netif_->address(),
                parsed.value().sender_mac,
                parsed.value().sender_ip);
            if (!encoded) {
                return util::unexpected(encoded.error());
            }

            auto sent = netif_->transmit(reply.view());
            if (!sent) {
                return util::unexpected(sent.error());
            }
            ++reply_count_;
            return {};
        }

    private:
        struct PendingEntry {
            bool used{false};
            bool failed{false};
            IpAddress ip{};
            util::usize attempts{0};
            util::usize last_request_tick{0};
        };

        static constexpr util::usize invalid_index() noexcept {
            return static_cast<util::usize>(-1);
        }

        [[nodiscard]] util::usize find_entry(IpAddress ip) const noexcept {
            for (util::usize i = 0; i < pending_.size(); ++i) {
                if (!pending_[i].used) {
                    continue;
                }
                if (is_same_ipv4_address(pending_[i].ip, ip)) {
                    return i;
                }
            }
            return invalid_index();
        }

        [[nodiscard]] util::usize find_pending(IpAddress ip) const noexcept {
            const auto index = find_entry(ip);
            if (index == invalid_index() || pending_[index].failed) {
                return invalid_index();
            }
            return index;
        }

        [[nodiscard]] util::usize find_failed(IpAddress ip) const noexcept {
            const auto index = find_entry(ip);
            if (index == invalid_index() || !pending_[index].failed) {
                return invalid_index();
            }
            return index;
        }

        [[nodiscard]] Result<util::usize> reserve_pending_slot() const noexcept {
            for (util::usize i = 0; i < pending_.size(); ++i) {
                if (!pending_[i].used) {
                    return Result<util::usize>{std::in_place, i};
                }
            }
            return util::unexpected(errc::buffer_overflow);
        }

        [[nodiscard]] Result<void> issue_pending_request(PendingEntry& pending) noexcept {
            auto requested = send_arp_ipv4_request<TxCapacity>(*netif_, pending.ip);
            if (!requested) {
                return util::unexpected(requested.error());
            }
            ++pending.attempts;
            pending.last_request_tick = tick_count_;
            ++request_count_;
            return {};
        }

        void clear_resolution(IpAddress ip) noexcept {
            const auto index = find_entry(ip);
            if (index == invalid_index()) {
                return;
            }
            pending_[index] = {};
        }

        NetIf* netif_{nullptr};
        ArpTable<TableCapacity> table_{};
        std::array<PendingEntry, TableCapacity> pending_{};
        util::usize tick_count_{0};
        util::usize request_count_{0};
        util::usize reply_count_{0};
    };
}
