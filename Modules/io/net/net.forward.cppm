module;

#include <array>
#include <concepts>

export module net.forward;

import net.arp;
import net.driver;
import net.ether;
import net.icmp;
import net.ipv4;
import util.core;
import util.error;
import util.expected;

namespace net::detail {
    template <typename Provider>
    concept MacConfigurableProvider = requires(Provider& provider, MacAddress mac) {
        provider.set_mac(mac);
    };
}

export namespace net {
    struct Ipv4ForwardingHopConfig {
        util::usize retry_interval_ticks{1};
        util::usize max_attempts{static_cast<util::usize>(-1)};
        util::u8 icmp_ttl{64};
    };

    struct Ipv4ForwardingHopProgress {
        bool polled_links{false};
        util::usize arp_packets{0};
        util::usize ipv4_packets{0};
        util::usize forwarded{0};
        util::usize ttl_expired{0};
        util::usize local_dropped{0};
        util::usize proxy_arp_replied{0};
        util::usize arp_retried{0};
        util::usize arp_timed_out{0};
        util::usize egress_flushed{0};
        util::usize egress_dropped{0};
    };

    template <util::usize TxCapacity,
              util::usize ArpCapacity,
              util::usize ArpTxCapacity,
              util::usize PendingCapacity,
              util::usize PacketCapacity = TxCapacity>
    class Ipv4ForwardingHop {
    private:
        struct Port {
            NetIf netif{};
            NetDriver driver{};
            ArpService<ArpCapacity, ArpTxCapacity> arp{};
            Ipv4EgressQueue<PendingCapacity, PacketCapacity> egress{};
            util::u16 next_identification{0x4000u};

            [[nodiscard]] bool ready() const noexcept {
                return driver.attached() && netif.state() == NetIfState::up;
            }
        };

        struct IngressPath {
            Ipv4ForwardingHop* owner{nullptr};
            util::usize index{0};

            [[nodiscard]] Result<void> consume(OwnedPacket packet) noexcept {
                if (owner == nullptr || index >= 2u) {
                    return util::unexpected(errc::bad_state);
                }
                return owner->consume_port(index, static_cast<OwnedPacket&&>(packet));
            }
        };

    public:
        Ipv4ForwardingHop() noexcept {
            ingress_paths_[0] = IngressPath{this, 0u};
            ingress_paths_[1] = IngressPath{this, 1u};
            ports_[0].arp.bind(ports_[0].netif);
            ports_[1].arp.bind(ports_[1].netif);
        }

        void configure(Ipv4ForwardingHopConfig config) noexcept {
            config_ = config;
        }

        [[nodiscard]] const Ipv4ForwardingHopConfig& config() const noexcept {
            return config_;
        }

        [[nodiscard]] bool ready() const noexcept {
            return ports_[0].ready() && ports_[1].ready();
        }

        [[nodiscard]] NetIf& netif_a() noexcept {
            return ports_[0].netif;
        }

        [[nodiscard]] const NetIf& netif_a() const noexcept {
            return ports_[0].netif;
        }

        [[nodiscard]] NetIf& netif_b() noexcept {
            return ports_[1].netif;
        }

        [[nodiscard]] const NetIf& netif_b() const noexcept {
            return ports_[1].netif;
        }

        [[nodiscard]] ArpService<ArpCapacity, ArpTxCapacity>& arp_a() noexcept {
            return ports_[0].arp;
        }

        [[nodiscard]] const ArpService<ArpCapacity, ArpTxCapacity>& arp_a() const noexcept {
            return ports_[0].arp;
        }

        [[nodiscard]] ArpService<ArpCapacity, ArpTxCapacity>& arp_b() noexcept {
            return ports_[1].arp;
        }

        [[nodiscard]] const ArpService<ArpCapacity, ArpTxCapacity>& arp_b() const noexcept {
            return ports_[1].arp;
        }

        [[nodiscard]] util::usize pending_count() const noexcept {
            return ports_[0].egress.pending_count() + ports_[1].egress.pending_count();
        }

