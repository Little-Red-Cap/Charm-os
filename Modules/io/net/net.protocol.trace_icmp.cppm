module;

#include <array>

export module net.protocol.trace_icmp;

export import net.icmp_protocol_binding;
export import net.packet;
import util.core;
import util.error;
import util.expected;

export namespace net::icmp::trace {
    using SendFn = net::IcmpControlSendFn;
    using ErrorFn = NetErrorFn;

    struct ProbeTicket {
        IcmpEchoInfo info{};
        util::u8 ttl{64};
        IcmpSendDisposition disposition{IcmpSendDisposition::queued};
    };

    enum class ProbeState : util::u8 {
        idle = 0u,
        pending = 1u,
        hop = 2u,
        reached = 3u,
        unreachable = 4u,
        timed_out = 5u,
        cancelled = 6u,
        error = 7u,
    };

    struct ProbeSnapshot {
        ProbeState state{ProbeState::idle};
        errc error{errc::ok};
        IcmpEchoInfo info{};
        IpAddress responder{};
        IcmpType response_type{IcmpType::time_exceeded};
        util::u8 response_code{0};
        util::u8 ttl{0};
        ByteView payload{};

        [[nodiscard]] constexpr bool idle() const noexcept {
            return state == ProbeState::idle;
        }

        [[nodiscard]] constexpr bool pending() const noexcept {
            return state == ProbeState::pending;
        }

        [[nodiscard]] constexpr bool ready() const noexcept {
            return state != ProbeState::idle && state != ProbeState::pending;
        }

        [[nodiscard]] constexpr bool hop() const noexcept {
            return state == ProbeState::hop;
        }

        [[nodiscard]] constexpr bool reached() const noexcept {
            return state == ProbeState::reached;
        }

        [[nodiscard]] constexpr bool unreachable() const noexcept {
            return state == ProbeState::unreachable;
        }

        [[nodiscard]] constexpr bool responded() const noexcept {
            return hop() || reached() || unreachable();
        }

        [[nodiscard]] constexpr bool ok() const noexcept {
            return responded();
        }

        [[nodiscard]] constexpr bool timed_out() const noexcept {
            return state == ProbeState::timed_out;
        }

        [[nodiscard]] constexpr bool cancelled() const noexcept {
            return state == ProbeState::cancelled;
        }

        [[nodiscard]] constexpr bool failed() const noexcept {
            return state == ProbeState::error;
        }

        [[nodiscard]] constexpr bool has_value() const noexcept {
            return reached();
        }

        [[nodiscard]] constexpr util::u16 identifier() const noexcept {
            return info.identifier;
        }

        [[nodiscard]] constexpr util::u16 sequence() const noexcept {
            return info.sequence;
        }

        [[nodiscard]] constexpr util::usize payload_size() const noexcept {
            return payload.size();
        }

        [[nodiscard]] constexpr bool has_payload() const noexcept {
            return payload.size() != 0u;
        }

        [[nodiscard]] ByteView value_payload() const noexcept {
            return has_value() ? payload : ByteView{};
        }
    };

    namespace detail {
        [[nodiscard]] constexpr bool same_ipv4(const IpAddress& lhs, const IpAddress& rhs) noexcept {
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

        [[nodiscard]] constexpr bool match_ipv4(const IpAddress& expected,
                                                const IpAddress& actual) noexcept {
            return expected.is_unspecified()
                || expected.is_any()
                || same_ipv4(expected, actual);
        }

        [[nodiscard]] constexpr bool same_echo_key(const IcmpEchoInfo& lhs,
                                                   const IcmpEchoInfo& rhs) noexcept {
            return lhs.identifier == rhs.identifier
                && lhs.sequence == rhs.sequence
                && match_ipv4(lhs.local, rhs.local)
                && match_ipv4(lhs.peer, rhs.peer);
        }
    }

    template <util::usize MaxPayload = 64>
    class Probe {
    public:
        Probe() noexcept = default;

        explicit constexpr Probe(IpAddress peer) noexcept
            : Probe(IpAddress::ipv4_any(), peer) {}

