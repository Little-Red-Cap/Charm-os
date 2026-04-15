module;

#include <utility>

export module net.pump;

import net.arp;
import net.netif;
import net.stack;
import net.udp;
import util.core;
import util.error;
import util.expected;

export namespace net {
    struct UdpEgressPumpConfig {
        util::usize retry_interval_ticks{1};
        util::usize max_attempts{static_cast<util::usize>(-1)};
    };

    struct UdpEgressPumpProgress {
        bool polled_links{false};
        util::usize arp_retried{0};
        util::usize arp_timed_out{0};
        util::usize flushed{0};
        util::usize dropped{0};
    };

    template <util::usize TxCapacity,
              util::usize ArpCapacity,
              util::usize ArpTxCapacity,
              util::usize PendingCapacity,
              util::usize PayloadCapacity>
    class UdpEgressPump {
    public:
        UdpEgressPump() noexcept = default;

        explicit UdpEgressPump(Stack& stack,
                               NetIf& netif,
                               ArpService<ArpCapacity, ArpTxCapacity>& arp,
                               UdpEgressPumpConfig config = {}) noexcept
            : stack_(&stack)
            , netif_(&netif)
            , arp_(&arp)
            , config_(config) {}

        void bind(Stack& stack,
                  NetIf& netif,
                  ArpService<ArpCapacity, ArpTxCapacity>& arp) noexcept {
            stack_ = &stack;
            netif_ = &netif;
            arp_ = &arp;
        }

        void configure(UdpEgressPumpConfig config) noexcept {
            config_ = config;
        }

        [[nodiscard]] const UdpEgressPumpConfig& config() const noexcept {
            return config_;
        }

        [[nodiscard]] bool bound() const noexcept {
            return stack_ != nullptr
                && netif_ != nullptr
                && arp_ != nullptr;
        }

        [[nodiscard]] Stack* stack() const noexcept {
            return stack_;
        }

        [[nodiscard]] NetIf* netif() const noexcept {
            return netif_;
        }

        [[nodiscard]] ArpService<ArpCapacity, ArpTxCapacity>* arp() const noexcept {
            return arp_;
        }

        [[nodiscard]] UdpEgressQueue<PendingCapacity, PayloadCapacity>& queue() noexcept {
            return queue_;
        }

        [[nodiscard]] const UdpEgressQueue<PendingCapacity, PayloadCapacity>& queue() const noexcept {
            return queue_;
        }

        [[nodiscard]] util::usize pending_count() const noexcept {
            return queue_.pending_count();
        }

        [[nodiscard]] util::usize queued_count() const noexcept {
            return queue_.queued_count();
        }

        [[nodiscard]] util::usize flushed_count() const noexcept {
            return queue_.flushed_count();
        }

        [[nodiscard]] util::usize dropped_count() const noexcept {
            return queue_.dropped_count();
        }

        [[nodiscard]] Result<UdpSendDisposition> send(Endpoint local,
                                                      const Endpoint& peer,
                                                      ByteView payload,
                                                      util::u8 ttl = 64,
                                                      util::u16 identification = 0,
                                                      util::u8 dscp_ecn = 0) noexcept {
            if (!bound()) {
                return util::unexpected(errc::bad_state);
            }
            return queue_.template send<TxCapacity>(
                *netif_,
                *arp_,
                local,
                peer,
                payload,
                ttl,
                identification,
                dscp_ecn);
        }

        [[nodiscard]] Result<UdpEgressPumpProgress> service(util::usize elapsed_ticks = 1) noexcept {
            if (!bound()) {
                return util::unexpected(errc::bad_state);
            }

            auto polled = stack_->poll_links();
            if (!polled) {
                return util::unexpected(polled.error());
            }

            auto progress = queue_.template service<TxCapacity>(
                *netif_,
                *arp_,
                elapsed_ticks,
                config_.retry_interval_ticks,
                config_.max_attempts);
            if (!progress) {
                return util::unexpected(progress.error());
            }

            return Result<UdpEgressPumpProgress>{std::in_place, UdpEgressPumpProgress{
                .polled_links = true,
                .arp_retried = progress.value().arp_retried,
                .arp_timed_out = progress.value().arp_timed_out,
                .flushed = progress.value().flushed,
                .dropped = progress.value().dropped,
            }};
        }

    private:
        Stack* stack_{nullptr};
        NetIf* netif_{nullptr};
        ArpService<ArpCapacity, ArpTxCapacity>* arp_{nullptr};
        UdpEgressQueue<PendingCapacity, PayloadCapacity> queue_{};
        UdpEgressPumpConfig config_{};
    };
}
