module;

#include <utility>

export module net.pump;

import net.arp;
import net.icmp;
import net.ipv4;
import net.netif;
import net.stack;
import net.udp;
import util.core;
import util.error;
import util.expected;

export namespace net {
    struct IcmpEgressPumpConfig {
        util::usize retry_interval_ticks{1};
        util::usize max_attempts{static_cast<util::usize>(-1)};
    };

    struct IcmpEgressPumpProgress {
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
    class IcmpEgressPump {
    public:
        IcmpEgressPump() noexcept = default;

        explicit IcmpEgressPump(Stack& stack,
                                NetIf& netif,
                                ArpService<ArpCapacity, ArpTxCapacity>& arp,
                                IcmpEgressPumpConfig config = {}) noexcept
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

        void configure(IcmpEgressPumpConfig config) noexcept {
            config_ = config;
        }

        [[nodiscard]] const IcmpEgressPumpConfig& config() const noexcept {
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

        [[nodiscard]] IcmpEgressQueue<PendingCapacity, PayloadCapacity>& queue() noexcept {
            return queue_;
        }

        [[nodiscard]] const IcmpEgressQueue<PendingCapacity, PayloadCapacity>& queue() const noexcept {
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

        [[nodiscard]] Result<IcmpSendDisposition> send(IpAddress local,
                                                       IpAddress peer,
                                                       IcmpType type,
                                                       util::u16 identifier,
                                                       util::u16 sequence,
                                                       ByteView payload,
                                                       util::u8 ttl = 64,
                                                       util::u16 ipv4_identification = 0,
                                                       util::u8 dscp_ecn = 0) noexcept {
            if (!bound()) {
                return util::unexpected(errc::bad_state);
            }
            return queue_.template send<TxCapacity>(
                *netif_,
                *arp_,
                local,
                peer,
                type,
                identifier,
                sequence,
                payload,
                ttl,
                ipv4_identification,
                dscp_ecn);
        }

        [[nodiscard]] Result<IcmpSendDisposition> send_request(IpAddress local,
                                                               IpAddress peer,
                                                               util::u16 identifier,
                                                               util::u16 sequence,
                                                               ByteView payload,
                                                               util::u8 ttl = 64,
                                                               util::u16 ipv4_identification = 0,
                                                               util::u8 dscp_ecn = 0) noexcept {
            if (!bound()) {
                return util::unexpected(errc::bad_state);
            }
            return queue_.template send_request<TxCapacity>(
                *netif_,
                *arp_,
                local,
                peer,
                identifier,
                sequence,
                payload,
                ttl,
                ipv4_identification,
                dscp_ecn);
        }

        [[nodiscard]] Result<IcmpSendDisposition> send_reply(IpAddress local,
                                                             IpAddress peer,
                                                             util::u16 identifier,
                                                             util::u16 sequence,
                                                             ByteView payload,
                                                             util::u8 ttl = 64,
                                                             util::u16 ipv4_identification = 0,
                                                             util::u8 dscp_ecn = 0) noexcept {
            if (!bound()) {
                return util::unexpected(errc::bad_state);
            }
            return queue_.template send_reply<TxCapacity>(
                *netif_,
                *arp_,
                local,
                peer,
                identifier,
                sequence,
                payload,
                ttl,
                ipv4_identification,
                dscp_ecn);
        }

        [[nodiscard]] Result<IcmpEgressPumpProgress> service(util::usize elapsed_ticks = 1) noexcept {
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

            return Result<IcmpEgressPumpProgress>{std::in_place, IcmpEgressPumpProgress{
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
        IcmpEgressQueue<PendingCapacity, PayloadCapacity> queue_{};
        IcmpEgressPumpConfig config_{};
    };

    struct IcmpStackPumpConfig {
        IcmpEgressPumpConfig egress{};
    };

    struct IcmpStackPumpProgress {
        bool polled_links{false};
        util::usize ipv4_delivered{0};
        util::usize ipv4_dropped{0};
        util::usize icmp_delivered{0};
        util::usize icmp_dropped{0};
        util::usize icmp_requests{0};
        util::usize icmp_replies{0};
        util::usize icmp_error_quotes{0};
        util::usize icmp_time_exceeded{0};
        util::usize icmp_destination_unreachable{0};
        util::usize arp_retried{0};
        util::usize arp_timed_out{0};
        util::usize egress_flushed{0};
        util::usize egress_dropped{0};
    };

    template <util::usize TxCapacity,
              util::usize ArpCapacity,
              util::usize ArpTxCapacity,
              util::usize PendingCapacity,
              util::usize PayloadCapacity>
    class IcmpStackPump {
    public:
        IcmpStackPump() noexcept = default;

        explicit IcmpStackPump(Stack& stack,
                               NetIf& netif,
                               IcmpStackPumpConfig config = {}) noexcept
            : stack_(&stack)
            , netif_(&netif)
            , arp_(netif)
            , ipv4_(netif)
            , egress_(stack, netif, arp_, config.egress)
            , config_(config) {
            wire_stack_sinks();
        }

        void bind(Stack& stack, NetIf& netif) noexcept {
            stack_ = &stack;
            netif_ = &netif;
            arp_.bind(netif);
            ipv4_.bind(netif);
            egress_.bind(stack, netif, arp_);
            wire_stack_sinks();
        }

        void configure(IcmpStackPumpConfig config) noexcept {
            config_ = config;
            egress_.configure(config.egress);
        }

        [[nodiscard]] const IcmpStackPumpConfig& config() const noexcept {
            return config_;
        }

        [[nodiscard]] bool bound() const noexcept {
            return stack_ != nullptr
                && netif_ != nullptr
                && egress_.bound();
        }

        [[nodiscard]] Stack* stack() const noexcept {
            return stack_;
        }

        [[nodiscard]] NetIf* netif() const noexcept {
            return netif_;
        }

        [[nodiscard]] ArpService<ArpCapacity, ArpTxCapacity>& arp() noexcept {
            return arp_;
        }

        [[nodiscard]] const ArpService<ArpCapacity, ArpTxCapacity>& arp() const noexcept {
            return arp_;
        }

        [[nodiscard]] Ipv4Service& ipv4() noexcept {
            return ipv4_;
        }

        [[nodiscard]] const Ipv4Service& ipv4() const noexcept {
            return ipv4_;
        }

        [[nodiscard]] IcmpEchoService& icmp() noexcept {
            return icmp_;
        }

        [[nodiscard]] const IcmpEchoService& icmp() const noexcept {
            return icmp_;
        }

        [[nodiscard]] IcmpEgressPump<TxCapacity,
                                     ArpCapacity,
                                     ArpTxCapacity,
                                     PendingCapacity,
                                     PayloadCapacity>& egress() noexcept {
            return egress_;
        }

        [[nodiscard]] const IcmpEgressPump<TxCapacity,
                                           ArpCapacity,
                                           ArpTxCapacity,
                                           PendingCapacity,
                                           PayloadCapacity>& egress() const noexcept {
            return egress_;
        }

        void set_echo_sink(IcmpEchoSinkRef sink) noexcept {
            icmp_.set_sink(sink);
        }

        template <IcmpEchoSink T>
        void set_echo_sink(T& sink) noexcept {
            icmp_.set_sink(sink);
        }

        void set_error_quote_sink(IcmpErrorQuoteSinkRef sink) noexcept {
            icmp_.set_error_quote_sink(sink);
        }

        template <IcmpErrorQuoteSink T>
        void set_error_quote_sink(T& sink) noexcept {
            icmp_.set_error_quote_sink(sink);
        }

        [[nodiscard]] bool has_echo_sink() const noexcept {
            return icmp_.has_sink();
        }

        [[nodiscard]] bool has_error_quote_sink() const noexcept {
            return icmp_.has_error_quote_sink();
        }

        [[nodiscard]] util::usize pending_count() const noexcept {
            return egress_.pending_count();
        }

        [[nodiscard]] util::usize queued_count() const noexcept {
            return egress_.queued_count();
        }

        [[nodiscard]] util::usize flushed_count() const noexcept {
            return egress_.flushed_count();
        }

        [[nodiscard]] util::usize dropped_count() const noexcept {
            return egress_.dropped_count();
        }

        [[nodiscard]] Result<IcmpSendDisposition> send(IpAddress local,
                                                       IpAddress peer,
                                                       IcmpType type,
                                                       util::u16 identifier,
                                                       util::u16 sequence,
                                                       ByteView payload,
                                                       util::u8 ttl = 64,
                                                       util::u16 ipv4_identification = 0,
                                                       util::u8 dscp_ecn = 0) noexcept {
            return egress_.send(
                local,
                peer,
                type,
                identifier,
                sequence,
                payload,
                ttl,
                ipv4_identification,
                dscp_ecn);
        }

        [[nodiscard]] Result<IcmpSendDisposition> send_request(IpAddress local,
                                                               IpAddress peer,
                                                               util::u16 identifier,
                                                               util::u16 sequence,
                                                               ByteView payload,
                                                               util::u8 ttl = 64,
                                                               util::u16 ipv4_identification = 0,
                                                               util::u8 dscp_ecn = 0) noexcept {
            return egress_.send_request(
                local,
                peer,
                identifier,
                sequence,
                payload,
                ttl,
                ipv4_identification,
                dscp_ecn);
        }

        [[nodiscard]] Result<IcmpSendDisposition> send_reply(IpAddress local,
                                                             IpAddress peer,
                                                             util::u16 identifier,
                                                             util::u16 sequence,
                                                             ByteView payload,
                                                             util::u8 ttl = 64,
                                                             util::u16 ipv4_identification = 0,
                                                             util::u8 dscp_ecn = 0) noexcept {
            return egress_.send_reply(
                local,
                peer,
                identifier,
                sequence,
                payload,
                ttl,
                ipv4_identification,
                dscp_ecn);
        }

        [[nodiscard]] Result<IcmpStackPumpProgress> service(util::usize elapsed_ticks = 1) noexcept {
            if (!bound()) {
                return util::unexpected(errc::bad_state);
            }

            const auto ipv4_packets_before = ipv4_.packet_count();
            const auto ipv4_drops_before = ipv4_.drop_count();
            const auto icmp_packets_before = icmp_.packet_count();
            const auto icmp_drops_before = icmp_.drop_count();
            const auto icmp_requests_before = icmp_.request_count();
            const auto icmp_replies_before = icmp_.reply_count();
            const auto icmp_error_quotes_before = icmp_.error_quote_count();
            const auto icmp_time_exceeded_before = icmp_.time_exceeded_count();
            const auto icmp_destination_unreachable_before = icmp_.destination_unreachable_count();

            auto progress = egress_.service(elapsed_ticks);
            if (!progress) {
                return util::unexpected(progress.error());
            }

            return Result<IcmpStackPumpProgress>{std::in_place, IcmpStackPumpProgress{
                .polled_links = progress.value().polled_links,
                .ipv4_delivered = ipv4_.packet_count() - ipv4_packets_before,
                .ipv4_dropped = ipv4_.drop_count() - ipv4_drops_before,
                .icmp_delivered = icmp_.packet_count() - icmp_packets_before,
                .icmp_dropped = icmp_.drop_count() - icmp_drops_before,
                .icmp_requests = icmp_.request_count() - icmp_requests_before,
                .icmp_replies = icmp_.reply_count() - icmp_replies_before,
                .icmp_error_quotes = icmp_.error_quote_count() - icmp_error_quotes_before,
                .icmp_time_exceeded = icmp_.time_exceeded_count() - icmp_time_exceeded_before,
                .icmp_destination_unreachable =
                    icmp_.destination_unreachable_count() - icmp_destination_unreachable_before,
                .arp_retried = progress.value().arp_retried,
                .arp_timed_out = progress.value().arp_timed_out,
                .egress_flushed = progress.value().flushed,
                .egress_dropped = progress.value().dropped,
            }};
        }

    private:
        void wire_stack_sinks() noexcept {
            if (stack_ == nullptr) {
                return;
            }
            ipv4_.set_icmp_sink(make_ipv4_packet_sink_ref(icmp_));
            stack_->set_arp_sink(make_owned_packet_sink_ref(arp_));
            stack_->set_ipv4_sink(make_owned_packet_sink_ref(ipv4_));
        }

        Stack* stack_{nullptr};
        NetIf* netif_{nullptr};
        ArpService<ArpCapacity, ArpTxCapacity> arp_{};
        Ipv4Service ipv4_{};
        IcmpEchoService icmp_{};
        IcmpEgressPump<TxCapacity,
                       ArpCapacity,
                       ArpTxCapacity,
                       PendingCapacity,
                       PayloadCapacity> egress_{};
        IcmpStackPumpConfig config_{};
    };

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

    struct UdpStackPumpConfig {
        UdpEgressPumpConfig egress{};
    };

    struct UdpStackPumpProgress {
        bool polled_links{false};
        util::usize ipv4_delivered{0};
        util::usize ipv4_dropped{0};
        util::usize udp_delivered{0};
        util::usize udp_dropped{0};
        util::usize arp_retried{0};
        util::usize arp_timed_out{0};
        util::usize egress_flushed{0};
        util::usize egress_dropped{0};
    };

    template <util::usize TxCapacity,
              util::usize ArpCapacity,
              util::usize ArpTxCapacity,
              util::usize PendingCapacity,
              util::usize PayloadCapacity,
              util::usize UdpBindingCapacity>
    class UdpStackPump {
    public:
        UdpStackPump() noexcept = default;

        explicit UdpStackPump(Stack& stack,
                              NetIf& netif,
                              UdpStackPumpConfig config = {}) noexcept
            : stack_(&stack)
            , netif_(&netif)
            , arp_(netif)
            , ipv4_(netif)
            , egress_(stack, netif, arp_, config.egress)
            , config_(config) {
            wire_stack_sinks();
        }

        void bind(Stack& stack, NetIf& netif) noexcept {
            stack_ = &stack;
            netif_ = &netif;
            arp_.bind(netif);
            ipv4_.bind(netif);
            egress_.bind(stack, netif, arp_);
            wire_stack_sinks();
        }

        void configure(UdpStackPumpConfig config) noexcept {
            config_ = config;
            egress_.configure(config.egress);
        }

        [[nodiscard]] const UdpStackPumpConfig& config() const noexcept {
            return config_;
        }

        [[nodiscard]] bool bound() const noexcept {
            return stack_ != nullptr
                && netif_ != nullptr
                && egress_.bound();
        }

        [[nodiscard]] Stack* stack() const noexcept {
            return stack_;
        }

        [[nodiscard]] NetIf* netif() const noexcept {
            return netif_;
        }

        [[nodiscard]] ArpService<ArpCapacity, ArpTxCapacity>& arp() noexcept {
            return arp_;
        }

        [[nodiscard]] const ArpService<ArpCapacity, ArpTxCapacity>& arp() const noexcept {
            return arp_;
        }

        [[nodiscard]] Ipv4Service& ipv4() noexcept {
            return ipv4_;
        }

        [[nodiscard]] const Ipv4Service& ipv4() const noexcept {
            return ipv4_;
        }

        [[nodiscard]] UdpService<UdpBindingCapacity>& udp() noexcept {
            return udp_;
        }

        [[nodiscard]] const UdpService<UdpBindingCapacity>& udp() const noexcept {
            return udp_;
        }

        [[nodiscard]] UdpEgressPump<TxCapacity,
                                    ArpCapacity,
                                    ArpTxCapacity,
                                    PendingCapacity,
                                    PayloadCapacity>& egress() noexcept {
            return egress_;
        }

        [[nodiscard]] const UdpEgressPump<TxCapacity,
                                          ArpCapacity,
                                          ArpTxCapacity,
                                          PendingCapacity,
                                          PayloadCapacity>& egress() const noexcept {
            return egress_;
        }

        [[nodiscard]] Result<void> bind_udp(util::u16 local_port, UdpDatagramSinkRef sink) noexcept {
            return udp_.bind(local_port, sink);
        }

        template <UdpDatagramSink T>
        [[nodiscard]] Result<void> bind_udp(util::u16 local_port, T& sink) noexcept {
            return udp_.bind(local_port, sink);
        }

        [[nodiscard]] bool has_udp_binding(util::u16 local_port) const noexcept {
            return udp_.has_binding(local_port);
        }

        [[nodiscard]] bool unbind_udp(util::u16 local_port) noexcept {
            return udp_.unbind(local_port);
        }

        [[nodiscard]] util::usize udp_binding_count() const noexcept {
            return udp_.binding_count();
        }

        [[nodiscard]] util::usize pending_count() const noexcept {
            return egress_.pending_count();
        }

        [[nodiscard]] util::usize queued_count() const noexcept {
            return egress_.queued_count();
        }

        [[nodiscard]] util::usize flushed_count() const noexcept {
            return egress_.flushed_count();
        }

        [[nodiscard]] util::usize dropped_count() const noexcept {
            return egress_.dropped_count();
        }

        [[nodiscard]] Result<UdpSendDisposition> send(Endpoint local,
                                                      const Endpoint& peer,
                                                      ByteView payload,
                                                      util::u8 ttl = 64,
                                                      util::u16 identification = 0,
                                                      util::u8 dscp_ecn = 0) noexcept {
            return egress_.send(local, peer, payload, ttl, identification, dscp_ecn);
        }

        [[nodiscard]] Result<UdpStackPumpProgress> service(util::usize elapsed_ticks = 1) noexcept {
            if (!bound()) {
                return util::unexpected(errc::bad_state);
            }

            const auto ipv4_packets_before = ipv4_.packet_count();
            const auto ipv4_drops_before = ipv4_.drop_count();
            const auto udp_packets_before = udp_.packet_count();
            const auto udp_drops_before = udp_.drop_count();

            auto progress = egress_.service(elapsed_ticks);
            if (!progress) {
                return util::unexpected(progress.error());
            }

            return Result<UdpStackPumpProgress>{std::in_place, UdpStackPumpProgress{
                .polled_links = progress.value().polled_links,
                .ipv4_delivered = ipv4_.packet_count() - ipv4_packets_before,
                .ipv4_dropped = ipv4_.drop_count() - ipv4_drops_before,
                .udp_delivered = udp_.packet_count() - udp_packets_before,
                .udp_dropped = udp_.drop_count() - udp_drops_before,
                .arp_retried = progress.value().arp_retried,
                .arp_timed_out = progress.value().arp_timed_out,
                .egress_flushed = progress.value().flushed,
                .egress_dropped = progress.value().dropped,
            }};
        }

    private:
        void wire_stack_sinks() noexcept {
            if (stack_ == nullptr) {
                return;
            }
            ipv4_.set_icmp_sink(make_ipv4_packet_sink_ref(icmp_));
            ipv4_.set_udp_sink(make_ipv4_packet_sink_ref(udp_));
            stack_->set_arp_sink(make_owned_packet_sink_ref(arp_));
            stack_->set_ipv4_sink(make_owned_packet_sink_ref(ipv4_));
        }

        Stack* stack_{nullptr};
        NetIf* netif_{nullptr};
        ArpService<ArpCapacity, ArpTxCapacity> arp_{};
        Ipv4Service ipv4_{};
        IcmpEchoService icmp_{};
        UdpService<UdpBindingCapacity> udp_{};
        UdpEgressPump<TxCapacity,
                      ArpCapacity,
                      ArpTxCapacity,
                      PendingCapacity,
                      PayloadCapacity> egress_{};
        UdpStackPumpConfig config_{};
    };
}
