module;

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

        [[nodiscard]] Result<IcmpSendDisposition> ping(util::u16 identifier,
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

        [[nodiscard]] Result<void> consume(const IcmpEchoInfo& info, OwnedPacket packet) noexcept {
            if (info.type != IcmpType::echo_reply) {
                ++drop_count_;
                return {};
            }
            if (configured_ && !accepts(info)) {
                ++drop_count_;
                return {};
            }

            ++reply_count_;
            last_error_ = errc::ok;
            if (reply_fn_ == nullptr) {
                return {};
            }
            return reply_fn_(reply_ctx_, info, packet.view());
        }

    private:
        [[nodiscard]] bool accepts(const IcmpEchoInfo& info) const noexcept {
            const auto local_ok = local_.is_unspecified() || local_.is_any()
                || detail::same_ipv4(local_, info.local);
            const auto peer_ok = peer_.is_unspecified() || peer_.is_any()
                || detail::same_ipv4(peer_, info.peer);
            return local_ok && peer_ok;
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
        ErrorFn error_fn_{nullptr};
        void* error_ctx_{nullptr};
        util::usize request_count_{0};
        util::usize reply_count_{0};
        util::usize drop_count_{0};
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
