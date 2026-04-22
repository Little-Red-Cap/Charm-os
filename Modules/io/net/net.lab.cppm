module;

#include <array>
#include <concepts>

export module net.lab;

export import net.packet;
export import net.driver;
export import net.stack;
import util.core;
import util.error;
import util.expected;

namespace net::lab::detail {
    template <typename Provider>
    concept MacConfigurableProvider = requires(Provider& provider, MacAddress mac) {
        provider.set_mac(mac);
    };

    template <typename Pump>
    concept StackBindablePump = requires(Pump& pump, Stack& stack, NetIf& netif) {
        { pump.bind(stack, netif) } noexcept;
    };

    template <typename Pump, typename Config>
    concept PumpConfigurable = requires(Pump& pump, const Config& config) {
        pump.configure(config);
    };

    template <typename Pump>
    concept ServiceablePump = requires(Pump& pump, util::usize elapsed_ticks) {
        pump.service(elapsed_ticks);
    };
}

export namespace net::lab {
    struct LinkDirectionConfig {
        util::u32 latency_ticks{1};
    };

    struct LinkDirectionStats {
        util::usize scheduled{0};
        util::usize delivered{0};
        util::usize dropped{0};
        util::usize rejected{0};
        util::usize pending{0};
    };

    template <util::usize Capacity, util::usize QueueDepth = 4, util::usize InFlightDepth = 8>
    class DuplexLink {
    private:
        enum class Side : util::u8 {
            a,
            b,
        };

        struct FrameSlot {
            bool used{false};
            bool drop{false};
            util::u32 due_tick{0};
            util::usize size{0};
            std::array<util::u8, Capacity> bytes{};
        };

        struct DirectionState {
            LinkDirectionConfig config{};
            std::array<FrameSlot, InFlightDepth> in_flight{};
            util::usize scheduled{0};
            util::usize delivered{0};
            util::usize dropped{0};
            util::usize rejected{0};
            bool drop_next{false};
        };

    public:
        class Endpoint {
        public:
            Endpoint() noexcept = default;

            void set_mac(MacAddress mac) noexcept {
                mac_ = mac;
            }

            [[nodiscard]] MacAddress mac() const noexcept {
                return mac_;
            }

            [[nodiscard]] NetDriverInfo info() const noexcept {
                return NetDriverInfo{
                    .mtu = Capacity,
                    .mac = mac_,
                    .capabilities = NetIfCapability::rx
                        | NetIfCapability::tx
                        | NetIfCapability::broadcast
                };
            }

            [[nodiscard]] Result<void> set_input_sink(OwnedPacketSinkRef sink) noexcept {
                input_sink_ = sink;
                return {};
            }

            [[nodiscard]] bool has_rx() const noexcept {
                return rx_count_ != 0;
            }

            [[nodiscard]] util::usize rx_pending() const noexcept {
                return rx_count_;
            }

            [[nodiscard]] Result<void> poll() noexcept {
                if (rx_count_ == 0) {
                    return {};
                }
                if (!input_sink_.valid()) {
                    return util::unexpected(errc::bad_state);
                }

                auto lease = rx_pool_.acquire();
                if (!lease) {
                    return util::unexpected(lease.error());
                }

                auto appended = lease.value()->append(ByteView{
                    rx_frames_[rx_head_].data(),
                    rx_sizes_[rx_head_]
                });
                if (!appended) {
                    return util::unexpected(appended.error());
                }

                rx_head_ = next_index(rx_head_);
                --rx_count_;

                return input_sink_.consume(OwnedPacket{
                    static_cast<typename PacketPool<QueueDepth, Capacity>::Lease&&>(lease.value())
                });
            }

            [[nodiscard]] Result<void> transmit(PacketView packet) noexcept {
                if (owner_ == nullptr) {
                    return util::unexpected(errc::bad_state);
                }
                return owner_->transmit_from(side_, packet);
            }

        private:
            friend class DuplexLink;

            Endpoint(DuplexLink* owner, Side side) noexcept
                : owner_(owner),
                  side_(side) {}

            static constexpr util::usize next_index(util::usize current) noexcept {
                return (current + 1) % QueueDepth;
            }

            [[nodiscard]] Result<void> enqueue_rx(ByteView packet) noexcept {
                if (rx_count_ == QueueDepth || packet.size() > Capacity) {
                    return util::unexpected(errc::buffer_overflow);
                }

                rx_sizes_[rx_tail_] = packet.size();
                for (util::usize index = 0; index < packet.size(); ++index) {
                    rx_frames_[rx_tail_][index] = packet[index];
                }

                rx_tail_ = next_index(rx_tail_);
                ++rx_count_;
                return {};
            }