        constexpr Probe(IpAddress local, IpAddress peer) noexcept
            : local_(local),
              peer_(peer),
              configured_(true) {}

        Probe(const Probe&) = delete;
        Probe& operator=(const Probe&) = delete;
        Probe(Probe&&) = delete;
        Probe& operator=(Probe&&) = delete;

        void set_sender(IcmpControlSenderRef sender = {}) noexcept {
            sender_ = sender;
        }

        void set_sender(SendFn fn, void* ctx) noexcept {
            set_sender(IcmpControlSenderRef::raw(fn, ctx));
        }

        void set_error_handler(NetErrorHandlerRef handler = {}) noexcept {
            error_ = handler;
        }

        void set_error_handler(ErrorFn fn, void* ctx = nullptr) noexcept {
            set_error_handler(NetErrorHandlerRef::raw(fn, ctx));
        }

        void configure(IpAddress local, IpAddress peer) noexcept {
            local_ = local;
            peer_ = peer;
            configured_ = true;
        }

        void configure(IpAddress peer) noexcept {
            configure(IpAddress::ipv4_any(), peer);
        }

        void reset() noexcept {
            pending_ = false;
            clear_observation();
            clear_ignored();
            next_identifier_ = 1u;
            next_sequence_ = 1u;
            request_count_ = 0;
            response_count_ = 0;
            hop_count_ = 0;
            reach_count_ = 0;
            unreachable_count_ = 0;
            timeout_count_ = 0;
            drop_count_ = 0;
            error_count_ = 0;
            transmitted_count_ = 0;
            queued_count_ = 0;
            last_error_ = errc::ok;
        }

        [[nodiscard]] bool configured() const noexcept {
            return configured_;
        }

        [[nodiscard]] IpAddress local_address() const noexcept {
            return local_;
        }

        [[nodiscard]] IpAddress peer_address() const noexcept {
            return peer_;
        }

        [[nodiscard]] bool has_pending() const noexcept {
            return pending_;
        }

        [[nodiscard]] util::usize pending_count() const noexcept {
            return pending_ ? 1u : 0u;
        }

        [[nodiscard]] util::usize request_count() const noexcept {
            return request_count_;
        }

        [[nodiscard]] util::usize response_count() const noexcept {
            return response_count_;
        }

        [[nodiscard]] util::usize hop_count() const noexcept {
            return hop_count_;
        }

        [[nodiscard]] util::usize reach_count() const noexcept {
            return reach_count_;
        }

        [[nodiscard]] util::usize unreachable_count() const noexcept {
            return unreachable_count_;
        }

        [[nodiscard]] util::usize timeout_count() const noexcept {
            return timeout_count_;
        }

        [[nodiscard]] util::usize drop_count() const noexcept {
            return drop_count_;
        }

        [[nodiscard]] util::usize error_count() const noexcept {
            return error_count_;
        }

        [[nodiscard]] util::usize transmitted_count() const noexcept {
            return transmitted_count_;
        }

        [[nodiscard]] util::usize queued_count() const noexcept {
            return queued_count_;
        }

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
        }

        [[nodiscard]] errc observed_error() const noexcept {
            return observed_error_;
        }

        [[nodiscard]] bool idle() const noexcept {
            return state_ == ProbeState::idle;
        }

        [[nodiscard]] bool pending() const noexcept {
            return state_ == ProbeState::pending;
        }

        [[nodiscard]] bool ready() const noexcept {
            return state_ != ProbeState::idle && state_ != ProbeState::pending;
        }

        [[nodiscard]] bool hop() const noexcept {
            return state_ == ProbeState::hop;
        }

        [[nodiscard]] bool reached() const noexcept {
            return state_ == ProbeState::reached;
        }

        [[nodiscard]] bool unreachable() const noexcept {
            return state_ == ProbeState::unreachable;
        }

        [[nodiscard]] bool responded() const noexcept {
            return hop() || reached() || unreachable();
        }

        [[nodiscard]] bool ok() const noexcept {
            return responded();
        }

