module;

#include <array>

export module net.protocol.diagnostic;

export import net.service_codec;
import util.core;
import util.error;

export namespace net::diag {
    struct PingRequest {
        util::u8 text[4]{};
    };

    struct PingReply {
        util::u8 text[4]{};
    };

    struct CounterValue {
        util::u16 value{0};
    };

    struct MetaRequest {
        util::u16 code{0};
        util::u8 flags{0};
        std::array<util::u8, 2> tag{};
    };

    struct MetaReply {
        util::u8 status{0};
        util::u16 reflected_code{0};
        std::array<util::u8, 2> tag{};
    };

    using PingOp = TrivialServiceOp<0x60u, PingRequest, PingReply>;

    using CountOp = WireServiceOp<
        0x61u,
        EmptyMessage,
        CounterValue,
        WireMembers<>,
        WireMembers<&CounterValue::value>>;

    using SlowCountOp = WireServiceOp<
        0x62u,
        CounterValue,
        CounterValue,
        WireMembers<&CounterValue::value>,
        WireMembers<&CounterValue::value>>;

    using MetaOp = WireServiceOp<
        0x63u,
        MetaRequest,
        MetaReply,
        WireMembers<
            &MetaRequest::code,
            &MetaRequest::flags,
            &MetaRequest::tag>,
        WireMembers<
            &MetaReply::status,
            &MetaReply::reflected_code,
            &MetaReply::tag>>;

    template <util::usize MaxPayload = 64,
              util::usize MaxPending = 4,
              util::usize MaxRoutes = 8,
              util::usize MaxDeferred = MaxPending>
    class Client {
    public:
        using Session = TypedServiceSession<MaxPayload, MaxPending, MaxRoutes, MaxDeferred>;
        using ErrorFn = typename Session::ErrorFn;
        using PingResponseFn = typename Session::template ResponseFn<PingOp>;
        using CountResponseFn = typename Session::template ResponseFn<CountOp>;
        using SlowCountResponseFn = typename Session::template ResponseFn<SlowCountOp>;
        using MetaResponseFn = typename Session::template ResponseFn<MetaOp>;
        using PingTimeoutFn = typename Session::template TimeoutFn<PingOp>;
        using CountTimeoutFn = typename Session::template TimeoutFn<CountOp>;
        using SlowCountTimeoutFn = typename Session::template TimeoutFn<SlowCountOp>;
        using MetaTimeoutFn = typename Session::template TimeoutFn<MetaOp>;

        void set_sender(FrameSendFn fn, void* ctx) noexcept {
            session_.set_sender(fn, ctx);
        }

        void set_error_handler(ErrorFn fn, void* ctx) noexcept {
            session_.set_error_handler(fn, ctx);
        }

        void reset() noexcept {
            session_.reset();
        }

        [[nodiscard]] constexpr util::usize payload_capacity() const noexcept {
            return session_.payload_capacity();
        }

        [[nodiscard]] bool has_pending() const noexcept {
            return session_.has_pending();
        }

        [[nodiscard]] util::usize pending_count() const noexcept {
            return session_.pending_count();
        }

        [[nodiscard]] errc last_error() const noexcept {
            return session_.last_error();
        }

        [[nodiscard]] Session& raw() noexcept {
            return session_;
        }

        [[nodiscard]] const Session& raw() const noexcept {
            return session_;
        }

        void feed(ByteView data) noexcept {
            session_.feed(data);
        }

        void notify_writable() noexcept {
            session_.notify_writable();
        }

        void tick(util::u32 now_ms) noexcept {
            session_.tick(now_ms);
        }

        void on_transport_closed() noexcept {
            session_.on_transport_closed();
        }

        void on_transport_error(errc error) noexcept {
            session_.on_transport_error(error);
        }

        [[nodiscard]] bool cancel_request(util::u16 request_id) noexcept {
            return session_.cancel_request(request_id);
        }

        [[nodiscard]] Result<util::u16> ping(const PingRequest& request,
                                             util::u32 now_ms,
                                             util::u32 timeout_ms,
                                             PingResponseFn on_response = nullptr,
                                             PingTimeoutFn on_timeout = nullptr,
                                             void* user = nullptr) noexcept {
            return session_.template send_request<PingOp>(
                request,
                now_ms,
                timeout_ms,
                on_response,
                on_timeout,
                user);
        }

        [[nodiscard]] Result<util::u16> query_count(util::u32 now_ms,
                                                    util::u32 timeout_ms,
                                                    CountResponseFn on_response = nullptr,
                                                    CountTimeoutFn on_timeout = nullptr,
                                                    void* user = nullptr) noexcept {
            return session_.template send_request<CountOp>(
                EmptyMessage{},
                now_ms,
                timeout_ms,
                on_response,
                on_timeout,
                user);
        }

        [[nodiscard]] Result<util::u16> query_slow_count(const CounterValue& request,
                                                         util::u32 now_ms,
                                                         util::u32 timeout_ms,
                                                         SlowCountResponseFn on_response = nullptr,
                                                         SlowCountTimeoutFn on_timeout = nullptr,
                                                         void* user = nullptr) noexcept {
            return session_.template send_request<SlowCountOp>(
                request,
                now_ms,
                timeout_ms,
                on_response,
                on_timeout,
                user);
        }

        [[nodiscard]] Result<util::u16> query_meta(const MetaRequest& request,
                                                   util::u32 now_ms,
                                                   util::u32 timeout_ms,
                                                   MetaResponseFn on_response = nullptr,
                                                   MetaTimeoutFn on_timeout = nullptr,
                                                   void* user = nullptr) noexcept {
            return session_.template send_request<MetaOp>(
                request,
                now_ms,
                timeout_ms,
                on_response,
                on_timeout,
                user);
        }

