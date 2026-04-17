module;

#include <array>

export module net.protocol.echo_icmp;

export import net.icmp_protocol_binding;
export import net.packet;
import util.core;
import util.error;
import util.expected;

export namespace net::icmp::echo {
    using SendFn = net::IcmpEchoSendFn;
    using ErrorFn = void (*)(void*, errc) noexcept;
    using ReplyFn = Result<void> (*)(void*, const IcmpEchoInfo&, PacketView) noexcept;
    using RequestFn = Result<void> (*)(void*, const IcmpEchoInfo&, PacketView) noexcept;
    using TimeoutFn = void (*)(void*, const IcmpEchoInfo&) noexcept;

    struct PingTicket {
        IcmpEchoInfo info{};
        IcmpSendDisposition disposition{IcmpSendDisposition::queued};
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

    class Client {
    public:
        Client() noexcept = default;

        constexpr Client(IpAddress local, IpAddress peer) noexcept
            : local_(local),
              peer_(peer),
              configured_(true) {}

        void set_sender(SendFn fn, void* ctx) noexcept {
            sender_ = fn;
            sender_ctx_ = ctx;
        }

        void set_reply_handler(ReplyFn fn, void* ctx = nullptr) noexcept {
            reply_fn_ = fn;
            reply_ctx_ = ctx;
        }

        void set_timeout_handler(TimeoutFn fn, void* ctx = nullptr) noexcept {
            timeout_fn_ = fn;
            timeout_ctx_ = ctx;
        }

        void set_error_handler(ErrorFn fn, void* ctx = nullptr) noexcept {
            error_fn_ = fn;
            error_ctx_ = ctx;
        }

        void configure(IpAddress local, IpAddress peer) noexcept {
            local_ = local;
            peer_ = peer;
            configured_ = true;
        }

