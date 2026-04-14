module;

#include <array>
#include <cstring>
#include <type_traits>

export module net.service_codec;

export import net.common;
export import net.frame_session;
export import net.schema_codec;
export import net.service_session;
import util.core;
import util.error;
import util.expected;

export namespace net {
    template <class Op>
    concept ServiceOperation = requires {
        typename Op::Request;
        typename Op::Response;
        typename Op::RequestCodec;
        typename Op::ResponseCodec;
        { Op::opcode };
        { Op::RequestCodec::max_size() };
        { Op::ResponseCodec::max_size() };
    };

    template <util::u8 Opcode,
              class RequestT,
              class ResponseT,
              class RequestCodecT,
              class ResponseCodecT>
    struct ServiceOp {
        static constexpr util::u8 opcode = Opcode;
        using Request = RequestT;
        using Response = ResponseT;
        using RequestCodec = RequestCodecT;
        using ResponseCodec = ResponseCodecT;
    };

    template <util::u8 Opcode,
              class RequestT,
              class ResponseT>
    using TrivialServiceOp = ServiceOp<Opcode,
                                       RequestT,
                                       ResponseT,
                                       TrivialCodec<RequestT>,
                                       TrivialCodec<ResponseT>>;

    template <util::u8 Opcode,
              class RequestT,
              class ResponseT,
              class RequestFields,
              class ResponseFields>
    using WireServiceOp = ServiceOp<Opcode,
                                    RequestT,
                                    ResponseT,
                                    SchemaFieldCodec<RequestT, RequestFields>,
                                    SchemaFieldCodec<ResponseT, ResponseFields>>;

    template <util::usize MaxPayload,
              util::usize MaxPending = 4,
              util::usize MaxRoutes = 8,
              util::usize MaxDeferred = MaxPending>
    class TypedServiceSession {
    public:
        using Service = ServiceSession<MaxPayload, MaxPending, MaxRoutes, MaxDeferred>;
        using ErrorFn = void (*)(void* ctx, errc error) noexcept;

        template <ServiceOperation Op>
        using ResponseFn = void (*)(void* ctx,
                                    util::u16 request_id,
                                    ServiceStatus status,
                                    const typename Op::Response& response) noexcept;

        template <ServiceOperation Op>
        using TimeoutFn = void (*)(void* ctx,
                                   util::u16 request_id) noexcept;

        template <ServiceOperation Op>
        using RouteFn = ServiceStatus (*)(void* ctx,
                                          const typename Op::Request& request,
                                          typename Op::Response& response) noexcept;

        template <ServiceOperation Op>
        using DeferredRouteFn = void (*)(void* ctx,
                                         TypedServiceSession& session,
                                         ServiceReplyToken token,
                                         const typename Op::Request& request) noexcept;

        TypedServiceSession() {
            service_.set_error_handler(&TypedServiceSession::on_service_error_trampoline, this);
        }

        void set_sender(FrameSendFn fn, void* ctx) noexcept {
            service_.set_sender(fn, ctx);
        }

        void set_error_handler(ErrorFn fn, void* ctx) noexcept {
            error_ = fn;
            error_ctx_ = ctx;
        }

        void reset() noexcept {
            service_.reset();
            clear_pending();
            last_error_ = errc::ok;
        }

        [[nodiscard]] constexpr util::usize payload_capacity() const noexcept {
            return MaxPayload;
        }

        [[nodiscard]] bool has_pending() const noexcept {
            return pending_count() != 0;
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

        [[nodiscard]] bool has_deferred() const noexcept {
            return service_.has_deferred();
        }

        [[nodiscard]] util::usize deferred_count() const noexcept {
            return service_.deferred_count();
        }

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
        }

        [[nodiscard]] Service& raw() noexcept {
            return service_;
        }

        [[nodiscard]] const Service& raw() const noexcept {
            return service_;
        }

        template <ServiceOperation Op>
        [[nodiscard]] Result<void> set_route(RouteFn<Op> fn,
                                             void* ctx = nullptr) noexcept {
            static_assert(Op::RequestCodec::max_size() <= MaxPayload);
            static_assert(Op::ResponseCodec::max_size() <= MaxPayload);

            if (!fn) {
                return util::unexpected(errc::invalid_arg);
            }

            auto* binding = ensure_binding(Op::opcode);
            if (!binding) {
                return util::unexpected(errc::busy);
            }

            const auto saved = *binding;
            binding->used = true;
            binding->opcode = Op::opcode;
            binding->owner = this;
            binding->user = ctx;
            store_callback(binding->callback, fn);

            auto set = service_.set_route(Op::opcode,
                                          &TypedServiceSession::sync_route_trampoline<Op>,
                                          binding);
            if (!set) {
                *binding = saved;
                return util::unexpected(set.error());
            }
            return {};
        }

