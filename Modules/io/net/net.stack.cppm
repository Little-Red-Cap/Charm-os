module;

#include <array>

export module net.stack;

import net.socket;
import net.driver;
import net.ether;
import net.packet;
import util.core;
import util.error;
import util.expected;

export namespace net {
    class Stack {
    public:
        static constexpr util::usize max_link_slots = 4;

        static constexpr util::usize max_netifs() noexcept {
            return max_link_slots;
        }

        static constexpr util::usize max_drivers() noexcept {
            return max_link_slots;
        }

        constexpr Stack() noexcept = default;

        constexpr explicit Stack(SocketProviderRef provider) noexcept
            : provider_(provider) {}

        template <SocketProvider T>
        constexpr explicit Stack(T& provider) noexcept
            : provider_(make_socket_provider_ref(provider)) {}

        [[nodiscard]] constexpr bool valid() const noexcept {
            return provider_.valid();
        }

        [[nodiscard]] constexpr SocketProviderRef provider() const noexcept {
            return provider_;
        }

        constexpr void bind(SocketProviderRef provider) noexcept {
            provider_ = provider;
        }

        template <SocketProvider T>
        constexpr void bind(T& provider) noexcept {
            provider_ = make_socket_provider_ref(provider);
        }

        [[nodiscard]] util::usize netif_count() const noexcept {
            util::usize count = 0;
            for (const auto* netif : netifs_) {
                if (netif != nullptr) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] util::usize driver_count() const noexcept {
            util::usize count = 0;
            for (const auto* driver : drivers_) {
                if (driver != nullptr) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] bool contains(const NetIf& netif) const noexcept {
            return find_netif_slot(netif) != invalid_index();
        }

        [[nodiscard]] bool contains(const NetDriver& driver) const noexcept {
            return find_driver_slot(driver) != invalid_index();
        }

        void set_arp_sink(OwnedPacketSinkRef sink) noexcept {
            arp_sink_ = sink;
        }

        void set_ipv4_sink(OwnedPacketSinkRef sink) noexcept {
            ipv4_sink_ = sink;
        }

        [[nodiscard]] Result<void> register_netif(NetIf& netif) noexcept {
            if (contains(netif)) {
                return util::unexpected(errc::exist);
            }
            if (netif.bound() && netif.stack() != this) {
                return util::unexpected(errc::busy);
            }
            if (netif.input_sink().valid() && !is_managed_input_sink(netif.input_sink())) {
                return util::unexpected(errc::busy);
            }

            const auto slot = reserve_netif_slot();
            if (!slot) {
                return util::unexpected(slot.error());
            }

            if (!netif.bound()) {
                auto bound = netif.bind(*this);
                if (!bound) {
                    return util::unexpected(bound.error());
                }
            }

            ingress_paths_[slot.value()] = IngressPath{this, &netif};
            netifs_[slot.value()] = &netif;
            netif.set_input_sink(make_owned_packet_sink_ref(ingress_paths_[slot.value()]));
            return {};
        }

        [[nodiscard]] Result<void> register_driver(NetDriver& driver) noexcept {
            if (contains(driver)) {
                return util::unexpected(errc::exist);
            }
            if (!driver.attached() || driver.netif() == nullptr) {
                return util::unexpected(errc::bad_state);
            }

            auto registered = register_netif(*driver.netif());
            if (!registered && registered.error() != errc::exist) {
                return util::unexpected(registered.error());
            }

            if (driver.netif()->stack() != this) {
                return util::unexpected(errc::busy);
            }

            const auto slot = reserve_driver_slot();
            if (!slot) {
                return util::unexpected(slot.error());
            }

            drivers_[slot.value()] = &driver;
            return {};
        }

        [[nodiscard]] Result<void> poll_links() const noexcept {
            for (const auto* driver : drivers_) {
                if (driver == nullptr) {
                    continue;
                }
                auto polled = driver->poll();
                if (!polled) {
                    return util::unexpected(polled.error());
                }
            }
            return {};
        }

    private:
        struct IngressPath {
            Stack* owner{nullptr};
            NetIf* netif{nullptr};

            [[nodiscard]] Result<void> consume(OwnedPacket packet) noexcept {
                if (owner == nullptr || netif == nullptr) {
                    return util::unexpected(errc::bad_state);
                }
                return owner->dispatch_input(*netif, static_cast<OwnedPacket&&>(packet));
            }
        };

        static constexpr util::usize invalid_index() noexcept {
            return static_cast<util::usize>(-1);
        }

        [[nodiscard]] util::usize find_netif_slot(const NetIf& netif) const noexcept {
            for (util::usize i = 0; i < netifs_.size(); ++i) {
                if (netifs_[i] == &netif) {
                    return i;
                }
            }
            return invalid_index();
        }

        [[nodiscard]] util::usize find_driver_slot(const NetDriver& driver) const noexcept {
            for (util::usize i = 0; i < drivers_.size(); ++i) {
                if (drivers_[i] == &driver) {
                    return i;
                }
            }
            return invalid_index();
        }

        [[nodiscard]] Result<util::usize> reserve_netif_slot() noexcept {
            for (util::usize i = 0; i < netifs_.size(); ++i) {
                if (netifs_[i] == nullptr) {
                    return Result<util::usize>{std::in_place, i};
                }
            }
            return util::unexpected(errc::buffer_overflow);
        }

        [[nodiscard]] Result<util::usize> reserve_driver_slot() noexcept {
            for (util::usize i = 0; i < drivers_.size(); ++i) {
                if (drivers_[i] == nullptr) {
                    return Result<util::usize>{std::in_place, i};
                }
            }
            return util::unexpected(errc::buffer_overflow);
        }

        [[nodiscard]] bool is_managed_input_sink(OwnedPacketSinkRef sink) const noexcept {
            if (!sink.valid()) {
                return false;
            }
            for (util::usize i = 0; i < ingress_paths_.size(); ++i) {
                if (sink.self == &ingress_paths_[i]
                    && sink.ops == owned_packet_sink_ops<IngressPath>()) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] Result<void> dispatch_input(NetIf& netif, OwnedPacket packet) const noexcept {
            (void)netif;

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
                    if (!arp_sink_.valid()) {
                        return util::unexpected(errc::not_supported);
                    }
                    return arp_sink_.consume(static_cast<OwnedPacket&&>(packet));
                case EtherType::ipv4:
                    if (!ipv4_sink_.valid()) {
                        return util::unexpected(errc::not_supported);
                    }
                    return ipv4_sink_.consume(static_cast<OwnedPacket&&>(packet));
                default:
                    return util::unexpected(errc::not_supported);
            }
        }

        SocketProviderRef provider_{};
        std::array<NetIf*, max_link_slots> netifs_{};
        std::array<NetDriver*, max_link_slots> drivers_{};
        std::array<IngressPath, max_link_slots> ingress_paths_{};
        OwnedPacketSinkRef arp_sink_{};
        OwnedPacketSinkRef ipv4_sink_{};
    };
}
