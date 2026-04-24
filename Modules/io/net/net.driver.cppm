module;

#include <concepts>

export module net.driver;

export import net.netif;
import net.packet;
import util.core;
import util.error;
import util.expected;

export namespace net {
    struct NetDriverInfo {
        util::u16 mtu{1500};
        MacAddress mac{};
        NetIfCapabilityMask capabilities{
            capability_mask(NetIfCapability::rx) | capability_mask(NetIfCapability::tx)
        };
    };

    enum class NetDriverState : util::u8 {
        detached,
        attached,
    };

    template <typename T>
    concept NetDriverProvider = requires(T& t, OwnedPacketSinkRef sink, PacketView packet) {
        { t.info() } noexcept -> std::same_as<NetDriverInfo>;
        { t.set_input_sink(sink) } noexcept -> std::same_as<Result<void>>;
        { t.poll() } noexcept -> std::same_as<Result<void>>;
        { t.transmit(packet) } noexcept -> std::same_as<Result<void>>;
    };

    struct NetDriverProviderOps {
        NetDriverInfo (*info)(void*) noexcept;
        Result<void> (*set_input_sink)(void*, OwnedPacketSinkRef) noexcept;
        Result<void> (*poll)(void*) noexcept;
        Result<void> (*transmit)(void*, PacketView) noexcept;
    };

    struct NetDriverProviderRef {
        void* self{nullptr};
        const NetDriverProviderOps* ops{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return self != nullptr
                && ops != nullptr
                && ops->info != nullptr
                && ops->set_input_sink != nullptr
                && ops->poll != nullptr
                && ops->transmit != nullptr;
        }

        [[nodiscard]] NetDriverInfo info() const noexcept {
            if (!valid()) {
                return {};
            }
            return ops->info(self);
        }

        [[nodiscard]] Result<void> set_input_sink(OwnedPacketSinkRef sink) const noexcept {
            if (!valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            return ops->set_input_sink(self, sink);
        }

        [[nodiscard]] Result<void> poll() const noexcept {
            if (!valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            return ops->poll(self);
        }

        [[nodiscard]] Result<void> transmit(PacketView packet) const noexcept {
            if (!valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            return ops->transmit(self, packet);
        }
    };

    template <NetDriverProvider T>
    inline const NetDriverProviderOps* net_driver_provider_ops() noexcept {
        static const NetDriverProviderOps ops{
            .info = [](void* self) noexcept {
                return static_cast<T*>(self)->info();
            },
            .set_input_sink = [](void* self, OwnedPacketSinkRef sink) noexcept {
                return static_cast<T*>(self)->set_input_sink(sink);
            },
            .poll = [](void* self) noexcept {
                return static_cast<T*>(self)->poll();
            },
            .transmit = [](void* self, PacketView packet) noexcept {
                return static_cast<T*>(self)->transmit(packet);
            }
        };
        return &ops;
    }

    template <NetDriverProvider T>
    inline NetDriverProviderRef make_net_driver_provider_ref(T& provider) noexcept {
        return NetDriverProviderRef{&provider, net_driver_provider_ops<T>()};
    }

    class NetDriver {
    public:
        NetDriver() noexcept
            : tx_path_{this},
              rx_path_{this} {}

        [[nodiscard]] constexpr bool attached() const noexcept {
            return provider_.valid() && netif_ != nullptr;
        }

        [[nodiscard]] constexpr NetDriverState state() const noexcept {
            return attached() ? NetDriverState::attached : NetDriverState::detached;
        }

        [[nodiscard]] constexpr NetDriverProviderRef provider() const noexcept {
            return provider_;
        }

        [[nodiscard]] constexpr NetIf* netif() const noexcept {
            return netif_;
        }

        [[nodiscard]] PacketSinkRef tx_sink() noexcept {
            return make_packet_sink_ref(tx_path_);
        }

        [[nodiscard]] OwnedPacketSinkRef rx_sink() noexcept {
            return make_owned_packet_sink_ref(rx_path_);
        }

        [[nodiscard]] Result<void> attach(NetDriverProviderRef provider, NetIf& netif) noexcept {
            if (!provider.valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            if (attached()) {
                return util::unexpected(errc::busy);
            }
            if (netif.output_sink().valid()) {
                return util::unexpected(errc::busy);
            }

            const auto previous = netif.config();
            const auto previous_output = netif.output_sink();
            const auto info = provider.info();
            if (info.mtu == 0) {
                return util::unexpected(errc::invalid_arg);
            }

            auto configured = netif.configure(NetIfConfig{
                .mtu = info.mtu,
                .mac = info.mac,
                .address = previous.address,
                .capabilities = info.capabilities
            });
            if (!configured) {
                return util::unexpected(configured.error());
            }

            netif.set_output_sink(tx_sink());
            auto installed = provider.set_input_sink(rx_sink());
            if (!installed) {
                netif.set_output_sink(previous_output);
                (void)netif.configure(previous);
                return util::unexpected(installed.error());
            }

            provider_ = provider;
            netif_ = &netif;
            return {};
        }

        [[nodiscard]] Result<void> poll() const noexcept {
            if (!attached()) {
                return util::unexpected(errc::bad_state);
            }
            return provider_.poll();
        }

        [[nodiscard]] Result<void> detach() noexcept {
            if (!attached()) {
                return {};
            }

            auto cleared = provider_.set_input_sink({});
            if (!cleared) {
                return util::unexpected(cleared.error());
            }

            if (netif_ != nullptr) {
                netif_->set_output_sink({});
            }

            provider_ = {};
            netif_ = nullptr;
            return {};
        }

    private:
        class TxPath {
        public:
            explicit TxPath(NetDriver* owner) noexcept
                : owner_(owner) {}

            [[nodiscard]] Result<void> consume(PacketView packet) noexcept {
                if (owner_ == nullptr) {
                    return util::unexpected(errc::bad_state);
                }
                return owner_->transmit_to_provider(packet);
            }

        private:
            NetDriver* owner_{nullptr};
        };

        class RxPath {
        public:
            explicit RxPath(NetDriver* owner) noexcept
                : owner_(owner) {}

            [[nodiscard]] Result<void> consume(OwnedPacket packet) noexcept {
                if (owner_ == nullptr) {
                    return util::unexpected(errc::bad_state);
                }
                return owner_->deliver_to_netif(static_cast<OwnedPacket&&>(packet));
            }

        private:
            NetDriver* owner_{nullptr};
        };

        [[nodiscard]] Result<void> transmit_to_provider(PacketView packet) const noexcept {
            if (!attached()) {
                return util::unexpected(errc::bad_state);
            }
            return provider_.transmit(packet);
        }

        [[nodiscard]] Result<void> deliver_to_netif(OwnedPacket packet) const noexcept {
            if (netif_ == nullptr) {
                return util::unexpected(errc::bad_state);
            }
            return netif_->deliver_input(static_cast<OwnedPacket&&>(packet));
        }

        NetDriverProviderRef provider_{};
        NetIf* netif_{nullptr};
        TxPath tx_path_;
        RxPath rx_path_;
    };
}