            DuplexLink* owner_{nullptr};
            Side side_{Side::a};
            OwnedPacketSinkRef input_sink_{};
            PacketPool<QueueDepth, Capacity> rx_pool_{};
            std::array<std::array<util::u8, Capacity>, QueueDepth> rx_frames_{};
            std::array<util::usize, QueueDepth> rx_sizes_{};
            util::usize rx_head_{0};
            util::usize rx_tail_{0};
            util::usize rx_count_{0};
            MacAddress mac_{};
        };

        DuplexLink() noexcept
            : endpoint_a_(this, Side::a),
              endpoint_b_(this, Side::b) {}

        [[nodiscard]] Endpoint& endpoint_a() noexcept {
            return endpoint_a_;
        }

        [[nodiscard]] const Endpoint& endpoint_a() const noexcept {
            return endpoint_a_;
        }

        [[nodiscard]] Endpoint& endpoint_b() noexcept {
            return endpoint_b_;
        }

        [[nodiscard]] const Endpoint& endpoint_b() const noexcept {
            return endpoint_b_;
        }

        void set_latency_a_to_b(util::u32 latency_ticks) noexcept {
            a_to_b_.config.latency_ticks = latency_ticks;
        }

        void set_latency_b_to_a(util::u32 latency_ticks) noexcept {
            b_to_a_.config.latency_ticks = latency_ticks;
        }

        void set_latency_a_to_b(LinkDirectionConfig config) noexcept {
            a_to_b_.config = config;
        }

        void set_latency_b_to_a(LinkDirectionConfig config) noexcept {
            b_to_a_.config = config;
        }

        [[nodiscard]] LinkDirectionConfig config_a_to_b() const noexcept {
            return a_to_b_.config;
        }

        [[nodiscard]] LinkDirectionConfig config_b_to_a() const noexcept {
            return b_to_a_.config;
        }

        void drop_next_a_to_b() noexcept {
            a_to_b_.drop_next = true;
        }

        void drop_next_b_to_a() noexcept {
            b_to_a_.drop_next = true;
        }

        [[nodiscard]] LinkDirectionStats stats_a_to_b() const noexcept {
            return make_stats(a_to_b_);
        }

        [[nodiscard]] LinkDirectionStats stats_b_to_a() const noexcept {
            return make_stats(b_to_a_);
        }

        [[nodiscard]] util::u32 now_ticks() const noexcept {
            return now_ticks_;
        }

        [[nodiscard]] util::usize pending_count() const noexcept {
            return pending_count(a_to_b_) + pending_count(b_to_a_);
        }

        [[nodiscard]] bool idle() const noexcept {
            return pending_count() == 0
                && !endpoint_a_.has_rx()
                && !endpoint_b_.has_rx();
        }

        void advance(util::u32 ticks = 1) noexcept {
            if (ticks == 0) {
                deliver_due();
                return;
            }

            for (util::u32 step = 0; step < ticks; ++step) {
                ++now_ticks_;
                deliver_due();
            }
        }

    private:
        [[nodiscard]] static util::usize pending_count(const DirectionState& direction) noexcept {
            util::usize count = 0;
            for (const auto& slot : direction.in_flight) {
                if (slot.used) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] static LinkDirectionStats make_stats(const DirectionState& direction) noexcept {
            return LinkDirectionStats{
                .scheduled = direction.scheduled,
                .delivered = direction.delivered,
                .dropped = direction.dropped,
                .rejected = direction.rejected,
                .pending = pending_count(direction),
            };
        }

        [[nodiscard]] static FrameSlot* reserve_slot(DirectionState& direction) noexcept {
            for (auto& slot : direction.in_flight) {
                if (!slot.used) {
                    return &slot;
                }
            }
            return nullptr;
        }

        static void clear_slot(FrameSlot& slot) noexcept {
            slot.used = false;
            slot.drop = false;
            slot.due_tick = 0;
            slot.size = 0;
        }

        [[nodiscard]] Result<void> transmit_from(Side source, PacketView packet) noexcept {
            if (packet.empty()) {
                return util::unexpected(errc::invalid_arg);
            }
            if (packet.size() > Capacity) {
                return util::unexpected(errc::buffer_overflow);
            }

            auto& direction = source == Side::a ? a_to_b_ : b_to_a_;
            auto* slot = reserve_slot(direction);
            if (slot == nullptr) {
                return util::unexpected(errc::busy);
            }

            slot->used = true;
            slot->drop = direction.drop_next;
            slot->due_tick = now_ticks_ + direction.config.latency_ticks;
            slot->size = packet.size();
            direction.drop_next = false;

            for (util::usize index = 0; index < packet.size(); ++index) {
                slot->bytes[index] = packet[index];
            }

            ++direction.scheduled;
            return {};
        }

        void deliver_due() noexcept {
            deliver_due(a_to_b_, endpoint_b_);
            deliver_due(b_to_a_, endpoint_a_);
        }

        static void deliver_due(DirectionState& direction, Endpoint& destination) noexcept {
            for (auto& slot : direction.in_flight) {
                if (!slot.used || slot.due_tick > destination.owner_->now_ticks_) {
                    continue;
                }

                if (slot.drop) {
                    ++direction.dropped;
                    clear_slot(slot);
                    continue;
                }

                auto queued = destination.enqueue_rx(ByteView{slot.bytes.data(), slot.size});
                if (queued) {
                    ++direction.delivered;
                } else {
                    ++direction.rejected;
                }

                clear_slot(slot);
            }
        }

        Endpoint endpoint_a_;
        Endpoint endpoint_b_;
        DirectionState a_to_b_{};
        DirectionState b_to_a_{};
        util::u32 now_ticks_{0};
    };