        void reset() noexcept {
            request_count_ = 0;
            reply_count_ = 0;
            drop_count_ = 0;
            timeout_count_ = 0;
            transmitted_count_ = 0;
            queued_count_ = 0;
            clear_pending();
            clear_ignored();
            next_identifier_ = 1u;
            next_sequence_ = 1u;
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

        [[nodiscard]] util::usize request_count() const noexcept {
            return request_count_;
        }

        [[nodiscard]] util::usize reply_count() const noexcept {
            return reply_count_;
        }

        [[nodiscard]] util::usize drop_count() const noexcept {
            return drop_count_;
        }

        [[nodiscard]] util::usize timeout_count() const noexcept {
            return timeout_count_;
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

        [[nodiscard]] bool has_pending() const noexcept {
            for (const auto& pending : pending_) {
                if (pending.used) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] util::usize pending_count() const noexcept {
            util::usize count = 0;
            for (const auto& pending : pending_) {
                if (pending.used) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] Result<IcmpSendDisposition> ping(util::u16 identifier,
                                                       util::u16 sequence,
                                                       ByteView payload) noexcept {
            return send_echo(identifier, sequence, payload);
        }

        [[nodiscard]] Result<IcmpSendDisposition> ping(util::u16 identifier,
                                                       util::u16 sequence,
                                                       ByteView payload,
                                                       util::u32 now_ms,
                                                       util::u32 timeout_ms) noexcept {
            if (timeout_ms == 0u) {
                report_error(errc::invalid_arg);
                return util::unexpected(errc::invalid_arg);
            }

            IcmpEchoInfo info{
                .type = IcmpType::echo_request,
                .local = local_,
                .peer = peer_,
                .identifier = identifier,
                .sequence = sequence,
            };
            if (find_pending(info) != nullptr || consume_ignored(info, false)) {
                report_error(errc::busy);
                return util::unexpected(errc::busy);
            }

            auto* pending = allocate_pending();
            if (pending == nullptr) {
                report_error(errc::busy);
                return util::unexpected(errc::busy);
            }

            auto sent = send_echo(identifier, sequence, payload);
            if (!sent) {
                *pending = {};
                return util::unexpected(sent.error());
            }

            pending->info = info;
            pending->start_ms = now_ms;
            pending->timeout_ms = timeout_ms;
            return sent;
        }

        [[nodiscard]] Result<PingTicket> ping(ByteView payload) noexcept {
            auto ticket = next_ticket();
            auto sent = send_echo(ticket.info.identifier, ticket.info.sequence, payload);
            if (!sent) {
                return util::unexpected(sent.error());
            }
            ticket.disposition = sent.value();
            advance_ticket_seed();
            return Result<PingTicket>{std::in_place, ticket};
        }

        [[nodiscard]] Result<PingTicket> ping(ByteView payload,
                                              util::u32 now_ms,
                                              util::u32 timeout_ms) noexcept {
            if (timeout_ms == 0u) {
                report_error(errc::invalid_arg);
                return util::unexpected(errc::invalid_arg);
            }

            auto ticket = next_ticket();
            auto sent = ping(ticket.info.identifier,
                             ticket.info.sequence,
                             payload,
                             now_ms,
                             timeout_ms);
            if (!sent) {
                return util::unexpected(sent.error());
            }

            ticket.disposition = sent.value();
            advance_ticket_seed();
            return Result<PingTicket>{std::in_place, ticket};
        }

        void tick(util::u32 now_ms) noexcept {
            for (auto& pending : pending_) {
                if (!pending.used) {
                    continue;
                }
                if ((now_ms - pending.start_ms) < pending.timeout_ms) {
                    continue;
                }

                const auto info = pending.info;
                pending = {};
                remember_ignored(info);
                ++timeout_count_;

                if (timeout_fn_ != nullptr) {
                    timeout_fn_(timeout_ctx_, info);
                    continue;
                }

                report_error(errc::timeout);
            }
        }

        [[nodiscard]] Result<void> consume(const IcmpEchoInfo& info, OwnedPacket packet) noexcept {
            if (info.type != IcmpType::echo_reply) {
                ++drop_count_;
                return {};
            }
            if (configured_ && !accepts(info)) {
                ++drop_count_;
                return {};
            }
            if (consume_ignored(info)) {
                ++drop_count_;
                return {};
            }

            auto* pending = find_pending(info);
            if (pending != nullptr) {
                *pending = {};
            }

            ++reply_count_;
            last_error_ = errc::ok;
            if (reply_fn_ == nullptr) {
                return {};
            }
            return reply_fn_(reply_ctx_, info, packet.view());
        }

    private:
        static constexpr util::usize kMaxPending = 4u;

        struct PendingEcho {
            bool used{false};
            IcmpEchoInfo info{};
            util::u32 start_ms{0};
            util::u32 timeout_ms{0};
        };

        struct IgnoredEcho {
            bool used{false};
            IcmpEchoInfo info{};
        };

        [[nodiscard]] bool accepts(const IcmpEchoInfo& info) const noexcept {
            const auto local_ok = detail::match_ipv4(local_, info.local);
            const auto peer_ok = detail::match_ipv4(peer_, info.peer);
            return local_ok && peer_ok;
        }

        [[nodiscard]] Result<IcmpSendDisposition> send_echo(util::u16 identifier,
                                                            util::u16 sequence,
                                                            ByteView payload) noexcept {
            if (!configured_ || sender_ == nullptr) {
                report_error(errc::bad_state);
                return util::unexpected(errc::bad_state);
            }

            auto sent = sender_(
                sender_ctx_,
                local_,
                peer_,
                IcmpType::echo_request,
                identifier,
                sequence,
                payload);
            if (!sent) {
                report_error(sent.error());
                return util::unexpected(sent.error());
            }

            ++request_count_;
            if (sent.value() == IcmpSendDisposition::transmitted) {
                ++transmitted_count_;
            } else {
                ++queued_count_;
            }
            last_error_ = errc::ok;
            return sent;
        }

        [[nodiscard]] PingTicket next_ticket() noexcept {
            PingTicket ticket{};
            ticket.info.type = IcmpType::echo_request;
            ticket.info.local = local_;
            ticket.info.peer = peer_;
            ticket.info.identifier = next_identifier_;
            ticket.info.sequence = next_sequence_;

            util::u32 attempts = 0;
            while ((find_pending(ticket.info) != nullptr || consume_ignored(ticket.info, false))
                   && attempts < 0x1'0000u) {
                advance_ticket(ticket.info.identifier, ticket.info.sequence);
                ++attempts;
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

        [[nodiscard]] PendingEcho* allocate_pending() noexcept {
            for (auto& pending : pending_) {
                if (pending.used) {
                    continue;
                }
                pending = {};
                pending.used = true;
                return &pending;
            }
            return nullptr;
        }

        [[nodiscard]] PendingEcho* find_pending(const IcmpEchoInfo& info) noexcept {
            for (auto& pending : pending_) {
                if (!pending.used) {
                    continue;
                }
                if (!detail::same_echo_key(pending.info, info)) {
                    continue;
                }
                return &pending;
            }
            return nullptr;
        }

        [[nodiscard]] const PendingEcho* find_pending(const IcmpEchoInfo& info) const noexcept {
            for (const auto& pending : pending_) {
                if (!pending.used) {
                    continue;
                }
                if (!detail::same_echo_key(pending.info, info)) {
                    continue;
                }
                return &pending;
            }
            return nullptr;
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

        void clear_pending() noexcept {
            for (auto& pending : pending_) {
                pending = {};
            }
        }

        void clear_ignored() noexcept {
            for (auto& ignored : ignored_) {
                ignored = {};
            }
            next_ignored_ = 0;
        }

        void report_error(errc error) noexcept {
            last_error_ = error;
            if (error_fn_ != nullptr) {
                error_fn_(error_ctx_, error);
            }
        }

        IpAddress local_{};
        IpAddress peer_{};
        bool configured_{false};
        SendFn sender_{nullptr};
        void* sender_ctx_{nullptr};
        ReplyFn reply_fn_{nullptr};
        void* reply_ctx_{nullptr};
        TimeoutFn timeout_fn_{nullptr};
        void* timeout_ctx_{nullptr};
        ErrorFn error_fn_{nullptr};
        void* error_ctx_{nullptr};
        std::array<PendingEcho, kMaxPending> pending_{};
        std::array<IgnoredEcho, kMaxPending> ignored_{};
        util::usize next_ignored_{0};
        util::u16 next_identifier_{1u};
        util::u16 next_sequence_{1u};
        util::usize request_count_{0};
        util::usize reply_count_{0};
        util::usize drop_count_{0};
        util::usize timeout_count_{0};
        util::usize transmitted_count_{0};
        util::usize queued_count_{0};
        errc last_error_{errc::ok};
    };

    class AutoReplyServer {
    public:
        void set_sender(SendFn fn, void* ctx) noexcept {
            sender_ = fn;
            sender_ctx_ = ctx;
        }

        void set_request_handler(RequestFn fn, void* ctx = nullptr) noexcept {
            request_fn_ = fn;
            request_ctx_ = ctx;
        }

        void set_error_handler(ErrorFn fn, void* ctx = nullptr) noexcept {
            error_fn_ = fn;
            error_ctx_ = ctx;
        }

        void reset() noexcept {
            request_count_ = 0;
            reply_count_ = 0;
            drop_count_ = 0;
            transmitted_count_ = 0;
            queued_count_ = 0;
            last_error_ = errc::ok;
        }

        [[nodiscard]] util::usize request_count() const noexcept {
            return request_count_;
        }

        [[nodiscard]] util::usize reply_count() const noexcept {
            return reply_count_;
        }

        [[nodiscard]] util::usize drop_count() const noexcept {
            return drop_count_;
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

        [[nodiscard]] Result<void> consume(const IcmpEchoInfo& info, OwnedPacket packet) noexcept {
            if (info.type != IcmpType::echo_request) {
                ++drop_count_;
                return {};
            }
            if (sender_ == nullptr) {
                report_error(errc::bad_state);
                return util::unexpected(errc::bad_state);
            }

            const auto payload = packet.view();
            if (request_fn_ != nullptr) {
                auto observed = request_fn_(request_ctx_, info, payload);
                if (!observed) {
                    report_error(observed.error());
                    return util::unexpected(observed.error());
                }
            }

            auto sent = sender_(
                sender_ctx_,
                info.local,
                info.peer,
                IcmpType::echo_reply,
                info.identifier,
                info.sequence,
                payload.payload);
            if (!sent) {
                report_error(sent.error());
                return util::unexpected(sent.error());
            }

            ++request_count_;
            ++reply_count_;
            if (sent.value() == IcmpSendDisposition::transmitted) {
                ++transmitted_count_;
            } else {
                ++queued_count_;
            }
            last_error_ = errc::ok;
            return {};
        }

    private:
        void report_error(errc error) noexcept {
            last_error_ = error;
            if (error_fn_ != nullptr) {
                error_fn_(error_ctx_, error);
            }
        }

        SendFn sender_{nullptr};
        void* sender_ctx_{nullptr};
        RequestFn request_fn_{nullptr};
        void* request_ctx_{nullptr};
        ErrorFn error_fn_{nullptr};
        void* error_ctx_{nullptr};
        util::usize request_count_{0};
        util::usize reply_count_{0};
        util::usize drop_count_{0};
        util::usize transmitted_count_{0};
        util::usize queued_count_{0};
        errc last_error_{errc::ok};
    };
}