        [[nodiscard]] bool timed_out() const noexcept {
            return state_ == ProbeState::timed_out;
        }

        [[nodiscard]] bool cancelled() const noexcept {
            return state_ == ProbeState::cancelled;
        }

        [[nodiscard]] bool failed() const noexcept {
            return state_ == ProbeState::error;
        }

        [[nodiscard]] bool has_value() const noexcept {
            return reached();
        }

        [[nodiscard]] ProbeState state() const noexcept {
            return state_;
        }

        [[nodiscard]] util::u16 identifier() const noexcept {
            return current_info_.identifier;
        }

        [[nodiscard]] util::u16 sequence() const noexcept {
            return current_info_.sequence;
        }

        [[nodiscard]] util::u8 ttl() const noexcept {
            return current_ttl_;
        }

        [[nodiscard]] IpAddress responder() const noexcept {
            return responder_;
        }

        [[nodiscard]] IcmpType response_type() const noexcept {
            return response_type_;
        }

        [[nodiscard]] util::u8 response_code() const noexcept {
            return response_code_;
        }

        [[nodiscard]] ByteView last_reply_payload() const noexcept {
            return ByteView{reply_payload_.data(), reply_payload_size_};
        }

        [[nodiscard]] util::usize payload_size() const noexcept {
            return has_value() ? reply_payload_size_ : 0u;
        }

        [[nodiscard]] bool has_payload() const noexcept {
            return payload_size() != 0u;
        }

        [[nodiscard]] ByteView value_payload() const noexcept {
            return has_value() ? last_reply_payload() : ByteView{};
        }

        [[nodiscard]] ProbeSnapshot snapshot() const noexcept {
            return ProbeSnapshot{
                .state = state_,
                .error = observed_error_,
                .info = current_info_,
                .responder = responder_,
                .response_type = response_type_,
                .response_code = response_code_,
                .ttl = current_ttl_,
                .payload = state_ == ProbeState::reached ? last_reply_payload() : ByteView{},
            };
        }

        [[nodiscard]] ProbeSnapshot result() const noexcept {
            return snapshot();
        }

        template <class Pump>
        [[nodiscard]] Result<void> bind(Pump& pump) noexcept {
            if (!configured_) {
                return util::unexpected(errc::bad_state);
            }
            return net::bind_icmp_protocol(pump, *this);
        }

        template <class Pump>
        [[nodiscard]] Result<void> bind(Pump& pump,
                                        IpAddress local,
                                        IpAddress peer) noexcept {
            configure(local, peer);
            return bind(pump);
        }

        template <class Pump>
        [[nodiscard]] Result<void> bind(Pump& pump, IpAddress peer) noexcept {
            configure(peer);
            return bind(pump);
        }

        [[nodiscard]] Result<ProbeTicket> probe(util::u8 ttl,
                                                ByteView payload,
                                                util::u32 now_ms,
                                                util::u32 timeout_ms) noexcept {
            if (ttl == 0u || timeout_ms == 0u) {
                report_error(errc::invalid_arg);
                return util::unexpected(errc::invalid_arg);
            }
            if (!configured_ || !sender_) {
                report_error(errc::bad_state);
                return util::unexpected(errc::bad_state);
            }
            if (pending_) {
                report_error(errc::busy);
                return util::unexpected(errc::busy);
            }

            clear_observation();

            auto ticket = next_ticket();
            ticket.ttl = ttl;

            auto sent = sender_.send(
                local_,
                peer_,
                IcmpType::echo_request,
                ticket.info.identifier,
                ticket.info.sequence,
                payload,
                ttl,
                0u,
                0u);
            if (!sent) {
                report_error(sent.error());
                return util::unexpected(sent.error());
            }

            ticket.disposition = sent.value();
            advance_ticket_seed();

            pending_ = true;
            current_info_ = ticket.info;
            current_ttl_ = ttl;
            start_ms_ = now_ms;
            timeout_ms_ = timeout_ms;
            state_ = ProbeState::pending;
            observed_error_ = errc::ok;
            last_error_ = errc::ok;
            ++request_count_;
            if (sent.value() == IcmpSendDisposition::transmitted) {
                ++transmitted_count_;
            } else {
                ++queued_count_;
            }

            return Result<ProbeTicket>{std::in_place, ticket};
        }

