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
    enum class Ipv4ForwardingPort : util::u8 {
        a = 0u,
        b = 1u,
    };

    struct Ipv4ForwardingRoute {
        IpAddress network{IpAddress::ipv4_any()};
        util::u8 prefix_length{0};
        Ipv4ForwardingPort egress_port{Ipv4ForwardingPort::a};
        bool has_next_hop{false};
        IpAddress next_hop{};
    };

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
        util::usize destination_unreachable{0};
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
            bool connected_prefix_configured{false};
            util::u8 connected_prefix_length{0};

            [[nodiscard]] bool ready() const noexcept {
                return driver.attached() && netif.state() == NetIfState::up;
            }
        };

        struct ForwardDecision {
            bool valid{false};
            util::usize egress{0};
            util::u8 prefix_length{0};
            IpAddress next_hop{};
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

        [[nodiscard]] util::usize destination_unreachable_count() const noexcept {
            return destination_unreachable_count_;
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

        [[nodiscard]] util::usize route_count() const noexcept {
            return route_count_;
        }

        [[nodiscard]] bool port_a_has_connected_prefix() const noexcept {
            return ports_[0].connected_prefix_configured;
        }

        [[nodiscard]] bool port_b_has_connected_prefix() const noexcept {
            return ports_[1].connected_prefix_configured;
        }

        [[nodiscard]] util::u8 port_a_prefix_length() const noexcept {
            return ports_[0].connected_prefix_length;
        }

        [[nodiscard]] util::u8 port_b_prefix_length() const noexcept {
            return ports_[1].connected_prefix_length;
        }

        [[nodiscard]] Result<void> set_port_a_prefix_length(util::u8 prefix_length) noexcept {
            return set_port_prefix<0u>(prefix_length);
        }

        [[nodiscard]] Result<void> set_port_b_prefix_length(util::u8 prefix_length) noexcept {
            return set_port_prefix<1u>(prefix_length);
        }

        void clear_routes() noexcept {
            for (util::usize index = 0; index < route_count_; ++index) {
                routes_[index] = {};
            }
            route_count_ = 0;
        }

        [[nodiscard]] Result<void> set_route(Ipv4ForwardingRoute route) noexcept {
            auto validated = validate_route(route);
            if (!validated) {
                return util::unexpected(validated.error());
            }

            route.network = canonical_ipv4_network(route.network, route.prefix_length);
            const auto existing = find_route_index(route.network, route.prefix_length);
            if (existing < route_count_) {
                routes_[existing] = route;
                return {};
            }

            if (route_count_ >= routes_.size()) {
                return util::unexpected(errc::buffer_overflow);
            }

            routes_[route_count_] = route;
            ++route_count_;
            return {};
        }

        [[nodiscard]] Result<void> add_route(Ipv4ForwardingRoute route) noexcept {
            auto validated = validate_route(route);
            if (!validated) {
                return util::unexpected(validated.error());
            }
            if (route_count_ >= routes_.size()) {
                return util::unexpected(errc::buffer_overflow);
            }

            route.network = canonical_ipv4_network(route.network, route.prefix_length);
            routes_[route_count_] = route;
            ++route_count_;
            return {};
        }

        [[nodiscard]] Result<void> add_direct_route(IpAddress network,
                                                    util::u8 prefix_length,
                                                    Ipv4ForwardingPort egress_port) noexcept {
            return add_route(Ipv4ForwardingRoute{
                .network = network,
                .prefix_length = prefix_length,
                .egress_port = egress_port,
                .has_next_hop = false,
                .next_hop = {},
            });
        }

        [[nodiscard]] Result<void> add_gateway_route(IpAddress network,
                                                     util::u8 prefix_length,
                                                     Ipv4ForwardingPort egress_port,
                                                     IpAddress next_hop) noexcept {
            return add_route(Ipv4ForwardingRoute{
                .network = network,
                .prefix_length = prefix_length,
                .egress_port = egress_port,
                .has_next_hop = true,
                .next_hop = next_hop,
            });
        }

        [[nodiscard]] Result<void> set_direct_route(IpAddress network,
                                                    util::u8 prefix_length,
                                                    Ipv4ForwardingPort egress_port) noexcept {
            return set_route(Ipv4ForwardingRoute{
                .network = network,
                .prefix_length = prefix_length,
                .egress_port = egress_port,
                .has_next_hop = false,
                .next_hop = {},
            });
        }

        [[nodiscard]] Result<void> set_gateway_route(IpAddress network,
                                                     util::u8 prefix_length,
                                                     Ipv4ForwardingPort egress_port,
                                                     IpAddress next_hop) noexcept {
            return set_route(Ipv4ForwardingRoute{
                .network = network,
                .prefix_length = prefix_length,
                .egress_port = egress_port,
                .has_next_hop = true,
                .next_hop = next_hop,
            });
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
            const auto destination_unreachable_before = destination_unreachable_count_;
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
                .destination_unreachable = destination_unreachable_count_ - destination_unreachable_before,
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

            auto valid_gateway_routes = validate_gateway_routes_for_port<Index>();
            if (!valid_gateway_routes) {
                return util::unexpected(valid_gateway_routes.error());
            }

            return {};
        }

        template <util::usize Index>
        [[nodiscard]] Result<void> set_port_prefix(util::u8 prefix_length) noexcept {
            if (prefix_length > 32u) {
                return util::unexpected(errc::invalid_arg);
            }

            auto& port = ports_[Index];
            const auto previous_configured = port.connected_prefix_configured;
            const auto previous_prefix_length = port.connected_prefix_length;

            port.connected_prefix_configured = true;
            port.connected_prefix_length = prefix_length;

            auto valid_gateway_routes = validate_gateway_routes_for_port<Index>();
            if (!valid_gateway_routes) {
                port.connected_prefix_configured = previous_configured;
                port.connected_prefix_length = previous_prefix_length;
                return util::unexpected(valid_gateway_routes.error());
            }

            return {};
        }

        [[nodiscard]] static constexpr util::usize egress_index(util::usize ingress) noexcept {
            return ingress == 0u ? 1u : 0u;
        }

        [[nodiscard]] static constexpr util::usize port_index(Ipv4ForwardingPort port) noexcept {
            return static_cast<util::usize>(port);
        }

        [[nodiscard]] static constexpr util::u32 ipv4_bits(IpAddress address) noexcept {
            return (static_cast<util::u32>(address.bytes[0]) << 24)
                | (static_cast<util::u32>(address.bytes[1]) << 16)
                | (static_cast<util::u32>(address.bytes[2]) << 8)
                | static_cast<util::u32>(address.bytes[3]);
        }

        [[nodiscard]] static constexpr util::u32 ipv4_prefix_mask(util::u8 prefix_length) noexcept {
            if (prefix_length == 0u) {
                return 0u;
            }
            if (prefix_length >= 32u) {
                return 0xFFFF'FFFFu;
            }
            return 0xFFFF'FFFFu << (32u - prefix_length);
        }

        [[nodiscard]] static constexpr IpAddress canonical_ipv4_network(IpAddress network,
                                                                        util::u8 prefix_length) noexcept {
            if (!network.is_ipv4()) {
                return {};
            }

            const auto masked = ipv4_bits(network) & ipv4_prefix_mask(prefix_length);
            return IpAddress::ipv4(
                static_cast<util::u8>((masked >> 24) & 0xFFu),
                static_cast<util::u8>((masked >> 16) & 0xFFu),
                static_cast<util::u8>((masked >> 8) & 0xFFu),
                static_cast<util::u8>(masked & 0xFFu));
        }

        [[nodiscard]] static constexpr bool matches_ipv4_prefix(IpAddress address,
                                                                IpAddress network,
                                                                util::u8 prefix_length) noexcept {
            if (!address.is_ipv4() || !network.is_ipv4() || prefix_length > 32u) {
                return false;
            }

            const auto mask = ipv4_prefix_mask(prefix_length);
            return (ipv4_bits(address) & mask) == (ipv4_bits(network) & mask);
        }

        [[nodiscard]] Result<void> validate_route(const Ipv4ForwardingRoute& route) const noexcept {
            if (!route.network.is_ipv4() || route.prefix_length > 32u) {
                return util::unexpected(errc::invalid_arg);
            }

            const auto egress = port_index(route.egress_port);
            if (egress >= ports_.size()) {
                return util::unexpected(errc::invalid_arg);
            }

            if (route.has_next_hop
                && (!route.next_hop.is_ipv4() || route.next_hop.is_any())) {
                return util::unexpected(errc::invalid_arg);
            }

            if (route.has_next_hop && !gateway_matches_port(egress, route.next_hop)) {
                return util::unexpected(errc::invalid_arg);
            }

            return {};
        }

        [[nodiscard]] util::usize find_route_index(IpAddress network,
                                                   util::u8 prefix_length) const noexcept {
            for (util::usize index = 0; index < route_count_; ++index) {
                const auto& route = routes_[index];
                if (route.prefix_length != prefix_length) {
                    continue;
                }
                if (is_same_ipv4_address(route.network, network)) {
                    return index;
                }
            }
            return route_count_;
        }

        [[nodiscard]] bool is_local_address(IpAddress address) const noexcept {
            return is_same_ipv4_address(address, ports_[0].netif.address())
                || is_same_ipv4_address(address, ports_[1].netif.address());
        }

        [[nodiscard]] bool routing_configured() const noexcept {
            return route_count_ != 0u
                || ports_[0].connected_prefix_configured
                || ports_[1].connected_prefix_configured;
        }

        [[nodiscard]] bool gateway_matches_port(util::usize egress,
                                                IpAddress next_hop) const noexcept {
            if (egress >= ports_.size()) {
                return false;
            }

            const auto& port = ports_[egress];
            if (!port.connected_prefix_configured || !port.netif.address().is_ipv4()) {
                return true;
            }

            return matches_ipv4_prefix(
                next_hop,
                canonical_ipv4_network(port.netif.address(), port.connected_prefix_length),
                port.connected_prefix_length);
        }

        template <util::usize Index>
        [[nodiscard]] Result<void> validate_gateway_routes_for_port() const noexcept {
            for (util::usize route_index = 0; route_index < route_count_; ++route_index) {
                const auto& route = routes_[route_index];
                if (port_index(route.egress_port) != Index || !route.has_next_hop) {
                    continue;
                }
                if (!gateway_matches_port(Index, route.next_hop)) {
                    return util::unexpected(errc::invalid_arg);
                }
            }
            return {};
        }

        [[nodiscard]] const Ipv4ForwardingRoute* select_route(IpAddress destination) const noexcept {
            if (!destination.is_ipv4()) {
                return nullptr;
            }

            const Ipv4ForwardingRoute* best = nullptr;
            util::u8 best_prefix = 0u;
            for (util::usize index = 0; index < route_count_; ++index) {
                const auto& route = routes_[index];
                if (!matches_ipv4_prefix(destination, route.network, route.prefix_length)) {
                    continue;
                }
                if (best == nullptr || route.prefix_length > best_prefix) {
                    best = &route;
                    best_prefix = route.prefix_length;
                }
            }
            return best;
        }

        [[nodiscard]] ForwardDecision select_connected_route(IpAddress destination) const noexcept {
            if (!destination.is_ipv4()) {
                return {};
            }

            ForwardDecision best{};
            for (util::usize index = 0; index < ports_.size(); ++index) {
                const auto& port = ports_[index];
                if (!port.connected_prefix_configured || !port.netif.address().is_ipv4()) {
                    continue;
                }
                if (!matches_ipv4_prefix(
                        destination,
                        canonical_ipv4_network(port.netif.address(), port.connected_prefix_length),
                        port.connected_prefix_length)) {
                    continue;
                }
                if (!best.valid || port.connected_prefix_length > best.prefix_length) {
                    best.valid = true;
                    best.egress = index;
                    best.prefix_length = port.connected_prefix_length;
                    best.next_hop = {};
                }
            }
            return best;
        }

        [[nodiscard]] ForwardDecision select_forwarding_decision(IpAddress destination) const noexcept {
            ForwardDecision best{};
            if (const auto* route = select_route(destination); route != nullptr) {
                best.valid = true;
                best.egress = port_index(route->egress_port);
                best.prefix_length = route->prefix_length;
                best.next_hop = route->has_next_hop ? route->next_hop : IpAddress{};
            }

            const auto connected = select_connected_route(destination);
            if (connected.valid
                && (!best.valid || connected.prefix_length > best.prefix_length)) {
                best = connected;
            }

            return best;
        }

        [[nodiscard]] bool can_proxy_arp_to(util::usize ingress,
                                            IpAddress destination) const noexcept {
            if (!destination.is_ipv4()
                || destination.is_any()
                || destination.is_ipv4_limited_broadcast()
                || is_local_address(destination)) {
                return false;
            }

            if (!routing_configured()) {
                return true;
            }

            const auto decision = select_forwarding_decision(destination);
            return decision.valid && decision.egress != ingress;
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
            } else if (can_proxy_arp_to(ingress, arp.value().target_ip)) {
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

            auto quoted_size = datagram.value().header_length;
            const auto quoted_payload_size = datagram.value().payload.size() < 8u
                ? datagram.value().payload.size()
                : 8u;
            quoted_size += quoted_payload_size;

            if (datagram.value().ttl <= 1u) {
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

            IpAddress next_hop{};
            util::usize egress = egress_index(ingress);
            const auto decision = select_forwarding_decision(datagram.value().destination);
            if (decision.valid) {
                egress = decision.egress;
                next_hop = decision.next_hop;
            } else if (routing_configured()) {
                auto emitted = emit_destination_unreachable(
                    ingress_port,
                    datagram.value().source,
                    0u,
                    packet.view().subspan(0, quoted_size).payload);
                if (!emitted) {
                    return util::unexpected(emitted.error());
                }

                ++destination_unreachable_count_;
                return {};
            }

            auto& egress_port = ports_[egress];

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
                forwarded.view().payload,
                next_hop);
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

        [[nodiscard]] Result<void> emit_destination_unreachable(Port& port,
                                                                IpAddress peer,
                                                                util::u8 code,
                                                                ByteView quoted_packet) noexcept {
            PacketBuffer<PacketCapacity> icmp_packet{};
            auto encoded_icmp = write_icmp_destination_unreachable_packet(
                icmp_packet,
                code,
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
        std::array<Ipv4ForwardingRoute, 8> routes_{};
        Ipv4ForwardingHopConfig config_{};
        util::usize route_count_{0};
        util::usize arp_packet_count_{0};
        util::usize ipv4_packet_count_{0};
        util::usize forwarded_count_{0};
        util::usize ttl_expired_count_{0};
        util::usize destination_unreachable_count_{0};
        util::usize local_drop_count_{0};
        util::usize proxy_arp_reply_count_{0};
    };
}
