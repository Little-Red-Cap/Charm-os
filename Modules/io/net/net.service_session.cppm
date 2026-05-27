module;

#include <array>
#include <concepts>

export module net.service_session;

import net.common;
import net.frame_session;
import net.request_session;
import util.core;
import util.error;
import util.expected;

export namespace net {
    enum class ServiceStatus : util::u8 {
        ok = 0,
        bad_request = 1,
        not_found = 2,
        not_supported = 3,
        busy = 4,
        internal_error = 5,
    };

    struct ServiceReplyToken {
        util::u16 request_id{0};
        util::u8 opcode{0};
        util::u16 slot{0};
        util::u16 generation{0};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return request_id != 0 && generation != 0;
        }
    };

    [[nodiscard]] constexpr bool service_status_ok(ServiceStatus status) noexcept {
        return status == ServiceStatus::ok;
    }

    using ServiceResponseFn = void (*)(void* ctx,
                                       util::u16 request_id,
                                       util::u8 opcode,
                                       ServiceStatus status,
                                       ByteView payload) noexcept;
    using ServiceTimeoutFn = void (*)(void* ctx,
                                      util::u16 request_id,
                                      util::u8 opcode) noexcept;
    using ServiceRouteFn = ServiceStatus (*)(void* ctx,
                                             ByteView request,
                                             MutByteView response,
                                             util::usize* response_size) noexcept;

    class ServiceResponseHandlerRef {
    public:
        constexpr ServiceResponseHandlerRef() noexcept = default;

        static constexpr ServiceResponseHandlerRef raw(ServiceResponseFn handler, void* ctx) noexcept {
            return ServiceResponseHandlerRef{handler, ctx};
        }

        template <typename Handler>
            requires(
                requires(Handler& value,
                         util::u16 request_id,
                         util::u8 opcode,
                         ServiceStatus status,
                         ByteView payload) {
                    { value.on_response(request_id, opcode, status, payload) } noexcept -> std::same_as<void>;
                } ||
                requires(Handler& value,
                         util::u16 request_id,
                         util::u8 opcode,
                         ServiceStatus status,
                         ByteView payload) {
                    { value(request_id, opcode, status, payload) } noexcept -> std::same_as<void>;
                })
        static constexpr ServiceResponseHandlerRef bind(Handler& handler) noexcept {
            return ServiceResponseHandlerRef{&invoke<Handler>, &handler};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return handler_ != nullptr;
        }

        void notify(util::u16 request_id,
                    util::u8 opcode,
                    ServiceStatus status,
                    ByteView payload) const noexcept {
            if (handler_) {
                handler_(ctx_, request_id, opcode, status, payload);
            }
        }

    private:
        constexpr ServiceResponseHandlerRef(ServiceResponseFn handler, void* ctx) noexcept
            : handler_(handler),
              ctx_(ctx) {
        }

        template <typename Handler>
        static void invoke(void* ctx,
                           util::u16 request_id,
                           util::u8 opcode,
                           ServiceStatus status,
                           ByteView payload) noexcept {
            auto* handler = static_cast<Handler*>(ctx);
            if (!handler) {
                return;
            }
            if constexpr (requires(Handler& value,
                                   util::u16 id,
                                   util::u8 op,
                                   ServiceStatus result,
                                   ByteView data) {
                              {
                                  value.on_response(id, op, result, data)
                              } noexcept -> std::same_as<void>;
                          }) {
                handler->on_response(request_id, opcode, status, payload);
            } else {
                (*handler)(request_id, opcode, status, payload);
            }
        }

        ServiceResponseFn handler_{nullptr};
        void* ctx_{nullptr};
    };

    class ServiceTimeoutHandlerRef {
    public:
        constexpr ServiceTimeoutHandlerRef() noexcept = default;

        static constexpr ServiceTimeoutHandlerRef raw(ServiceTimeoutFn handler, void* ctx) noexcept {
            return ServiceTimeoutHandlerRef{handler, ctx};
        }

        template <typename Handler>
            requires(
                requires(Handler& value, util::u16 request_id, util::u8 opcode) {
                    { value.on_timeout(request_id, opcode) } noexcept -> std::same_as<void>;
                } ||
                requires(Handler& value, util::u16 request_id, util::u8 opcode) {
                    { value(request_id, opcode) } noexcept -> std::same_as<void>;
                })
        static constexpr ServiceTimeoutHandlerRef bind(Handler& handler) noexcept {
            return ServiceTimeoutHandlerRef{&invoke<Handler>, &handler};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return handler_ != nullptr;
        }

        void notify(util::u16 request_id, util::u8 opcode) const noexcept {
            if (handler_) {
                handler_(ctx_, request_id, opcode);
            }
        }

    private:
        constexpr ServiceTimeoutHandlerRef(ServiceTimeoutFn handler, void* ctx) noexcept
            : handler_(handler),
              ctx_(ctx) {
        }

        template <typename Handler>
        static void invoke(void* ctx, util::u16 request_id, util::u8 opcode) noexcept {
            auto* handler = static_cast<Handler*>(ctx);
            if (!handler) {
                return;
            }
            if constexpr (requires(Handler& value, util::u16 id, util::u8 op) {
                              { value.on_timeout(id, op) } noexcept -> std::same_as<void>;
                          }) {
                handler->on_timeout(request_id, opcode);
            } else {
                (*handler)(request_id, opcode);
            }
        }

        ServiceTimeoutFn handler_{nullptr};
        void* ctx_{nullptr};
    };

    class ServiceRouteHandlerRef {
    public:
        constexpr ServiceRouteHandlerRef() noexcept = default;

        static constexpr ServiceRouteHandlerRef raw(ServiceRouteFn handler, void* ctx) noexcept {
            return ServiceRouteHandlerRef{handler, ctx};
        }

        template <typename Handler>
            requires(
                requires(Handler& value,
                         ByteView request,
                         MutByteView response,
                         util::usize* response_size) {
                    {
                        value.on_route(request, response, response_size)
                    } noexcept -> std::same_as<ServiceStatus>;
                } ||
                requires(Handler& value,
                         ByteView request,
                         MutByteView response,
                         util::usize* response_size) {
                    {
                        value(request, response, response_size)
                    } noexcept -> std::same_as<ServiceStatus>;
                })
        static constexpr ServiceRouteHandlerRef bind(Handler& handler) noexcept {
            return ServiceRouteHandlerRef{&invoke<Handler>, &handler};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return handler_ != nullptr;
        }

        [[nodiscard]] ServiceStatus handle(ByteView request,
                                           MutByteView response,
                                           util::usize* response_size) const noexcept {
            if (!handler_) {
                return ServiceStatus::internal_error;
            }
            return handler_(ctx_, request, response, response_size);
        }

    private:
        constexpr ServiceRouteHandlerRef(ServiceRouteFn handler, void* ctx) noexcept
            : handler_(handler),
              ctx_(ctx) {
        }

        template <typename Handler>
        static ServiceStatus invoke(void* ctx,
                                    ByteView request,
                                    MutByteView response,
                                    util::usize* response_size) noexcept {
            auto* handler = static_cast<Handler*>(ctx);
            if (!handler) {
                return ServiceStatus::internal_error;
            }
            if constexpr (requires(Handler& value,
                                   ByteView req,
                                   MutByteView resp,
                                   util::usize* size) {
                              {
                                  value.on_route(req, resp, size)
                              } noexcept -> std::same_as<ServiceStatus>;
                          }) {
                return handler->on_route(request, response, response_size);
            } else {
                return (*handler)(request, response, response_size);
            }
        }

        ServiceRouteFn handler_{nullptr};
        void* ctx_{nullptr};
    };

    template <util::usize MaxPayload,
              util::usize MaxPending = 4,
              util::usize MaxRoutes = 8,
              util::usize MaxDeferred = MaxPending>
    class ServiceSession {
    public:
        using RouteFn = ServiceRouteFn;
        using DeferredRouteFn = void (*)(void* ctx,
                                         ServiceSession& session,
                                         ServiceReplyToken token,
                                         ByteView request) noexcept;
        using ResponseFn = ServiceResponseFn;
        using TimeoutFn = ServiceTimeoutFn;
        using ErrorFn = NetErrorFn;

        class ServiceDeferredRouteHandlerRef {
        public:
            constexpr ServiceDeferredRouteHandlerRef() noexcept = default;

            static constexpr ServiceDeferredRouteHandlerRef raw(DeferredRouteFn handler, void* ctx) noexcept {
                return ServiceDeferredRouteHandlerRef{handler, ctx};
            }

            template <typename Handler>
                requires(
                    requires(Handler& value,
                             ServiceSession& session,
                             ServiceReplyToken token,
                             ByteView request) {
                        { value.on_deferred_route(session, token, request) } noexcept -> std::same_as<void>;
                    } ||
                    requires(Handler& value,
                             ServiceSession& session,
                             ServiceReplyToken token,
                             ByteView request) {
                        { value(session, token, request) } noexcept -> std::same_as<void>;
                    })
            static constexpr ServiceDeferredRouteHandlerRef bind(Handler& handler) noexcept {
                return ServiceDeferredRouteHandlerRef{&invoke<Handler>, &handler};
            }

            [[nodiscard]] constexpr explicit operator bool() const noexcept {
                return handler_ != nullptr;
            }

            void notify(ServiceSession& session,
                        ServiceReplyToken token,
                        ByteView request) const noexcept {
                if (handler_) {
                    handler_(ctx_, session, token, request);
                }
            }

        private:
            constexpr ServiceDeferredRouteHandlerRef(DeferredRouteFn handler, void* ctx) noexcept
                : handler_(handler),
                  ctx_(ctx) {
            }

            template <typename Handler>
            static void invoke(void* ctx,
                               ServiceSession& session,
                               ServiceReplyToken token,
                               ByteView request) noexcept {
                auto* handler = static_cast<Handler*>(ctx);
                if (!handler) {
                    return;
                }
                if constexpr (requires(Handler& value,
                                       ServiceSession& svc,
                                       ServiceReplyToken reply_token,
                                       ByteView data) {
                                  {
                                      value.on_deferred_route(svc, reply_token, data)
                                  } noexcept -> std::same_as<void>;
                              }) {
                    handler->on_deferred_route(session, token, request);
                } else {
                    (*handler)(session, token, request);
                }
            }

            DeferredRouteFn handler_{nullptr};
            void* ctx_{nullptr};
        };

        ServiceSession() {
            request_.set_request_handler(WireSession::RequestHandlerRef::raw(
                &ServiceSession::on_request_trampoline,
                this));
            request_.set_error_handler(NetErrorHandlerRef::raw(
                &ServiceSession::on_request_error_trampoline,
                this));
        }

        void set_sender(StreamSenderRef sender = {}) noexcept {
            request_.set_sender(sender);
        }

        void set_sender(FrameSendFn fn, void* ctx) noexcept {
            set_sender(StreamSenderRef::raw(fn, ctx));
        }

        void set_error_handler(NetErrorHandlerRef handler = {}) noexcept {
            error_ = handler;
        }

        void set_error_handler(ErrorFn fn, void* ctx) noexcept {
            set_error_handler(NetErrorHandlerRef::raw(fn, ctx));
        }

        void reset() noexcept {
            request_.reset();
            clear_pending();
            clear_deferred();
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
            for (const auto& pending : responses_) {
                if (pending.used) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] bool has_deferred() const noexcept {
            return deferred_count() != 0;
        }

        [[nodiscard]] util::usize deferred_count() const noexcept {
            util::usize count = 0;
            for (const auto& pending : deferred_) {
                if (pending.used) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
        }

        [[nodiscard]] Result<void> set_route(util::u8 opcode, ServiceRouteHandlerRef handler) noexcept {
            if (!handler) {
                return util::unexpected(errc::invalid_arg);
            }

            auto* route = ensure_route(opcode);
            if (!route) {
                return util::unexpected(errc::busy);
            }

            route->used = true;
            route->opcode = opcode;
            route->sync = handler;
            route->deferred = {};
            return {};
        }

        [[nodiscard]] Result<void> set_route(util::u8 opcode, RouteFn fn, void* ctx = nullptr) noexcept {
            return set_route(opcode, ServiceRouteHandlerRef::raw(fn, ctx));
        }

        [[nodiscard]] Result<void> set_deferred_route(util::u8 opcode,
                                                      ServiceDeferredRouteHandlerRef handler) noexcept {
            if (!handler) {
                return util::unexpected(errc::invalid_arg);
            }

            auto* route = ensure_route(opcode);
            if (!route) {
                return util::unexpected(errc::busy);
            }

            route->used = true;
            route->opcode = opcode;
            route->sync = {};
            route->deferred = handler;
            return {};
        }

        [[nodiscard]] Result<void> set_deferred_route(util::u8 opcode,
                                                      DeferredRouteFn fn,
                                                      void* ctx = nullptr) noexcept {
            return set_deferred_route(opcode, ServiceDeferredRouteHandlerRef::raw(fn, ctx));
        }

        [[nodiscard]] bool clear_route(util::u8 opcode) noexcept {
            for (auto& route : routes_) {
                if (!route.used || route.opcode != opcode) continue;
                route = {};
                return true;
            }
            return false;
        }

        void clear_routes() noexcept {
            for (auto& route : routes_) {
                route = {};
            }
        }

        [[nodiscard]] bool has_route(util::u8 opcode) const noexcept {
            return find_route(opcode) != nullptr;
        }

        void feed(ByteView data) noexcept {
            request_.feed(data);
        }

        void notify_writable() noexcept {
            request_.notify_writable();
        }

        [[nodiscard]] Result<util::u16> send_request(util::u8 opcode,
                                                     ByteView payload,
                                                     util::u32 now_ms,
                                                     util::u32 timeout_ms,
                                                     ServiceResponseHandlerRef on_response = {},
                                                     ServiceTimeoutHandlerRef on_timeout = {}) noexcept {
            if (payload.size() > MaxPayload) {
                return util::unexpected(errc::buffer_overflow);
            }

            auto* pending = allocate_pending();
            if (!pending) {
                return util::unexpected(errc::busy);
            }

            pending->owner = this;
            pending->on_response = on_response;
            pending->on_timeout = on_timeout;

            auto sent = request_.send_request(opcode,
                                              payload,
                                              now_ms,
                                              timeout_ms,
                                              RequestResponseHandlerRef::raw(
                                                  &ServiceSession::on_response_trampoline,
                                                  pending),
                                              RequestTimeoutHandlerRef::raw(
                                                  &ServiceSession::on_timeout_trampoline,
                                                  pending));
            if (!sent) {
                *pending = {};
                return util::unexpected(sent.error());
            }

            pending->request_id = sent.value();
            return sent.value();
        }

        [[nodiscard]] Result<util::u16> send_request(util::u8 opcode,
                                                     ByteView payload,
                                                     util::u32 now_ms,
                                                     util::u32 timeout_ms,
                                                     ResponseFn on_response,
                                                     TimeoutFn on_timeout,
                                                     void* user = nullptr) noexcept {
            return send_request(opcode,
                                payload,
                                now_ms,
                                timeout_ms,
                                ServiceResponseHandlerRef::raw(on_response, user),
                                ServiceTimeoutHandlerRef::raw(on_timeout, user));
        }

        [[nodiscard]] bool cancel_request(util::u16 request_id) noexcept {
            if (!request_.cancel_request(request_id)) {
                return false;
            }
            clear_pending_request(request_id);
            return true;
        }

        [[nodiscard]] Result<void> send_response(util::u16 request_id,
                                                 util::u8 opcode,
                                                 ServiceStatus status,
                                                 ByteView payload = {}) noexcept {
            if (payload.size() > MaxPayload) {
                return util::unexpected(errc::buffer_overflow);
            }

            std::array<util::u8, MaxPayload + 1> wire{};
            wire[0] = static_cast<util::u8>(status);
            for (util::usize i = 0; i < payload.size(); ++i) {
                wire[i + 1] = payload[i];
            }

            return request_.send_response(request_id,
                                          opcode,
                                          ByteView{wire.data(), payload.size() + 1},
                                          service_status_ok(status));
        }

        [[nodiscard]] Result<void> send_error(util::u16 request_id,
                                              util::u8 opcode,
                                              ServiceStatus status,
                                              ByteView payload = {}) noexcept {
            if (service_status_ok(status)) {
                return util::unexpected(errc::invalid_arg);
            }
            return send_response(request_id, opcode, status, payload);
        }

        [[nodiscard]] Result<void> send_deferred_response(ServiceReplyToken token,
                                                          ServiceStatus status,
                                                          ByteView payload = {}) noexcept {
            auto* deferred = find_deferred(token);
            if (!deferred) {
                return util::unexpected(errc::noent);
            }

            auto sent = send_response(deferred->request_id, deferred->opcode, status, payload);
            if (sent) {
                *deferred = {};
            }
            return sent;
        }

        [[nodiscard]] Result<void> send_deferred_error(ServiceReplyToken token,
                                                       ServiceStatus status,
                                                       ByteView payload = {}) noexcept {
            if (service_status_ok(status)) {
                return util::unexpected(errc::invalid_arg);
            }
            return send_deferred_response(token, status, payload);
        }

        [[nodiscard]] bool cancel_deferred(ServiceReplyToken token) noexcept {
            auto* deferred = find_deferred(token);
            if (!deferred) {
                return false;
            }
            *deferred = {};
            return true;
        }

        void tick(util::u32 now_ms) noexcept {
            request_.tick(now_ms);
        }

        void on_transport_closed() noexcept {
            clear_pending();
            clear_deferred();
            request_.on_transport_closed();
        }

        void on_transport_error(errc error) noexcept {
            clear_pending();
            clear_deferred();
            request_.on_transport_error(error);
        }

    private:
        using WireSession = RequestSession<MaxPayload + 1, MaxPending>;

        struct RouteEntry {
            bool used{false};
            util::u8 opcode{0};
            ServiceRouteHandlerRef sync{};
            ServiceDeferredRouteHandlerRef deferred{};
        };

        struct PendingResponse {
            bool used{false};
            util::u16 request_id{0};
            ServiceSession* owner{nullptr};
            ServiceResponseHandlerRef on_response{};
            ServiceTimeoutHandlerRef on_timeout{};
        };

        struct DeferredReply {
            bool used{false};
            util::u16 request_id{0};
            util::u8 opcode{0};
            util::u16 generation{0};
        };

        struct DecodedResponse {
            ServiceStatus status{ServiceStatus::internal_error};
            ByteView payload{};
        };

        [[nodiscard]] PendingResponse* allocate_pending() noexcept {
            for (auto& pending : responses_) {
                if (pending.used) continue;
                pending = {};
                pending.used = true;
                return &pending;
            }
            return nullptr;
        }

        [[nodiscard]] RouteEntry* ensure_route(util::u8 opcode) noexcept {
            for (auto& route : routes_) {
                if (route.used && route.opcode == opcode) {
                    return &route;
                }
            }

            for (auto& route : routes_) {
                if (route.used) continue;
                return &route;
            }
            return nullptr;
        }

        [[nodiscard]] RouteEntry* find_route(util::u8 opcode) noexcept {
            for (auto& route : routes_) {
                if (route.used && route.opcode == opcode) {
                    return &route;
                }
            }
            return nullptr;
        }

        [[nodiscard]] const RouteEntry* find_route(util::u8 opcode) const noexcept {
            for (const auto& route : routes_) {
                if (route.used && route.opcode == opcode) {
                    return &route;
                }
            }
            return nullptr;
        }

        [[nodiscard]] Result<ServiceReplyToken> allocate_deferred(util::u16 request_id,
                                                                  util::u8 opcode) noexcept {
            for (util::usize i = 0; i < deferred_.size(); ++i) {
                auto& deferred = deferred_[i];
                if (deferred.used) continue;

                ++deferred.generation;
                if (deferred.generation == 0) {
                    deferred.generation = 1;
                }
                deferred.used = true;
                deferred.request_id = request_id;
                deferred.opcode = opcode;
                return ServiceReplyToken{
                    request_id,
                    opcode,
                    static_cast<util::u16>(i),
                    deferred.generation
                };
            }
            return util::unexpected(errc::busy);
        }

        [[nodiscard]] DeferredReply* find_deferred(ServiceReplyToken token) noexcept {
            if (!token.valid()) {
                return nullptr;
            }
            if (token.slot >= deferred_.size()) {
                return nullptr;
            }

            auto& deferred = deferred_[token.slot];
            if (!deferred.used) {
                return nullptr;
            }
            if (deferred.generation != token.generation) {
                return nullptr;
            }
            if (deferred.request_id != token.request_id || deferred.opcode != token.opcode) {
                return nullptr;
            }
            return &deferred;
        }

        void clear_pending_request(util::u16 request_id) noexcept {
            for (auto& pending : responses_) {
                if (!pending.used || pending.request_id != request_id) continue;
                pending = {};
                return;
            }
        }

        static void on_request_trampoline(void* ctx,
                                          WireSession&,
                                          util::u16 request_id,
                                          util::u8 opcode,
                                          ByteView payload) noexcept {
            auto* self = static_cast<ServiceSession*>(ctx);
            if (self) {
                self->on_request(request_id, opcode, payload);
            }
        }

        static void on_request_error_trampoline(void* ctx, errc error) noexcept {
            auto* self = static_cast<ServiceSession*>(ctx);
            if (self) {
                self->last_error_ = error;
                self->notify_error(error);
            }
        }

        static void on_response_trampoline(void* ctx,
                                           util::u16 request_id,
                                           util::u8 opcode,
                                           bool ok,
                                           ByteView payload) noexcept {
            auto* pending = static_cast<PendingResponse*>(ctx);
            if (pending && pending->owner) {
                pending->owner->on_response(*pending, request_id, opcode, ok, payload);
            }
        }

        static void on_timeout_trampoline(void* ctx,
                                          util::u16 request_id,
                                          util::u8 opcode) noexcept {
            auto* pending = static_cast<PendingResponse*>(ctx);
            if (pending && pending->owner) {
                pending->owner->on_timeout(*pending, request_id, opcode);
            }
        }

        void on_request(util::u16 request_id,
                        util::u8 opcode,
                        ByteView payload) noexcept {
            auto* route = find_route(opcode);
            if (!route) {
                auto sent = send_error(request_id, opcode, ServiceStatus::not_supported);
                if (!sent) {
                    last_error_ = sent.error();
                    notify_error(last_error_);
                }
                return;
            }

            if (route->sync) {
                std::array<util::u8, MaxPayload> response{};
                util::usize response_size = 0;
                const auto status = route->sync.handle(
                    payload,
                    MutByteView{response.data(), response.size()},
                    &response_size);
                if (response_size > response.size()) {
                    last_error_ = errc::format_error;
                    notify_error(last_error_);
                    response_size = 0;
                }

                auto sent = send_response(request_id,
                                          opcode,
                                          status,
                                          ByteView{response.data(), response_size});
                if (!sent) {
                    last_error_ = sent.error();
                    notify_error(last_error_);
                }
                return;
            }

            if (route->deferred) {
                auto token = allocate_deferred(request_id, opcode);
                if (!token) {
                    auto sent = send_error(request_id, opcode, ServiceStatus::busy);
                    if (!sent) {
                        last_error_ = sent.error();
                        notify_error(last_error_);
                    }
                    return;
                }

                route->deferred.notify(*this, token.value(), payload);
                return;
            }

            auto sent = send_error(request_id, opcode, ServiceStatus::not_supported);
            if (!sent) {
                last_error_ = sent.error();
                notify_error(last_error_);
            }
        }

        void on_response(PendingResponse& pending,
                         util::u16 request_id,
                         util::u8 opcode,
                         bool ok,
                         ByteView payload) noexcept {
            const auto callback = pending.on_response;
            pending = {};

            auto decoded = decode_response(ok, payload);
            if (!decoded) {
                last_error_ = decoded.error();
                notify_error(last_error_);
                return;
            }

            if (callback) {
                callback.notify(request_id, opcode, decoded.value().status, decoded.value().payload);
            }
        }

        void on_timeout(PendingResponse& pending,
                        util::u16 request_id,
                        util::u8 opcode) noexcept {
            const auto callback = pending.on_timeout;
            pending = {};

            if (callback) {
                callback.notify(request_id, opcode);
                return;
            }

            last_error_ = errc::timeout;
            notify_error(last_error_);
        }

        [[nodiscard]] Result<DecodedResponse> decode_response(bool ok,
                                                              ByteView payload) const noexcept {
            if (payload.empty()) {
                return DecodedResponse{
                    ok ? ServiceStatus::ok : ServiceStatus::internal_error,
                    ByteView{}
                };
            }

            const auto status = static_cast<ServiceStatus>(payload[0]);
            if (ok != service_status_ok(status)) {
                return util::unexpected(errc::format_error);
            }

            return DecodedResponse{status, payload.subspan(1)};
        }

        void clear_pending() noexcept {
            for (auto& pending : responses_) {
                pending = {};
            }
        }

        void clear_deferred() noexcept {
            for (auto& pending : deferred_) {
                pending.used = false;
                pending.request_id = 0;
                pending.opcode = 0;
            }
        }

        void notify_error(errc error) noexcept {
            error_.notify(error);
        }

        WireSession request_{};
        std::array<RouteEntry, MaxRoutes> routes_{};
        std::array<PendingResponse, MaxPending> responses_{};
        std::array<DeferredReply, MaxDeferred> deferred_{};
        NetErrorHandlerRef error_{};
        errc last_error_{errc::ok};
    };
}
