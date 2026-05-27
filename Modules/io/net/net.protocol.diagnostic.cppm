module;

#include <array>

export module net.protocol.diagnostic;

export import net.forward;
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

    namespace detail {
        [[nodiscard]] constexpr std::array<util::u8, 4> ipv4_bytes(IpAddress address) noexcept {
            if (!address.is_ipv4()) {
                return {};
            }
            return {
                address.bytes[0],
                address.bytes[1],
                address.bytes[2],
                address.bytes[3],
            };
        }

        [[nodiscard]] constexpr IpAddress ipv4_from_bytes(
            const std::array<util::u8, 4>& bytes) noexcept {
            return IpAddress::ipv4(bytes[0], bytes[1], bytes[2], bytes[3]);
        }
    }

    struct ForwardExplainRequest {
        Ipv4ForwardingPort ingress_port{Ipv4ForwardingPort::a};
        util::u8 ttl{64};
        std::array<util::u8, 4> destination{};

        [[nodiscard]] static constexpr ForwardExplainRequest ipv4(
            Ipv4ForwardingPort ingress,
            IpAddress target,
            util::u8 target_ttl = 64u) noexcept {
            return ForwardExplainRequest{
                .ingress_port = ingress,
                .ttl = target_ttl,
                .destination = detail::ipv4_bytes(target),
            };
        }

        [[nodiscard]] constexpr IpAddress destination_address() const noexcept {
            return detail::ipv4_from_bytes(destination);
        }
    };

    struct ForwardExplainReply {
        Ipv4ForwardingDisposition disposition{Ipv4ForwardingDisposition::forwarded};
        Ipv4ForwardingReason reason{Ipv4ForwardingReason::none};
        Ipv4ForwardingPort egress_port{Ipv4ForwardingPort::a};
        util::u8 has_egress{0};
        util::u8 has_decision{0};
        util::u8 routing_configured{0};
        std::array<util::u8, 4> network{};
        util::u8 prefix_length{0};
        util::u8 has_next_hop{0};
        std::array<util::u8, 4> next_hop{};
        util::u16 metric{0};
        util::u8 from_connected_prefix{0};

        [[nodiscard]] constexpr bool forwarded() const noexcept {
            return disposition == Ipv4ForwardingDisposition::forwarded;
        }

        [[nodiscard]] constexpr bool ttl_expired() const noexcept {
            return disposition == Ipv4ForwardingDisposition::ttl_expired;
        }

        [[nodiscard]] constexpr bool destination_unreachable() const noexcept {
            return disposition == Ipv4ForwardingDisposition::destination_unreachable;
        }

        [[nodiscard]] constexpr bool local_dropped() const noexcept {
            return disposition == Ipv4ForwardingDisposition::local_dropped;
        }

        [[nodiscard]] constexpr bool emits_icmp_error() const noexcept {
            return ttl_expired() || destination_unreachable();
        }

        [[nodiscard]] constexpr bool has_egress_port() const noexcept {
            return has_egress != 0u;
        }

        [[nodiscard]] constexpr bool has_decision_snapshot() const noexcept {
            return has_decision != 0u;
        }

        [[nodiscard]] constexpr bool routing_is_configured() const noexcept {
            return routing_configured != 0u;
        }

        [[nodiscard]] constexpr bool has_next_hop_address() const noexcept {
            return has_next_hop != 0u;
        }

        [[nodiscard]] constexpr bool is_from_connected_prefix() const noexcept {
            return from_connected_prefix != 0u;
        }

        [[nodiscard]] constexpr bool uses_explicit_route() const noexcept {
            return has_decision_snapshot()
                && reason == Ipv4ForwardingReason::explicit_route;
        }

        [[nodiscard]] constexpr bool uses_connected_prefix() const noexcept {
            return has_decision_snapshot()
                && reason == Ipv4ForwardingReason::connected_prefix;
        }

        [[nodiscard]] constexpr bool uses_opposite_port_fallback() const noexcept {
            return reason == Ipv4ForwardingReason::opposite_port_fallback;
        }

        [[nodiscard]] constexpr IpAddress network_address() const noexcept {
            return detail::ipv4_from_bytes(network);
        }

        [[nodiscard]] constexpr IpAddress next_hop_address() const noexcept {
            return has_next_hop_address()
                ? detail::ipv4_from_bytes(next_hop)
                : IpAddress{};
        }

        [[nodiscard]] constexpr Ipv4ForwardingDecisionSnapshot decision() const noexcept {
            return Ipv4ForwardingDecisionSnapshot{
                .network = network_address(),
                .prefix_length = prefix_length,
                .egress_port = egress_port,
                .has_next_hop = has_next_hop_address(),
                .next_hop = next_hop_address(),
                .metric = metric,
                .from_connected_prefix = is_from_connected_prefix(),
            };
        }

        [[nodiscard]] static constexpr ForwardExplainReply from_snapshot(
            const Ipv4ForwardingExplanationSnapshot& snapshot) noexcept {
            return ForwardExplainReply{
                .disposition = snapshot.disposition,
                .reason = snapshot.reason,
                .egress_port = snapshot.egress_port,
                .has_egress = static_cast<util::u8>(snapshot.has_egress),
                .has_decision = static_cast<util::u8>(snapshot.has_decision),
                .routing_configured = static_cast<util::u8>(snapshot.routing_configured),
                .network = detail::ipv4_bytes(snapshot.decision.network),
                .prefix_length = snapshot.decision.prefix_length,
                .has_next_hop = static_cast<util::u8>(snapshot.decision.has_next_hop),
                .next_hop = detail::ipv4_bytes(snapshot.decision.next_hop),
                .metric = snapshot.decision.metric,
                .from_connected_prefix = static_cast<util::u8>(
                    snapshot.decision.from_connected_prefix),
            };
        }
    };

    using ForwardInspectRequest = ForwardExplainRequest;

    struct ForwardInspectReply {
        Ipv4ForwardingDisposition disposition{Ipv4ForwardingDisposition::forwarded};
        Ipv4ForwardingReason reason{Ipv4ForwardingReason::none};
        Ipv4ForwardingPort egress_port{Ipv4ForwardingPort::a};
        util::u8 has_egress{0};
        util::u8 has_decision{0};
        util::u8 routing_configured{0};
        std::array<util::u8, 4> network{};
        util::u8 prefix_length{0};
        util::u8 has_next_hop{0};
        std::array<util::u8, 4> next_hop{};
        util::u16 metric{0};
        util::u8 from_connected_prefix{0};
        util::u8 route_count{0};
        util::u8 has_selected_route{0};
        util::u8 selected_route_index{0};
        std::array<util::u8, 4> selected_network{};
        util::u8 selected_prefix_length{0};
        Ipv4ForwardingPort selected_egress_port{Ipv4ForwardingPort::a};
        util::u8 selected_has_next_hop{0};
        std::array<util::u8, 4> selected_next_hop{};
        util::u16 selected_metric{0};

        [[nodiscard]] constexpr bool forwarded() const noexcept {
            return disposition == Ipv4ForwardingDisposition::forwarded;
        }

        [[nodiscard]] constexpr bool ttl_expired() const noexcept {
            return disposition == Ipv4ForwardingDisposition::ttl_expired;
        }

        [[nodiscard]] constexpr bool destination_unreachable() const noexcept {
            return disposition == Ipv4ForwardingDisposition::destination_unreachable;
        }

        [[nodiscard]] constexpr bool local_dropped() const noexcept {
            return disposition == Ipv4ForwardingDisposition::local_dropped;
        }

        [[nodiscard]] constexpr bool emits_icmp_error() const noexcept {
            return ttl_expired() || destination_unreachable();
        }

        [[nodiscard]] constexpr bool has_egress_port() const noexcept {
            return has_egress != 0u;
        }

        [[nodiscard]] constexpr bool has_decision_snapshot() const noexcept {
            return has_decision != 0u;
        }

        [[nodiscard]] constexpr bool routing_is_configured() const noexcept {
            return routing_configured != 0u;
        }

        [[nodiscard]] constexpr bool has_next_hop_address() const noexcept {
            return has_next_hop != 0u;
        }

        [[nodiscard]] constexpr bool is_from_connected_prefix() const noexcept {
            return from_connected_prefix != 0u;
        }

        [[nodiscard]] constexpr bool uses_explicit_route() const noexcept {
            return has_decision_snapshot()
                && reason == Ipv4ForwardingReason::explicit_route;
        }

        [[nodiscard]] constexpr bool uses_connected_prefix() const noexcept {
            return has_decision_snapshot()
                && reason == Ipv4ForwardingReason::connected_prefix;
        }

        [[nodiscard]] constexpr bool uses_opposite_port_fallback() const noexcept {
            return reason == Ipv4ForwardingReason::opposite_port_fallback;
        }

        [[nodiscard]] constexpr bool has_selected_route_entry() const noexcept {
            return has_selected_route != 0u;
        }

        [[nodiscard]] constexpr util::usize route_total() const noexcept {
            return static_cast<util::usize>(route_count);
        }

        [[nodiscard]] constexpr IpAddress network_address() const noexcept {
            return detail::ipv4_from_bytes(network);
        }

        [[nodiscard]] constexpr IpAddress next_hop_address() const noexcept {
            return has_next_hop_address()
                ? detail::ipv4_from_bytes(next_hop)
                : IpAddress{};
        }

        [[nodiscard]] constexpr IpAddress selected_network_address() const noexcept {
            return detail::ipv4_from_bytes(selected_network);
        }

        [[nodiscard]] constexpr bool selected_route_has_next_hop_address() const noexcept {
            return selected_has_next_hop != 0u;
        }

        [[nodiscard]] constexpr IpAddress selected_next_hop_address() const noexcept {
            return selected_route_has_next_hop_address()
                ? detail::ipv4_from_bytes(selected_next_hop)
                : IpAddress{};
        }

        [[nodiscard]] constexpr Ipv4ForwardingDecisionSnapshot decision() const noexcept {
            return Ipv4ForwardingDecisionSnapshot{
                .network = network_address(),
                .prefix_length = prefix_length,
                .egress_port = egress_port,
                .has_next_hop = has_next_hop_address(),
                .next_hop = next_hop_address(),
                .metric = metric,
                .from_connected_prefix = is_from_connected_prefix(),
            };
        }

        [[nodiscard]] constexpr Ipv4ForwardingRoute selected_route() const noexcept {
            return Ipv4ForwardingRoute{
                .network = selected_network_address(),
                .prefix_length = selected_prefix_length,
                .egress_port = selected_egress_port,
                .has_next_hop = selected_route_has_next_hop_address(),
                .next_hop = selected_next_hop_address(),
                .metric = selected_metric,
            };
        }

        [[nodiscard]] constexpr ForwardExplainReply explanation() const noexcept {
            return ForwardExplainReply{
                .disposition = disposition,
                .reason = reason,
                .egress_port = egress_port,
                .has_egress = has_egress,
                .has_decision = has_decision,
                .routing_configured = routing_configured,
                .network = network,
                .prefix_length = prefix_length,
                .has_next_hop = has_next_hop,
                .next_hop = next_hop,
                .metric = metric,
                .from_connected_prefix = from_connected_prefix,
            };
        }

        [[nodiscard]] static constexpr ForwardInspectReply from_explain(
            const ForwardExplainReply& explain,
            util::u8 total_routes = 0u,
            bool has_selected = false,
            util::u8 selected_index = 0u,
            Ipv4ForwardingRoute selected_route_value = {}) noexcept {
            return ForwardInspectReply{
                .disposition = explain.disposition,
                .reason = explain.reason,
                .egress_port = explain.egress_port,
                .has_egress = explain.has_egress,
                .has_decision = explain.has_decision,
                .routing_configured = explain.routing_configured,
                .network = explain.network,
                .prefix_length = explain.prefix_length,
                .has_next_hop = explain.has_next_hop,
                .next_hop = explain.next_hop,
                .metric = explain.metric,
                .from_connected_prefix = explain.from_connected_prefix,
                .route_count = total_routes,
                .has_selected_route = static_cast<util::u8>(has_selected),
                .selected_route_index = selected_index,
                .selected_network = detail::ipv4_bytes(selected_route_value.network),
                .selected_prefix_length = selected_route_value.prefix_length,
                .selected_egress_port = selected_route_value.egress_port,
                .selected_has_next_hop = static_cast<util::u8>(selected_route_value.has_next_hop),
                .selected_next_hop = detail::ipv4_bytes(selected_route_value.next_hop),
                .selected_metric = selected_route_value.metric,
            };
        }

        [[nodiscard]] static constexpr ForwardInspectReply from_snapshot(
            const Ipv4ForwardingExplanationSnapshot& snapshot,
            util::u8 total_routes = 0u,
            bool has_selected = false,
            util::u8 selected_index = 0u,
            Ipv4ForwardingRoute selected_route_value = {}) noexcept {
            return from_explain(
                ForwardExplainReply::from_snapshot(snapshot),
                total_routes,
                has_selected,
                selected_index,
                selected_route_value);
        }
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

    using ForwardExplainOp = WireServiceOp<
        0x64u,
        ForwardExplainRequest,
        ForwardExplainReply,
        WireMembers<
            &ForwardExplainRequest::ingress_port,
            &ForwardExplainRequest::ttl,
            &ForwardExplainRequest::destination>,
        WireMembers<
            &ForwardExplainReply::disposition,
            &ForwardExplainReply::reason,
            &ForwardExplainReply::egress_port,
            &ForwardExplainReply::has_egress,
            &ForwardExplainReply::has_decision,
            &ForwardExplainReply::routing_configured,
            &ForwardExplainReply::network,
            &ForwardExplainReply::prefix_length,
            &ForwardExplainReply::has_next_hop,
            &ForwardExplainReply::next_hop,
            &ForwardExplainReply::metric,
            &ForwardExplainReply::from_connected_prefix>>;

    using ForwardInspectOp = WireServiceOp<
        0x65u,
        ForwardInspectRequest,
        ForwardInspectReply,
        WireMembers<
            &ForwardInspectRequest::ingress_port,
            &ForwardInspectRequest::ttl,
            &ForwardInspectRequest::destination>,
        WireMembers<
            &ForwardInspectReply::disposition,
            &ForwardInspectReply::reason,
            &ForwardInspectReply::egress_port,
            &ForwardInspectReply::has_egress,
            &ForwardInspectReply::has_decision,
            &ForwardInspectReply::routing_configured,
            &ForwardInspectReply::network,
            &ForwardInspectReply::prefix_length,
            &ForwardInspectReply::has_next_hop,
            &ForwardInspectReply::next_hop,
            &ForwardInspectReply::metric,
            &ForwardInspectReply::from_connected_prefix,
            &ForwardInspectReply::route_count,
            &ForwardInspectReply::has_selected_route,
            &ForwardInspectReply::selected_route_index,
            &ForwardInspectReply::selected_network,
            &ForwardInspectReply::selected_prefix_length,
            &ForwardInspectReply::selected_egress_port,
            &ForwardInspectReply::selected_has_next_hop,
            &ForwardInspectReply::selected_next_hop,
            &ForwardInspectReply::selected_metric>>;

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
        using ForwardExplainResponseFn = typename Session::template ResponseFn<ForwardExplainOp>;
        using ForwardInspectResponseFn = typename Session::template ResponseFn<ForwardInspectOp>;
        using PingTimeoutFn = typename Session::template TimeoutFn<PingOp>;
        using CountTimeoutFn = typename Session::template TimeoutFn<CountOp>;
        using SlowCountTimeoutFn = typename Session::template TimeoutFn<SlowCountOp>;
        using MetaTimeoutFn = typename Session::template TimeoutFn<MetaOp>;
        using ForwardExplainTimeoutFn = typename Session::template TimeoutFn<ForwardExplainOp>;
        using ForwardInspectTimeoutFn = typename Session::template TimeoutFn<ForwardInspectOp>;

        void set_sender(StreamSenderRef sender = {}) noexcept {
            session_.set_sender(sender);
        }

        void set_sender(FrameSendFn fn, void* ctx) noexcept {
            set_sender(StreamSenderRef::raw(fn, ctx));
        }

        void set_error_handler(NetErrorHandlerRef handler = {}) noexcept {
            session_.set_error_handler(handler);
        }

        void set_error_handler(ErrorFn fn, void* ctx) noexcept {
            set_error_handler(NetErrorHandlerRef::raw(fn, ctx));
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

        [[nodiscard]] Result<util::u16> query_forward_explain(
            const ForwardExplainRequest& request,
            util::u32 now_ms,
            util::u32 timeout_ms,
            ForwardExplainResponseFn on_response = nullptr,
            ForwardExplainTimeoutFn on_timeout = nullptr,
            void* user = nullptr) noexcept {
            return session_.template send_request<ForwardExplainOp>(
                request,
                now_ms,
                timeout_ms,
                on_response,
                on_timeout,
                user);
        }

        [[nodiscard]] Result<util::u16> query_forward_inspect(
            const ForwardInspectRequest& request,
            util::u32 now_ms,
            util::u32 timeout_ms,
            ForwardInspectResponseFn on_response = nullptr,
            ForwardInspectTimeoutFn on_timeout = nullptr,
            void* user = nullptr) noexcept {
            return session_.template send_request<ForwardInspectOp>(
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
        using ForwardExplainHandler = typename Session::template RouteFn<ForwardExplainOp>;
        using ForwardInspectHandler = typename Session::template RouteFn<ForwardInspectOp>;
        using SlowCountHandler = void (*)(void* ctx,
                                          Server& server,
                                          ServiceReplyToken token,
                                          const CounterValue& request) noexcept;

        void set_sender(StreamSenderRef sender = {}) noexcept {
            session_.set_sender(sender);
        }

        void set_sender(FrameSendFn fn, void* ctx) noexcept {
            set_sender(StreamSenderRef::raw(fn, ctx));
        }

        void set_error_handler(NetErrorHandlerRef handler = {}) noexcept {
            session_.set_error_handler(handler);
        }

        void set_error_handler(ErrorFn fn, void* ctx) noexcept {
            set_error_handler(NetErrorHandlerRef::raw(fn, ctx));
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

        [[nodiscard]] Result<void> on_forward_explain(ForwardExplainHandler fn,
                                                      void* ctx = nullptr) noexcept {
            return session_.template set_route<ForwardExplainOp>(fn, ctx);
        }

        [[nodiscard]] Result<void> on_forward_inspect(ForwardInspectHandler fn,
                                                      void* ctx = nullptr) noexcept {
            return session_.template set_route<ForwardInspectOp>(fn, ctx);
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

        [[nodiscard]] bool clear_forward_explain() noexcept {
            return session_.template clear_route<ForwardExplainOp>();
        }

        [[nodiscard]] bool clear_forward_inspect() noexcept {
            return session_.template clear_route<ForwardInspectOp>();
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

        [[nodiscard]] bool has_forward_explain() const noexcept {
            return session_.template has_route<ForwardExplainOp>();
        }

        [[nodiscard]] bool has_forward_inspect() const noexcept {
            return session_.template has_route<ForwardInspectOp>();
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