        [[nodiscard]] util::usize forwarded_count() const noexcept {
            return forwarded_count_;
        }

        [[nodiscard]] util::usize ttl_expired_count() const noexcept {
            return ttl_expired_count_;
        }

        [[nodiscard]] util::usize local_drop_count() const noexcept {
            return local_drop_count_;
        }

        [[nodiscard]] util::usize arp_packet_count() const noexcept {
            return arp_packet_count_;
        }

        [[nodiscard]] util::usize ipv4_packet_count() const noexcept {
            return ipv4_packet_count_;
        }

        [[nodiscard]] util::usize proxy_arp_reply_count() const noexcept {
            return proxy_arp_reply_count_;
        }

        template <NetDriverProvider Provider>
        [[nodiscard]] Result<void> init_port_a(Provider& provider,
                                               IpAddress address) noexcept {
            return init_port<0u>(provider, address);
        }

        template <NetDriverProvider Provider>
            requires detail::MacConfigurableProvider<Provider>
        [[nodiscard]] Result<void> init_port_a(Provider& provider,
                                               MacAddress mac,
                                               IpAddress address) noexcept {
            provider.set_mac(mac);
            return init_port<0u>(provider, address);
        }

        template <NetDriverProvider Provider>
        [[nodiscard]] Result<void> init_port_b(Provider& provider,
                                               IpAddress address) noexcept {
            return init_port<1u>(provider, address);
        }

        template <NetDriverProvider Provider>
            requires detail::MacConfigurableProvider<Provider>
        [[nodiscard]] Result<void> init_port_b(Provider& provider,
                                               MacAddress mac,
                                               IpAddress address) noexcept {
            provider.set_mac(mac);
            return init_port<1u>(provider, address);
        }

        [[nodiscard]] Result<Ipv4ForwardingHopProgress> service(
            util::usize elapsed_ticks = 1) noexcept {
            if (!ready()) {
                return util::unexpected(errc::bad_state);
            }

            const auto arp_packets_before = arp_packet_count_;
            const auto ipv4_packets_before = ipv4_packet_count_;
            const auto forwarded_before = forwarded_count_;
            const auto ttl_expired_before = ttl_expired_count_;
            const auto local_drop_before = local_drop_count_;
            const auto proxy_arp_before = proxy_arp_reply_count_;

            auto polled_a = ports_[0].driver.poll();
            if (!polled_a) {
                return util::unexpected(polled_a.error());
            }

            auto polled_b = ports_[1].driver.poll();
            if (!polled_b) {
                return util::unexpected(polled_b.error());
            }

            auto egress_a = ports_[0].egress.template service<TxCapacity>(
                ports_[0].netif,
                ports_[0].arp,
                elapsed_ticks,
                config_.retry_interval_ticks,
                config_.max_attempts);
            if (!egress_a) {
                return util::unexpected(egress_a.error());
            }

            auto egress_b = ports_[1].egress.template service<TxCapacity>(
                ports_[1].netif,
                ports_[1].arp,
                elapsed_ticks,
                config_.retry_interval_ticks,
                config_.max_attempts);
            if (!egress_b) {
                return util::unexpected(egress_b.error());
            }

            return Result<Ipv4ForwardingHopProgress>{std::in_place, Ipv4ForwardingHopProgress{
                .polled_links = true,
                .arp_packets = arp_packet_count_ - arp_packets_before,
                .ipv4_packets = ipv4_packet_count_ - ipv4_packets_before,
                .forwarded = forwarded_count_ - forwarded_before,
                .ttl_expired = ttl_expired_count_ - ttl_expired_before,
                .local_dropped = local_drop_count_ - local_drop_before,
                .proxy_arp_replied = proxy_arp_reply_count_ - proxy_arp_before,
                .arp_retried = egress_a.value().arp_retried + egress_b.value().arp_retried,
                .arp_timed_out = egress_a.value().arp_timed_out + egress_b.value().arp_timed_out,
                .egress_flushed = egress_a.value().flushed + egress_b.value().flushed,
                .egress_dropped = egress_a.value().dropped + egress_b.value().dropped,
            }};
        }