        template <util::usize Size>
        [[nodiscard]] Result<ProbeTicket> probe(util::u8 ttl,
                                                const util::u8 (&payload)[Size],
                                                util::u32 now_ms,
                                                util::u32 timeout_ms) noexcept {
            return probe(ttl, ByteView{payload, Size}, now_ms, timeout_ms);
        }

        [[nodiscard]] Result<ProbeTicket> probe(util::u8 ttl,
                                                util::u32 now_ms,
                                                util::u32 timeout_ms) noexcept {
            return probe(ttl, ByteView{}, now_ms, timeout_ms);
        }

        [[nodiscard]] bool cancel() noexcept {
            if (!pending_) {
                return false;
            }

            const auto info = current_info_;
            remember_ignored(info);
            pending_ = false;
            clear_reply_payload();
            responder_ = {};
            response_type_ = IcmpType::time_exceeded;
            response_code_ = 0u;
            observed_error_ = errc::ok;
            state_ = ProbeState::cancelled;
            return true;
        }

        void tick(util::u32 now_ms) noexcept {
            if (!pending_) {
                return;
            }
            if ((now_ms - start_ms_) < timeout_ms_) {
                return;
            }

            const auto info = current_info_;
            remember_ignored(info);
            pending_ = false;
            clear_reply_payload();
            responder_ = {};
            response_type_ = IcmpType::time_exceeded;
            response_code_ = 0u;
            observed_error_ = errc::ok;
            state_ = ProbeState::timed_out;
            ++timeout_count_;
        }

        [[nodiscard]] Result<void> consume(const IcmpEchoInfo& info, OwnedPacket packet) noexcept {
            if (info.type != IcmpType::echo_reply) {
                ++drop_count_;
                return {};
            }

            if (pending_ && detail::same_echo_key(current_info_, info)) {
                if (packet.view().size() > MaxPayload) {
                    pending_ = false;
                    remember_ignored(current_info_);
                    clear_reply_payload();
                    responder_ = info.peer;
                    response_type_ = IcmpType::echo_reply;
                    response_code_ = 0u;
                    observed_error_ = errc::buffer_overflow;
                    state_ = ProbeState::error;
                    ++error_count_;
                    return util::unexpected(observed_error_);
                }

                for (util::usize index = 0; index < packet.view().size(); ++index) {
                    reply_payload_[index] = packet.view()[index];
                }
                reply_payload_size_ = packet.view().size();
                pending_ = false;
                remember_ignored(current_info_);
                current_info_ = info;
                responder_ = info.peer;
                response_type_ = IcmpType::echo_reply;
                response_code_ = 0u;
                observed_error_ = errc::ok;
                state_ = ProbeState::reached;
                ++response_count_;
                ++reach_count_;
                last_error_ = errc::ok;
                return {};
            }

            if (consume_ignored(info)) {
                ++drop_count_;
                return {};
            }

            ++drop_count_;
            return {};
        }

        [[nodiscard]] Result<void> consume(const IcmpErrorQuoteInfo& info, OwnedPacket packet) noexcept {
            static_cast<void>(packet);

            const auto quoted = parse_icmp_error_quote_echo_info(info);
            if (!quoted) {
                ++drop_count_;
                return {};
            }

            if (pending_ && detail::same_echo_key(current_info_, quoted.value())) {
                pending_ = false;
                remember_ignored(current_info_);
                clear_reply_payload();
                current_info_ = quoted.value();
                responder_ = info.peer;
                response_type_ = info.type;
                response_code_ = info.code;
                observed_error_ = errc::ok;

                if (info.type == IcmpType::time_exceeded) {
                    state_ = ProbeState::hop;
                    ++hop_count_;
                } else {
                    state_ = ProbeState::unreachable;
                    ++unreachable_count_;
                }

                ++response_count_;
                last_error_ = errc::ok;
                return {};
            }

            if (consume_ignored(quoted.value())) {
                ++drop_count_;
                return {};
            }

            ++drop_count_;
            return {};
        }