        template <ServiceOperation Op>
        [[nodiscard]] Result<void> set_deferred_route(DeferredRouteFn<Op> fn,
                                                      void* ctx = nullptr) noexcept {
            static_assert(Op::RequestCodec::max_size() <= MaxPayload);
            static_assert(Op::ResponseCodec::max_size() <= MaxPayload);

            if (!fn) {
                return util::unexpected(errc::invalid_arg);
            }

            auto* binding = ensure_binding(Op::opcode);
            if (!binding) {
                return util::unexpected(errc::busy);
            }

            const auto saved = *binding;
            binding->used = true;
            binding->opcode = Op::opcode;
            binding->owner = this;
            binding->user = ctx;
            store_callback(binding->callback, fn);

            auto set = service_.set_deferred_route(
                Op::opcode,
                &TypedServiceSession::deferred_route_trampoline<Op>,
                binding);
            if (!set) {
                *binding = saved;
                return util::unexpected(set.error());
            }
            return {};
        }

        template <ServiceOperation Op>
        [[nodiscard]] bool clear_route() noexcept {
            clear_binding(Op::opcode);
            return service_.clear_route(Op::opcode);
        }

        template <ServiceOperation Op>
        [[nodiscard]] bool has_route() const noexcept {
            return service_.has_route(Op::opcode);
        }

        void feed(ByteView data) noexcept {
            service_.feed(data);
        }

        void notify_writable() noexcept {
            service_.notify_writable();
        }

        template <ServiceOperation Op>
        [[nodiscard]] Result<util::u16> send_request(const typename Op::Request& request,
                                                     util::u32 now_ms,
                                                     util::u32 timeout_ms,
                                                     ResponseFn<Op> on_response = nullptr,
                                                     TimeoutFn<Op> on_timeout = nullptr,
                                                     void* user = nullptr) noexcept {
            static_assert(Op::RequestCodec::max_size() <= MaxPayload);
            static_assert(Op::ResponseCodec::max_size() <= MaxPayload);

            auto* pending = allocate_pending();
            if (!pending) {
                return util::unexpected(errc::busy);
            }

            std::array<util::u8, MaxPayload> wire{};
            auto encoded = encode_value<typename Op::RequestCodec>(
                request,
                MutByteView{wire.data(), wire.size()});
            if (!encoded) {
                *pending = {};
                return util::unexpected(encoded.error());
            }

            pending->used = true;
            pending->owner = this;
            pending->request_id = 0;
            pending->user = user;
            pending->on_response = &TypedServiceSession::dispatch_response<Op>;
            pending->on_timeout = &TypedServiceSession::dispatch_timeout<Op>;
            store_callback(pending->response_callback, on_response);
            store_callback(pending->timeout_callback, on_timeout);

            auto sent = service_.send_request(
                Op::opcode,
                ByteView{wire.data(), encoded.value()},
                now_ms,
                timeout_ms,
                &TypedServiceSession::on_service_response_trampoline,
                &TypedServiceSession::on_service_timeout_trampoline,
                pending);
            if (!sent) {
                *pending = {};
                return util::unexpected(sent.error());
            }

            pending->request_id = sent.value();
            return sent.value();
        }

        [[nodiscard]] bool cancel_request(util::u16 request_id) noexcept {
            if (!service_.cancel_request(request_id)) {
                return false;
            }

            for (auto& pending : pending_) {
                if (!pending.used || pending.request_id != request_id) continue;
                pending = {};
                break;
            }
            return true;
        }

        template <ServiceOperation Op>
        [[nodiscard]] Result<void> send_response(util::u16 request_id,
                                                 ServiceStatus status,
                                                 const typename Op::Response& response) noexcept {
            static_assert(Op::ResponseCodec::max_size() <= MaxPayload);

            std::array<util::u8, MaxPayload> wire{};
            auto encoded = encode_value<typename Op::ResponseCodec>(
                response,
                MutByteView{wire.data(), wire.size()});
            if (!encoded) {
                return util::unexpected(encoded.error());
            }

            return service_.send_response(
                request_id,
                Op::opcode,
                status,
                ByteView{wire.data(), encoded.value()});
        }

        template <ServiceOperation Op>
        [[nodiscard]] Result<void> send_error(util::u16 request_id,
                                              ServiceStatus status,
                                              const typename Op::Response& response) noexcept {
            if (service_status_ok(status)) {
                return util::unexpected(errc::invalid_arg);
            }
            return send_response<Op>(request_id, status, response);
        }

