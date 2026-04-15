module;

#include <array>
#include <concepts>

export module net.netif;

export import net.common;
import net.packet;
import util.core;
import util.error;
import util.expected;

export namespace net {
    struct StackRef {
        const void* self{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return self != nullptr;
        }
    };

    template <typename T>
    [[nodiscard]] constexpr bool operator==(StackRef lhs, const T* rhs) noexcept {
        return lhs.self == rhs;
    }

    template <typename T>
    [[nodiscard]] constexpr bool operator==(const T* lhs, StackRef rhs) noexcept {
        return rhs == lhs;
    }

    template <typename T>
    [[nodiscard]] constexpr bool operator!=(StackRef lhs, const T* rhs) noexcept {
        return !(lhs == rhs);
    }

    template <typename T>
    [[nodiscard]] constexpr bool operator!=(const T* lhs, StackRef rhs) noexcept {
        return !(rhs == lhs);
    }

    [[nodiscard]] constexpr bool operator==(StackRef lhs, decltype(nullptr)) noexcept {
        return lhs.self == nullptr;
    }

    [[nodiscard]] constexpr bool operator==(decltype(nullptr), StackRef rhs) noexcept {
        return rhs == nullptr;
    }

    [[nodiscard]] constexpr bool operator!=(StackRef lhs, decltype(nullptr)) noexcept {
        return !(lhs == nullptr);
    }

    [[nodiscard]] constexpr bool operator!=(decltype(nullptr), StackRef rhs) noexcept {
        return !(rhs == nullptr);
    }

    struct MacAddress {
        std::array<util::u8, 6> bytes{};