    private:
        template <util::usize Index, NetDriverProvider Provider>
        [[nodiscard]] Result<void> init_port(Provider& provider,
                                             IpAddress address) noexcept {
            auto& port = ports_[Index];

            if (port.driver.attached() || port.netif.input_sink().valid()) {
                return util::unexpected(errc::busy);
            }

            const auto info = provider.info();
            auto configured = port.netif.configure(NetIfConfig{
                .mtu = info.mtu,
                .mac = info.mac,
                .address = address,
                .capabilities = info.capabilities
            });
            if (!configured) {
                return util::unexpected(configured.error());
            }

            auto attached = port.driver.attach(make_net_driver_provider_ref(provider), port.netif);
            if (!attached) {
                return util::unexpected(attached.error());
            }

            port.arp.bind(port.netif);
            port.netif.set_input_sink(make_owned_packet_sink_ref(ingress_paths_[Index]));

            if (!port.netif.bound()) {
                auto bound = port.netif.bind(*this);
                if (!bound && bound.error() != errc::exist) {
                    return util::unexpected(bound.error());
                }
            }

            auto up = port.netif.bring_up();
            if (!up) {
                return util::unexpected(up.error());
            }

            return {};
        }

        [[nodiscard]] static constexpr util::usize egress_index(util::usize ingress) noexcept {
            return ingress == 0u ? 1u : 0u;
        }

        [[nodiscard]] bool is_local_address(IpAddress address) const noexcept {
            return is_same_ipv4_address(address, ports_[0].netif.address())
                || is_same_ipv4_address(address, ports_[1].netif.address());
        }

        [[nodiscard]] Result<void> consume_port(util::usize ingress,
                                                OwnedPacket packet) noexcept {
            const auto frame = parse_ether_frame(packet.view());
            if (!frame) {
                return util::unexpected(frame.error());
            }

            auto trimmed = packet.trim_front(ether_header_size());
            if (!trimmed) {
                return util::unexpected(trimmed.error());
            }

            switch (frame.value().type) {
                case EtherType::arp:
                    ++arp_packet_count_;
                    return consume_arp(ingress, static_cast<OwnedPacket&&>(packet));
                case EtherType::ipv4:
                    ++ipv4_packet_count_;
                    return consume_ipv4(ingress, frame.value().source, static_cast<OwnedPacket&&>(packet));
                default:
                    return {};
            }
        }

        [[nodiscard]] Result<void> consume_arp(util::usize ingress,
                                               OwnedPacket packet) noexcept {
            auto& port = ports_[ingress];

            const auto arp = parse_arp_ipv4_ethernet(packet.view());
            if (!arp) {
                return util::unexpected(arp.error());
            }

            auto remembered = port.arp.remember(arp.value());
            if (!remembered) {
                return util::unexpected(remembered.error());
            }

            if (arp.value().operation != ArpOperation::request
                || arp.value().target_ip.is_any()
                || arp.value().target_ip.is_ipv4_limited_broadcast()) {
                return {};
            }

            bool should_reply = false;
            IpAddress sender_ip{};
            if (is_same_ipv4_address(arp.value().target_ip, port.netif.address())) {
                should_reply = true;
                sender_ip = port.netif.address();
            } else if (!is_local_address(arp.value().target_ip)) {
                should_reply = true;
                sender_ip = arp.value().target_ip;
            }

            if (!should_reply) {
                return {};
            }

            PacketBuffer<TxCapacity> reply{};
            auto encoded = write_arp_ipv4_reply_frame(
                reply,
                arp.value().sender_mac,
                port.netif.mac(),
                sender_ip,
                arp.value().sender_mac,
                arp.value().sender_ip);
            if (!encoded) {
                return util::unexpected(encoded.error());
            }

            auto sent = port.netif.transmit(reply.view());
            if (!sent) {
                return util::unexpected(sent.error());
            }

            if (!is_same_ipv4_address(sender_ip, port.netif.address())) {
                ++proxy_arp_reply_count_;
            }
            return {};
        }