        template <ServiceOperation Op>
        [[nodiscard]] Result<void> send_deferred_response(
            ServiceReplyToken token,
            const typename Op::Response& response,
            ServiceStatus status = ServiceStatus::ok) noexcept {
            static_assert(Op::ResponseCodec::max_size() <= MaxPayload);

            if (token.opcode != Op::opcode) {
                return util::unexpected(errc::invalid_arg);
            }

            std::array<util::u8, MaxPayload> wire{};
            auto encoded = encode_value<typename Op::ResponseCodec>(
                response,
                MutByteView{wire.data(), wire.size()});
            if (!encoded) {
                return util::unexpected(encoded.error());
            }

            return service_.send_deferred_response(
                token,
                status,
                ByteView{wire.data(), encoded.value()});
        }

        template <ServiceOperation Op>
        [[nodiscard]] Result<void> send_deferred_error(
            ServiceReplyToken token,
            ServiceStatus status,
            const typename Op::Response& response) noexcept {
            if (service_status_ok(status)) {
                return util::unexpected(errc::invalid_arg);
            }
            return send_deferred_response<Op>(token, response, status);
        }

        [[nodiscard]] bool cancel_deferred(ServiceReplyToken token) noexcept {
            return service_.cancel_deferred(token);
        }

        void tick(util::u32 now_ms) noexcept {
            service_.tick(now_ms);
        }

        void on_transport_closed() noexcept {
            clear_pending();
            service_.on_transport_closed();
        }

        void on_transport_error(errc error) noexcept {
            clear_pending();
            service_.on_transport_error(error);
        }

    private:
        struct CallbackStorage {
            std::array<util::u8, sizeof(void (*)())> bytes{};
        };

        struct RouteBinding {
            bool used{false};
            util::u8 opcode{0};
            TypedServiceSession* owner{nullptr};
            void* user{nullptr};
            CallbackStorage callback{};
        };

        struct PendingCall {
            bool used{false};
            TypedServiceSession* owner{nullptr};
            util::u16 request_id{0};
            void* user{nullptr};
            void (*on_response)(PendingCall& pending,
                                util::u16 request_id,
                                ServiceStatus status,
                                ByteView payload) noexcept{nullptr};
            void (*on_timeout)(PendingCall& pending,
                               util::u16 request_id) noexcept{nullptr};
            CallbackStorage response_callback{};
            CallbackStorage timeout_callback{};
        };

        template <class Fn>
        static void store_callback(CallbackStorage& slot, Fn fn) noexcept {
            static_assert(std::is_trivially_copyable_v<Fn>);
            static_assert(sizeof(Fn) <= sizeof(slot.bytes));

            slot.bytes = {};
            if constexpr (sizeof(Fn) != 0) {
                std::memcpy(slot.bytes.data(), &fn, sizeof(Fn));
            }
        }

        template <class Fn>
        [[nodiscard]] static Fn load_callback(const CallbackStorage& slot) noexcept {
            static_assert(std::is_trivially_copyable_v<Fn>);
            static_assert(sizeof(Fn) <= sizeof(slot.bytes));

            Fn fn{};
            if constexpr (sizeof(Fn) != 0) {
                std::memcpy(&fn, slot.bytes.data(), sizeof(Fn));
            }
            return fn;
        }

        template <class Codec, class Value>
        [[nodiscard]] static Result<util::usize> encode_value(const Value& value,
                                                              MutByteView payload) noexcept {
            auto encoded = Codec::encode(value, payload);
            if (!encoded) {
                return util::unexpected(encoded.error());
            }
            if (encoded.value() > payload.size()) {
                return util::unexpected(errc::format_error);
            }
            return encoded.value();
        }

        template <class Codec, class Value>
        [[nodiscard]] static Result<Value> decode_value(ByteView payload) noexcept {
            return Codec::decode(payload);
        }

        [[nodiscard]] PendingCall* allocate_pending() noexcept {
            for (auto& pending : pending_) {
                if (pending.used) continue;
                pending = {};
                return &pending;
            }
            return nullptr;
        }

        [[nodiscard]] RouteBinding* ensure_binding(util::u8 opcode) noexcept {
            for (auto& binding : bindings_) {
                if (binding.used && binding.opcode == opcode) {
                    return &binding;
                }
            }

            for (auto& binding : bindings_) {
                if (binding.used) continue;
                return &binding;
            }
            return nullptr;
        }

        void clear_binding(util::u8 opcode) noexcept {
            for (auto& binding : bindings_) {
                if (!binding.used || binding.opcode != opcode) continue;
                binding = {};
                return;
            }
        }