    private:
        static constexpr util::usize kMaxIgnored = 4u;

        struct IgnoredEcho {
            bool used{false};
            IcmpEchoInfo info{};
        };

        [[nodiscard]] ProbeTicket next_ticket() noexcept {
            ProbeTicket ticket{};
            ticket.info.type = IcmpType::echo_request;
            ticket.info.local = local_;
            ticket.info.peer = peer_;
            ticket.info.identifier = next_identifier_;
            ticket.info.sequence = next_sequence_;

            util::u32 attempts = 0;
            while ((pending_ && detail::same_echo_key(current_info_, ticket.info))
                   || consume_ignored(ticket.info, false)) {
                advance_ticket(ticket.info.identifier, ticket.info.sequence);
                ++attempts;
                if (attempts >= 0x1'0000u) {
                    break;
                }
            }

            return ticket;
        }

        void advance_ticket_seed() noexcept {
            advance_ticket(next_identifier_, next_sequence_);
        }

        static void advance_ticket(util::u16& identifier, util::u16& sequence) noexcept {
            ++sequence;
            if (sequence != 0u) {
                return;
            }

            sequence = 1u;
            ++identifier;
            if (identifier == 0u) {
                identifier = 1u;
            }
        }

        void remember_ignored(const IcmpEchoInfo& info) noexcept {
            ignored_[next_ignored_] = IgnoredEcho{true, info};
            ++next_ignored_;
            if (next_ignored_ >= ignored_.size()) {
                next_ignored_ = 0;
            }
        }

        [[nodiscard]] bool consume_ignored(const IcmpEchoInfo& info,
                                           bool clear = true) noexcept {
            for (auto& ignored : ignored_) {
                if (!ignored.used) {
                    continue;
                }
                if (!detail::same_echo_key(ignored.info, info)) {
                    continue;
                }
                if (clear) {
                    ignored = {};
                }
                return true;
            }
            return false;
        }

        void clear_ignored() noexcept {
            for (auto& ignored : ignored_) {
                ignored = {};
            }
            next_ignored_ = 0;
        }

        void clear_observation() noexcept {
            clear_reply_payload();
            current_info_ = {};
            responder_ = {};
            response_type_ = IcmpType::time_exceeded;
            response_code_ = 0u;
            current_ttl_ = 0u;
            start_ms_ = 0u;
            timeout_ms_ = 0u;
            observed_error_ = errc::ok;
            state_ = ProbeState::idle;
        }

        void clear_reply_payload() noexcept {
            reply_payload_size_ = 0u;
        }

        void report_error(errc error) noexcept {
            last_error_ = error;
            error_.notify(error);
        }

        IpAddress local_{};
        IpAddress peer_{};
        bool configured_{false};
        bool pending_{false};
        IcmpControlSenderRef sender_{};
        NetErrorHandlerRef error_{};
        std::array<IgnoredEcho, kMaxIgnored> ignored_{};
        util::usize next_ignored_{0};
        std::array<util::u8, MaxPayload> reply_payload_{};
        IcmpEchoInfo current_info_{};
        IpAddress responder_{};
        util::u32 start_ms_{0};
        util::u32 timeout_ms_{0};
        util::u16 next_identifier_{1u};
        util::u16 next_sequence_{1u};
        util::usize reply_payload_size_{0};
        util::usize request_count_{0};
        util::usize response_count_{0};
        util::usize hop_count_{0};
        util::usize reach_count_{0};
        util::usize unreachable_count_{0};
        util::usize timeout_count_{0};
        util::usize drop_count_{0};
        util::usize error_count_{0};
        util::usize transmitted_count_{0};
        util::usize queued_count_{0};
        util::u8 current_ttl_{0};
        util::u8 response_code_{0};
        IcmpType response_type_{IcmpType::time_exceeded};
        errc last_error_{errc::ok};
        errc observed_error_{errc::ok};
        ProbeState state_{ProbeState::idle};
    };
}