    private:
        Session session_{};
    };

    template <util::usize MaxPayload = 64,
              util::usize MaxPending = 4,
              util::usize MaxRoutes = 8,
              util::usize MaxDeferred = MaxPending>
    class Server {
    public:
        using Session = TypedServiceSession<MaxPayload, MaxPending, MaxRoutes, MaxDeferred>;
        using ErrorFn = typename Session::ErrorFn;
        using PingHandler = typename Session::template RouteFn<PingOp>;
        using CountHandler = typename Session::template RouteFn<CountOp>;
        using MetaHandler = typename Session::template RouteFn<MetaOp>;
        using SlowCountHandler = void (*)(void* ctx,
                                          Server& server,
                                          ServiceReplyToken token,
                                          const CounterValue& request) noexcept;

        void set_sender(FrameSendFn fn, void* ctx) noexcept {
            session_.set_sender(fn, ctx);
        }

        void set_error_handler(ErrorFn fn, void* ctx) noexcept {
            session_.set_error_handler(fn, ctx);
        }

        void reset() noexcept {
            session_.reset();
        }

        [[nodiscard]] constexpr util::usize payload_capacity() const noexcept {
            return session_.payload_capacity();
        }

        [[nodiscard]] bool has_pending() const noexcept {
            return session_.has_pending();
        }

        [[nodiscard]] util::usize pending_count() const noexcept {
            return session_.pending_count();
        }

        [[nodiscard]] bool has_deferred() const noexcept {
            return session_.has_deferred();
        }

        [[nodiscard]] util::usize deferred_count() const noexcept {
            return session_.deferred_count();
        }

        [[nodiscard]] errc last_error() const noexcept {
            return session_.last_error();
        }

        [[nodiscard]] Session& raw() noexcept {
            return session_;
        }

        [[nodiscard]] const Session& raw() const noexcept {
            return session_;
        }

        void feed(ByteView data) noexcept {
            session_.feed(data);
        }

        void notify_writable() noexcept {
            session_.notify_writable();
        }

        void tick(util::u32 now_ms) noexcept {
            session_.tick(now_ms);
        }

        void on_transport_closed() noexcept {
            session_.on_transport_closed();
        }

        void on_transport_error(errc error) noexcept {
            session_.on_transport_error(error);
        }

        [[nodiscard]] Result<void> on_ping(PingHandler fn,
                                           void* ctx = nullptr) noexcept {
            return session_.template set_route<PingOp>(fn, ctx);
        }

        [[nodiscard]] Result<void> on_count(CountHandler fn,
                                            void* ctx = nullptr) noexcept {
            return session_.template set_route<CountOp>(fn, ctx);
        }

        [[nodiscard]] Result<void> on_meta(MetaHandler fn,
                                           void* ctx = nullptr) noexcept {
            return session_.template set_route<MetaOp>(fn, ctx);
        }

        [[nodiscard]] Result<void> on_slow_count(SlowCountHandler fn,
                                                 void* ctx = nullptr) noexcept {
            if (!fn) {
                return util::unexpected(errc::invalid_arg);
            }

            auto* saved_fn = slow_count_handler_;
            void* saved_ctx = slow_count_ctx_;
            slow_count_handler_ = fn;
            slow_count_ctx_ = ctx;

            auto set = session_.template set_deferred_route<SlowCountOp>(
                &Server::on_slow_count_trampoline,
                this);
            if (!set) {
                slow_count_handler_ = saved_fn;
                slow_count_ctx_ = saved_ctx;
                return util::unexpected(set.error());
            }
            return {};
        }

        [[nodiscard]] bool clear_ping() noexcept {
            return session_.template clear_route<PingOp>();
        }

        [[nodiscard]] bool clear_count() noexcept {
            return session_.template clear_route<CountOp>();
        }

        [[nodiscard]] bool clear_meta() noexcept {
            return session_.template clear_route<MetaOp>();
        }

        [[nodiscard]] bool clear_slow_count() noexcept {
            slow_count_handler_ = nullptr;
            slow_count_ctx_ = nullptr;
            return session_.template clear_route<SlowCountOp>();
        }

        [[nodiscard]] bool has_ping() const noexcept {
            return session_.template has_route<PingOp>();
        }

        [[nodiscard]] bool has_count() const noexcept {
            return session_.template has_route<CountOp>();
        }

        [[nodiscard]] bool has_meta() const noexcept {
            return session_.template has_route<MetaOp>();
        }

        [[nodiscard]] bool has_slow_count() const noexcept {
            return session_.template has_route<SlowCountOp>();
        }

        [[nodiscard]] Result<void> reply_slow_count(
            ServiceReplyToken token,
            const CounterValue& response,
            ServiceStatus status = ServiceStatus::ok) noexcept {
            return session_.template send_deferred_response<SlowCountOp>(
                token,
                response,
                status);
        }

        [[nodiscard]] Result<void> reply_slow_count_error(
            ServiceReplyToken token,
            ServiceStatus status,
            const CounterValue& response) noexcept {
            return session_.template send_deferred_error<SlowCountOp>(
                token,
                status,
                response);
        }

    private:
        static void on_slow_count_trampoline(void* ctx,
                                             Session&,
                                             ServiceReplyToken token,
                                             const CounterValue& request) noexcept {
            auto* self = static_cast<Server*>(ctx);
            if (!self || !self->slow_count_handler_) {
                return;
            }
            self->slow_count_handler_(self->slow_count_ctx_, *self, token, request);
        }

        Session session_{};
        SlowCountHandler slow_count_handler_{nullptr};
        void* slow_count_ctx_{nullptr};
    };
}