    template <class Pump>
    class StackNode {
    public:
        StackNode() noexcept = default;

        [[nodiscard]] NetIf& netif() noexcept {
            return netif_;
        }

        [[nodiscard]] const NetIf& netif() const noexcept {
            return netif_;
        }

        [[nodiscard]] NetDriver& driver() noexcept {
            return driver_;
        }

        [[nodiscard]] const NetDriver& driver() const noexcept {
            return driver_;
        }

        [[nodiscard]] Stack& stack() noexcept {
            return stack_;
        }

        [[nodiscard]] const Stack& stack() const noexcept {
            return stack_;
        }

        [[nodiscard]] Pump& pump() noexcept {
            return pump_;
        }

        [[nodiscard]] const Pump& pump() const noexcept {
            return pump_;
        }

        [[nodiscard]] bool ready() const noexcept {
            return driver_.attached() && netif_.state() == NetIfState::up;
        }

        template <NetDriverProvider Provider>
            requires detail::StackBindablePump<Pump>
        [[nodiscard]] Result<void> init(Provider& provider, IpAddress address) noexcept {
            return init_impl(provider, address);
        }

        template <NetDriverProvider Provider>
            requires detail::StackBindablePump<Pump> && detail::MacConfigurableProvider<Provider>
        [[nodiscard]] Result<void> init(Provider& provider,
                                        MacAddress mac,
                                        IpAddress address) noexcept {
            provider.set_mac(mac);
            return init_impl(provider, address);
        }

        template <NetDriverProvider Provider, class PumpConfig>
            requires detail::StackBindablePump<Pump> && detail::PumpConfigurable<Pump, PumpConfig>
        [[nodiscard]] Result<void> init(Provider& provider,
                                        IpAddress address,
                                        const PumpConfig& config) noexcept {
            auto initialized = init(provider, address);
            if (!initialized) {
                return util::unexpected(initialized.error());
            }
            pump_.configure(config);
            return {};
        }

        template <NetDriverProvider Provider, class PumpConfig>
            requires detail::StackBindablePump<Pump>
                && detail::MacConfigurableProvider<Provider>
                && detail::PumpConfigurable<Pump, PumpConfig>
        [[nodiscard]] Result<void> init(Provider& provider,
                                        MacAddress mac,
                                        IpAddress address,
                                        const PumpConfig& config) noexcept {
            auto initialized = init(provider, mac, address);
            if (!initialized) {
                return util::unexpected(initialized.error());
            }
            pump_.configure(config);
            return {};
        }

        template <class PumpConfig>
            requires detail::PumpConfigurable<Pump, PumpConfig>
        void configure_pump(const PumpConfig& config) noexcept(noexcept(pump_.configure(config))) {
            pump_.configure(config);
        }

        [[nodiscard]] auto service(util::usize elapsed_ticks = 1) noexcept
            requires detail::ServiceablePump<Pump> {
            return pump_.service(elapsed_ticks);
        }

    private:
        template <NetDriverProvider Provider>
            requires detail::StackBindablePump<Pump>
        [[nodiscard]] Result<void> init_impl(Provider& provider,
                                             IpAddress address) noexcept {
            const auto info = provider.info();

            auto configured = netif_.configure(NetIfConfig{
                .mtu = info.mtu,
                .mac = info.mac,
                .address = address,
                .capabilities = info.capabilities
            });
            if (!configured) {
                return util::unexpected(configured.error());
            }

            auto attached = driver_.attach(make_net_driver_provider_ref(provider), netif_);
            if (!attached) {
                return util::unexpected(attached.error());
            }

            auto registered = stack_.register_driver(driver_);
            if (!registered) {
                return util::unexpected(registered.error());
            }

            auto up = netif_.bring_up();
            if (!up) {
                return util::unexpected(up.error());
            }

            pump_.bind(stack_, netif_);
            return {};
        }

        NetIf netif_{};
        NetDriver driver_{};
        Stack stack_{};
        Pump pump_{};
    };
}