        template <ServiceOperation Op>
        static ServiceStatus sync_route_trampoline(void* ctx,
                                                   ByteView request,
                                                   MutByteView response,
                                                   util::usize* response_size) noexcept {
            auto* binding = static_cast<RouteBinding*>(ctx);
            if (!binding || !binding->owner || !response_size) {
                return ServiceStatus::internal_error;
            }

            auto* owner = binding->owner;
            auto handler = load_callback<RouteFn<Op>>(binding->callback);
            if (!handler) {
                owner->notify_error(errc::bad_state);
                return ServiceStatus::internal_error;
            }

            auto decoded = decode_value<typename Op::RequestCodec, typename Op::Request>(request);
            if (!decoded) {
                owner->notify_error(decoded.error());
                *response_size = 0;
                return ServiceStatus::bad_request;
            }

            typename Op::Response typed_response{};
            const auto status = handler(binding->user, decoded.value(), typed_response);

            auto encoded = encode_value<typename Op::ResponseCodec>(
                typed_response,
                response);
            if (!encoded) {
                owner->notify_error(encoded.error());
                *response_size = 0;
                return ServiceStatus::internal_error;
            }

            *response_size = encoded.value();
            return status;
        }

        template <ServiceOperation Op>
        static void deferred_route_trampoline(void* ctx,
                                              Service&,
                                              ServiceReplyToken token,
                                              ByteView request) noexcept {
            auto* binding = static_cast<RouteBinding*>(ctx);
            if (!binding || !binding->owner) {
                return;
            }

            auto* owner = binding->owner;
            auto handler = load_callback<DeferredRouteFn<Op>>(binding->callback);
            if (!handler) {
                owner->notify_error(errc::bad_state);
                (void)owner->raw().send_deferred_error(token, ServiceStatus::internal_error);
                return;
            }

            auto decoded = decode_value<typename Op::RequestCodec, typename Op::Request>(request);
            if (!decoded) {
                owner->notify_error(decoded.error());
                (void)owner->raw().send_deferred_error(token, ServiceStatus::bad_request);
                return;
            }

            handler(binding->user, *owner, token, decoded.value());
        }

        static void on_service_error_trampoline(void* ctx, errc error) noexcept {
            auto* self = static_cast<TypedServiceSession*>(ctx);
            if (self) {
                self->notify_error(error);
            }
        }

        static void on_service_response_trampoline(void* ctx,
                                                   util::u16 request_id,
                                                   util::u8 opcode,
                                                   ServiceStatus status,
                                                   ByteView payload) noexcept {
            auto* pending = static_cast<PendingCall*>(ctx);
            if (!pending || !pending->owner || !pending->on_response) {
                return;
            }
            (void)opcode;
            pending->on_response(*pending, request_id, status, payload);
        }

        static void on_service_timeout_trampoline(void* ctx,
                                                  util::u16 request_id,
                                                  util::u8 opcode) noexcept {
            auto* pending = static_cast<PendingCall*>(ctx);
            if (!pending || !pending->owner || !pending->on_timeout) {
                return;
            }
            (void)opcode;
            pending->on_timeout(*pending, request_id);
        }

        template <ServiceOperation Op>
        static void dispatch_response(PendingCall& pending,
                                      util::u16 request_id,
                                      ServiceStatus status,
                                      ByteView payload) noexcept {
            auto* owner = pending.owner;
            const auto callback = load_callback<ResponseFn<Op>>(pending.response_callback);
            void* user = pending.user;
            pending = {};

            auto decoded = decode_value<typename Op::ResponseCodec, typename Op::Response>(payload);
            if (!decoded) {
                if (owner) {
                    owner->notify_error(decoded.error());
                }
                return;
            }

            if (callback) {
                callback(user, request_id, status, decoded.value());
            }
        }

        template <ServiceOperation Op>
        static void dispatch_timeout(PendingCall& pending,
                                     util::u16 request_id) noexcept {
            auto* owner = pending.owner;
            const auto callback = load_callback<TimeoutFn<Op>>(pending.timeout_callback);
            void* user = pending.user;
            pending = {};

            if (callback) {
                callback(user, request_id);
                return;
            }

            if (owner) {
                owner->notify_error(errc::timeout);
            }
        }

        void clear_pending() noexcept {
            for (auto& pending : pending_) {
                pending = {};
            }
        }

        void notify_error(errc error) noexcept {
            last_error_ = error;
            if (error_) {
                error_(error_ctx_, error);
            }
        }

        Service service_{};
        std::array<RouteBinding, MaxRoutes> bindings_{};
        std::array<PendingCall, MaxPending> pending_{};
        ErrorFn error_{nullptr};
        void* error_ctx_{nullptr};
        errc last_error_{errc::ok};
    };
}