        [[nodiscard]] Result<void> consume_ipv4(util::usize ingress,
                                                MacAddress source_mac,
                                                OwnedPacket packet) noexcept {
            auto& ingress_port = ports_[ingress];
            auto& egress_port = ports_[egress_index(ingress)];

            const auto datagram = parse_ipv4_packet(packet.view());
            if (!datagram) {
                return util::unexpected(datagram.error());
            }

            if (!datagram.value().source.is_any()) {
                auto remembered = ingress_port.arp.remember(datagram.value().source, source_mac);
                if (!remembered) {
                    return util::unexpected(remembered.error());
                }
            }

            if (is_local_address(datagram.value().destination)
                || datagram.value().destination.is_ipv4_limited_broadcast()) {
                ++local_drop_count_;
                return {};
            }

            if (datagram.value().ttl <= 1u) {
                auto quoted_size = datagram.value().header_length;
                const auto quoted_payload_size = datagram.value().payload.size() < 8u
                    ? datagram.value().payload.size()
                    : 8u;
                quoted_size += quoted_payload_size;

                auto emitted = emit_time_exceeded(
                    ingress_port,
                    datagram.value().source,
                    packet.view().subspan(0, quoted_size).payload);
                if (!emitted) {
                    return util::unexpected(emitted.error());
                }

                ++ttl_expired_count_;
                return {};
            }

            PacketBuffer<PacketCapacity> forwarded{};
            auto reset = forwarded.reset();
            if (!reset) {
                return util::unexpected(reset.error());
            }

            auto appended = forwarded.append(packet.view().subspan(0, datagram.value().total_length).payload);
            if (!appended) {
                return util::unexpected(appended.error());
            }

            auto rewritten = rewrite_ipv4_ttl(
                forwarded.mut_view(),
                static_cast<util::u8>(datagram.value().ttl - 1u));
            if (!rewritten) {
                return util::unexpected(rewritten.error());
            }

            auto sent = egress_port.egress.template send<TxCapacity>(
                egress_port.netif,
                egress_port.arp,
                forwarded.view().payload);
            if (!sent) {
                return util::unexpected(sent.error());
            }

            ++forwarded_count_;
            return {};
        }

        [[nodiscard]] Result<void> emit_time_exceeded(Port& port,
                                                      IpAddress peer,
                                                      ByteView quoted_packet) noexcept {
            PacketBuffer<PacketCapacity> icmp_packet{};
            auto encoded_icmp = write_icmp_time_exceeded_packet(
                icmp_packet,
                0u,
                quoted_packet);
            if (!encoded_icmp) {
                return util::unexpected(encoded_icmp.error());
            }

            PacketBuffer<PacketCapacity> ipv4_packet{};
            auto encoded_ipv4 = write_ipv4_packet(
                ipv4_packet,
                Ipv4PacketSpec{
                    .identification = port.next_identification++,
                    .flags_fragment = ipv4_do_not_fragment_flag(),
                    .ttl = config_.icmp_ttl,
                    .protocol = Ipv4Protocol::icmp,
                    .source = port.netif.address(),
                    .destination = peer,
                },
                icmp_packet.view().payload);
            if (!encoded_ipv4) {
                return util::unexpected(encoded_ipv4.error());
            }

            auto sent = port.egress.template send<TxCapacity>(
                port.netif,
                port.arp,
                ipv4_packet.view().payload);
            if (!sent) {
                return util::unexpected(sent.error());
            }
            return {};
        }

        std::array<Port, 2> ports_{};
        std::array<IngressPath, 2> ingress_paths_{};
        Ipv4ForwardingHopConfig config_{};
        util::usize arp_packet_count_{0};
        util::usize ipv4_packet_count_{0};
        util::usize forwarded_count_{0};
        util::usize ttl_expired_count_{0};
        util::usize local_drop_count_{0};
        util::usize proxy_arp_reply_count_{0};
    };
}