        [[nodiscard]] constexpr bool is_zero() const noexcept {
            for (auto byte : bytes) {
                if (byte != 0u) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] constexpr bool is_broadcast() const noexcept {
            for (auto byte : bytes) {
                if (byte != 0xFFu) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] static constexpr MacAddress from_bytes(util::u8 b0,
                                                             util::u8 b1,
                                                             util::u8 b2,
                                                             util::u8 b3,
                                                             util::u8 b4,
                                                             util::u8 b5) noexcept {
            return MacAddress{{b0, b1, b2, b3, b4, b5}};
        }

        [[nodiscard]] static constexpr MacAddress broadcast() noexcept {
            return from_bytes(0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu);
        }
    };

    enum class NetIfState : util::u8 {
        detached,
        down,
        up,
    };

    enum class NetIfCapability : util::u32 {
        rx = 1u << 0,
        tx = 1u << 1,
        loopback = 1u << 2,
        broadcast = 1u << 3,
        multicast = 1u << 4,
    };

    using NetIfCapabilityMask = util::u32;

    [[nodiscard]] constexpr NetIfCapabilityMask capability_mask(NetIfCapability capability) noexcept {
        return static_cast<NetIfCapabilityMask>(capability);
    }

    [[nodiscard]] constexpr NetIfCapabilityMask operator|(
        NetIfCapability lhs,
        NetIfCapability rhs) noexcept {
        return capability_mask(lhs) | capability_mask(rhs);
    }

    [[nodiscard]] constexpr NetIfCapabilityMask operator|(
        NetIfCapabilityMask lhs,
        NetIfCapability rhs) noexcept {
        return lhs | capability_mask(rhs);
    }

    [[nodiscard]] constexpr NetIfCapabilityMask operator|(
        NetIfCapability lhs,
        NetIfCapabilityMask rhs) noexcept {
        return capability_mask(lhs) | rhs;
    }

    [[nodiscard]] constexpr bool has_capability(NetIfCapabilityMask mask, NetIfCapability capability) noexcept {
        return (mask & capability_mask(capability)) != 0u;
    }

    struct NetIfConfig {
        util::u16 mtu{1500};
        MacAddress mac{};
        IpAddress address{};
        NetIfCapabilityMask capabilities{
            capability_mask(NetIfCapability::rx) | capability_mask(NetIfCapability::tx)
        };
    };

    template <typename T>
    concept PacketSink = requires(T& t, PacketView packet) {
        { t.consume(packet) } noexcept -> std::same_as<Result<void>>;
    };

    template <typename T>
    concept OwnedPacketSink = requires(T& t, OwnedPacket packet) {
        { t.consume(static_cast<OwnedPacket&&>(packet)) } noexcept -> std::same_as<Result<void>>;
    };

    struct PacketSinkOps {
        Result<void> (*consume)(void*, PacketView) noexcept;
    };

    struct OwnedPacketSinkOps {
        Result<void> (*consume)(void*, OwnedPacket) noexcept;
    };

    struct PacketSinkRef {
        void* self{nullptr};
        const PacketSinkOps* ops{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return self != nullptr && ops != nullptr && ops->consume != nullptr;
        }

        [[nodiscard]] Result<void> consume(PacketView packet) const noexcept {
            if (!valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            return ops->consume(self, packet);
        }
    };

    struct OwnedPacketSinkRef {
        void* self{nullptr};
        const OwnedPacketSinkOps* ops{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return self != nullptr && ops != nullptr && ops->consume != nullptr;
        }

        [[nodiscard]] Result<void> consume(OwnedPacket packet) const noexcept {
            if (!valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            return ops->consume(self, static_cast<OwnedPacket&&>(packet));
        }
    };

    template <PacketSink T>
    inline const PacketSinkOps* packet_sink_ops() noexcept {
        static const PacketSinkOps ops{
            .consume = [](void* self, PacketView packet) noexcept {
                return static_cast<T*>(self)->consume(packet);
            }
        };
        return &ops;
    }

    template <OwnedPacketSink T>
    inline const OwnedPacketSinkOps* owned_packet_sink_ops() noexcept {
        static const OwnedPacketSinkOps ops{
            .consume = [](void* self, OwnedPacket packet) noexcept {
                return static_cast<T*>(self)->consume(static_cast<OwnedPacket&&>(packet));
            }
        };
        return &ops;
    }

    template <PacketSink T>
    inline PacketSinkRef make_packet_sink_ref(T& sink) noexcept {
        return PacketSinkRef{&sink, packet_sink_ops<T>()};
    }

    template <OwnedPacketSink T>
    inline OwnedPacketSinkRef make_owned_packet_sink_ref(T& sink) noexcept {
        return OwnedPacketSinkRef{&sink, owned_packet_sink_ops<T>()};
    }

    class NetIf {
    public:
        NetIf() noexcept = default;

        [[nodiscard]] constexpr NetIfConfig config() const noexcept {
            return NetIfConfig{
                .mtu = mtu_,
                .mac = mac_,
                .address = address_,
                .capabilities = capabilities_
            };
        }

        [[nodiscard]] constexpr util::u16 mtu() const noexcept {
            return mtu_;
        }

        [[nodiscard]] constexpr MacAddress mac() const noexcept {
            return mac_;
        }

        [[nodiscard]] constexpr IpAddress address() const noexcept {
            return address_;
        }

        [[nodiscard]] constexpr NetIfCapabilityMask capabilities() const noexcept {
            return capabilities_;
        }

        [[nodiscard]] constexpr bool supports(NetIfCapability capability) const noexcept {
            return has_capability(capabilities_, capability);
        }

        [[nodiscard]] constexpr NetIfState state() const noexcept {
            return state_;
        }

        [[nodiscard]] constexpr bool bound() const noexcept {
            return stack_.valid();
        }

        [[nodiscard]] constexpr StackRef stack() const noexcept {
            return stack_;
        }

        [[nodiscard]] constexpr OwnedPacketSinkRef input_sink() const noexcept {
            return input_sink_;
        }

        [[nodiscard]] constexpr PacketSinkRef output_sink() const noexcept {
            return output_sink_;
        }

        [[nodiscard]] Result<void> configure(const NetIfConfig& config) noexcept {
            if (config.mtu == 0) {
                return util::unexpected(errc::invalid_arg);
            }
            if (!config.address.is_unspecified()) {
                auto supported = validate_supported_family_v0(config.address.family);
                if (!supported) {
                    return util::unexpected(supported.error());
                }
            }

            mtu_ = config.mtu;
            mac_ = config.mac;
            address_ = config.address;
            capabilities_ = config.capabilities;
            return {};
        }

        constexpr void set_input_sink(OwnedPacketSinkRef sink) noexcept {
            input_sink_ = sink;
        }

        constexpr void set_output_sink(PacketSinkRef sink) noexcept {
            output_sink_ = sink;
        }

        [[nodiscard]] Result<void> bind(StackRef stack) noexcept {
            if (!stack.valid()) {
                return util::unexpected(errc::invalid_arg);
            }
            if (stack_ == stack.self) {
                return util::unexpected(errc::exist);
            }
            if (stack_.valid()) {
                return util::unexpected(errc::busy);
            }

            stack_ = stack;
            state_ = NetIfState::down;
            return {};
        }

        template <typename T>
        [[nodiscard]] Result<void> bind(T& stack) noexcept {
            return bind(StackRef{&stack});
        }

        void unbind() noexcept {
            stack_ = {};
            state_ = NetIfState::detached;
        }

        [[nodiscard]] Result<void> bring_up() noexcept {
            if (!bound()) {
                return util::unexpected(errc::bad_state);
            }
            state_ = NetIfState::up;
            return {};
        }

        void bring_down() noexcept {
            state_ = bound() ? NetIfState::down : NetIfState::detached;
        }

        [[nodiscard]] Result<void> deliver_input(OwnedPacket packet) const noexcept {
            auto checked = validate_packet(packet.view(), NetIfCapability::rx);
            if (!checked) {
                return util::unexpected(checked.error());
            }
            if (!input_sink_.valid()) {
                return util::unexpected(errc::not_supported);
            }
            return input_sink_.consume(static_cast<OwnedPacket&&>(packet));
        }

        [[nodiscard]] Result<void> deliver_input(PacketView packet) const noexcept {
            return deliver_input(OwnedPacket::borrowed(packet));
        }

        [[nodiscard]] Result<void> transmit(PacketView packet) const noexcept {
            auto checked = validate_packet(packet, NetIfCapability::tx);
            if (!checked) {
                return util::unexpected(checked.error());
            }
            if (!output_sink_.valid()) {
                return util::unexpected(errc::not_supported);
            }
            return output_sink_.consume(packet);
        }

    private:
        [[nodiscard]] Result<void> validate_packet(PacketView packet,
                                                   NetIfCapability required_capability) const noexcept {
            if (state_ != NetIfState::up) {
                return util::unexpected(errc::bad_state);
            }
            if (!supports(required_capability)) {
                return util::unexpected(errc::not_supported);
            }
            if (packet.empty() || packet.size() > mtu_) {
                return util::unexpected(errc::invalid_arg);
            }
            return {};
        }

        util::u16 mtu_{1500};
        MacAddress mac_{};
        IpAddress address_{};
        NetIfCapabilityMask capabilities_{
            capability_mask(NetIfCapability::rx) | capability_mask(NetIfCapability::tx)
        };
        StackRef stack_{};
        OwnedPacketSinkRef input_sink_{};
        PacketSinkRef output_sink_{};
        NetIfState state_{NetIfState::detached};
    };
}
