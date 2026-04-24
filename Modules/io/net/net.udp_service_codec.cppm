module;

#include <array>
#include <cstring>
#include <type_traits>

export module net.udp_service_codec;

export import net.packet;
export import net.service_codec;
export import net.udp;
import util.core;
import util.error;
import util.expected;

export namespace net::udp::service {
    namespace detail {
        [[nodiscard]] constexpr bool same_ip(const IpAddress& lhs, const IpAddress& rhs) noexcept {
            if (lhs.family != rhs.family) {
                return false;
            }

            const auto size = lhs.is_ipv4() ? 4u : lhs.bytes.size();
            for (util::usize i = 0; i < size; ++i) {
                if (lhs.bytes[i] != rhs.bytes[i]) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] constexpr bool same_endpoint(const Endpoint& lhs,
                                                   const Endpoint& rhs) noexcept {
            return lhs.port == rhs.port && same_ip(lhs.address, rhs.address);
        }

        [[nodiscard]] constexpr bool endpoint_matches(const Endpoint& expected,
                                                      const Endpoint& actual) noexcept {
            if (expected.port != actual.port) {
                return false;
            }
            if (expected.address.is_any() || expected.address.is_unspecified()) {
                return true;
            }
            return same_ip(expected.address, actual.address);
        }

        [[nodiscard]] constexpr bool deadline_expired(util::u32 now_ms,
                                                      util::u32 deadline_ms) noexcept {
            return static_cast<util::i32>(now_ms - deadline_ms) >= 0;
        }

        struct CallbackStorage {
            std::array<util::u8, sizeof(void (*)())> bytes{};
        };

        template <class Fn>
        constexpr void store_callback(CallbackStorage& slot, Fn fn) noexcept {
            static_assert(std::is_trivially_copyable_v<Fn>);
            static_assert(sizeof(Fn) <= sizeof(slot.bytes));

            slot.bytes = {};
            if constexpr (sizeof(Fn) != 0) {
                std::memcpy(slot.bytes.data(), &fn, sizeof(Fn));
            }
        }

        template <class Fn>
        [[nodiscard]] constexpr Fn load_callback(const CallbackStorage& slot) noexcept {
            static_assert(std::is_trivially_copyable_v<Fn>);
            static_assert(sizeof(Fn) <= sizeof(slot.bytes));

            Fn fn{};
            if constexpr (sizeof(Fn) != 0) {
                std::memcpy(&fn, slot.bytes.data(), sizeof(Fn));
            }
            return fn;
        }
    }

    template <class Wire, util::usize MaxPayload = 64, util::usize MaxPending = 4>
    class Client {
    public:
        using Status = typename Wire::Status;
        using DatagramView = typename Wire::DatagramView;
        using SendFn = Result<UdpSendDisposition> (*)(void* ctx,
                                                      Endpoint local,
                                                      const Endpoint& peer,
                                                      ByteView payload) noexcept;
        using ErrorFn = void (*)(void* ctx, errc error) noexcept;

        template <ServiceOperation Op>
        using ResponseFn = void (*)(void* ctx,
                                    util::u16 request_id,
                                    Status status,
                                    const typename Op::Response& response) noexcept;

        template <ServiceOperation Op>
        using TimeoutFn = void (*)(void* ctx, util::u16 request_id) noexcept;

        void set_sender(SendFn fn, void* ctx) noexcept {
            sender_ = fn;
            sender_ctx_ = ctx;
        }

        void set_error_handler(ErrorFn fn, void* ctx = nullptr) noexcept {
            error_ = fn;
            error_ctx_ = ctx;
        }

        void reset() noexcept {
            clear_pending();
            next_request_id_ = 1;
            request_count_ = 0;
            response_count_ = 0;
            timeout_count_ = 0;
            queued_count_ = 0;
            drop_count_ = 0;
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

        [[nodiscard]] util::usize request_count() const noexcept {
            return request_count_;
        }

        [[nodiscard]] util::usize response_count() const noexcept {
            return response_count_;
        }

        [[nodiscard]] util::usize timeout_count() const noexcept {
            return timeout_count_;
        }

        [[nodiscard]] util::usize queued_count() const noexcept {
            return queued_count_;
        }

        [[nodiscard]] util::usize drop_count() const noexcept {
            return drop_count_;
        }

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
        }

        [[nodiscard]] bool cancel_request(util::u16 request_id) noexcept {
            auto* pending = find_pending(request_id);
            if (!pending) {
                return false;
            }
            *pending = {};
            return true;
        }

        void tick(util::u32 now_ms) noexcept {
            for (auto& pending : pending_) {
                if (!pending.used || !detail::deadline_expired(now_ms, pending.deadline_ms)) {
                    continue;
                }
                if (pending.on_timeout) {
                    pending.on_timeout(pending, pending.request_id);
                }
            }
        }

        [[nodiscard]] Result<void> consume(const UdpDatagramInfo& info,
                                           OwnedPacket packet) noexcept {
            const auto parsed = Wire::parse_datagram(packet.view());
            if (!parsed || parsed.value().header.kind != Wire::response_kind()) {
                ++drop_count_;
                return {};
            }

            auto* pending = find_pending(parsed.value().header.request_id);
            if (!pending
                || pending->opcode != parsed.value().header.opcode
                || !detail::endpoint_matches(pending->local, info.local)
                || !detail::same_endpoint(pending->peer, info.peer)) {
                ++drop_count_;
                return {};
            }

            if (!pending->on_response) {
                ++drop_count_;
                *pending = {};
                return {};
            }

            pending->on_response(*pending, parsed.value().header.status, parsed.value().payload.payload);
            return {};
        }

        template <ServiceOperation Op>
        [[nodiscard]] Result<util::u16> send_request(const Endpoint& local,
                                                     const Endpoint& peer,
                                                     const typename Op::Request& request,
                                                     util::u32 now_ms,
                                                     util::u32 timeout_ms,
                                                     ResponseFn<Op> on_response = nullptr,
                                                     TimeoutFn<Op> on_timeout = nullptr,
                                                     void* user = nullptr) noexcept {
            static_assert(Op::RequestCodec::max_size() <= MaxPayload);
            static_assert(Op::ResponseCodec::max_size() <= MaxPayload);

            if (!sender_) {
                return util::unexpected(errc::bad_state);
            }

            auto* pending = allocate_pending();
            if (!pending) {
                return util::unexpected(errc::busy);
            }

            auto request_id = allocate_request_id();
            if (!request_id) {
                *pending = {};
                return util::unexpected(request_id.error());
            }

            static constexpr util::usize wire_capacity = MaxPayload + Wire::header_size();
            PacketBuffer<wire_capacity> datagram{};
            auto encoded = Wire::template write_request_datagram<Op>(datagram, request_id.value(), request);
            if (!encoded) {
                *pending = {};
                return util::unexpected(encoded.error());
            }

            pending->used = true;
            pending->owner = this;
            pending->opcode = Op::opcode;
            pending->request_id = request_id.value();
            pending->local = local;
            pending->peer = peer;
            pending->deadline_ms = now_ms + timeout_ms;
            pending->user = user;
            pending->on_response = &Client::dispatch_response<Op>;
            pending->on_timeout = &Client::dispatch_timeout<Op>;
            detail::store_callback(pending->response_callback, on_response);
            detail::store_callback(pending->timeout_callback, on_timeout);

            auto sent = sender_(sender_ctx_, local, peer, datagram.view().payload);
            if (!sent) {
                *pending = {};
                return util::unexpected(sent.error());
            }

            ++request_count_;
            if (sent.value() == UdpSendDisposition::queued) {
                ++queued_count_;
            }
            return request_id;
        }

    private:
        struct PendingCall {
            bool used{false};
            Client* owner{nullptr};
            util::u8 opcode{0};
            util::u16 request_id{0};
            Endpoint local{};
            Endpoint peer{};
            util::u32 deadline_ms{0};
            void* user{nullptr};
            void (*on_response)(PendingCall& pending, Status status, ByteView payload) noexcept{nullptr};
            void (*on_timeout)(PendingCall& pending, util::u16 request_id) noexcept{nullptr};
            detail::CallbackStorage response_callback{};
            detail::CallbackStorage timeout_callback{};
        };

        [[nodiscard]] PendingCall* allocate_pending() noexcept {
            for (auto& pending : pending_) {
                if (pending.used) {
                    continue;
                }
                pending = {};
                return &pending;
            }
            return nullptr;
        }

        [[nodiscard]] PendingCall* find_pending(util::u16 request_id) noexcept {
            for (auto& pending : pending_) {
                if (pending.used && pending.request_id == request_id) {
                    return &pending;
                }
            }
            return nullptr;
        }

        [[nodiscard]] bool request_id_in_use(util::u16 request_id) const noexcept {
            for (const auto& pending : pending_) {
                if (pending.used && pending.request_id == request_id) {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] Result<util::u16> allocate_request_id() noexcept {
            for (util::usize attempts = 0; attempts < 0xFFFFu; ++attempts) {
                auto request_id = next_request_id_++;
                if (request_id == 0u) {
                    request_id = next_request_id_++;
                }
                if (request_id == 0u || request_id_in_use(request_id)) {
                    continue;
                }
                return Result<util::u16>{std::in_place, request_id};
            }
            return util::unexpected(errc::busy);
        }

        template <ServiceOperation Op>
        static void dispatch_response(PendingCall& pending,
                                      Status status,
                                      ByteView payload) noexcept {
            auto* owner = pending.owner;
            const auto callback = detail::load_callback<ResponseFn<Op>>(pending.response_callback);
            void* user = pending.user;
            const auto request_id = pending.request_id;
            pending = {};

            typename Op::Response decoded_value{};
            if (status == Wire::ok_status() || !payload.empty()) {
                auto decoded = Op::ResponseCodec::decode(payload);
                if (!decoded) {
                    if (owner) {
                        ++owner->drop_count_;
                        owner->notify_error(decoded.error());
                    }
                    return;
                }
                decoded_value = decoded.value();
            }

            if (owner) {
                ++owner->response_count_;
            }
            if (callback) {
                callback(user, request_id, status, decoded_value);
                return;
            }
            if (owner && status != Wire::ok_status()) {
                owner->notify_error(Wire::status_error(status));
            }
        }

        template <ServiceOperation Op>
        static void dispatch_timeout(PendingCall& pending,
                                     util::u16 request_id) noexcept {
            auto* owner = pending.owner;
            const auto callback = detail::load_callback<TimeoutFn<Op>>(pending.timeout_callback);
            void* user = pending.user;
            pending = {};

            if (owner) {
                ++owner->timeout_count_;
            }
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

        SendFn sender_{nullptr};
        void* sender_ctx_{nullptr};
        ErrorFn error_{nullptr};
        void* error_ctx_{nullptr};
        util::u16 next_request_id_{1};
        std::array<PendingCall, MaxPending> pending_{};
        util::usize request_count_{0};
        util::usize response_count_{0};
        util::usize timeout_count_{0};
        util::usize queued_count_{0};
        util::usize drop_count_{0};
        errc last_error_{errc::ok};
    };

    template <class Wire, util::usize MaxPayload = 64, util::usize MaxRoutes = 8>
    class Server {
    public:
        using Status = typename Wire::Status;
        using DatagramView = typename Wire::DatagramView;
        using SendFn = Result<UdpSendDisposition> (*)(void* ctx,
                                                      Endpoint local,
                                                      const Endpoint& peer,
                                                      ByteView payload) noexcept;
        using ErrorFn = void (*)(void* ctx, errc error) noexcept;

        template <ServiceOperation Op>
        using RouteFn = Status (*)(void* ctx,
                                   const typename Op::Request& request,
                                   typename Op::Response& response) noexcept;

        void set_sender(SendFn fn, void* ctx) noexcept {
            sender_ = fn;
            sender_ctx_ = ctx;
        }

        void set_error_handler(ErrorFn fn, void* ctx = nullptr) noexcept {
            error_ = fn;
            error_ctx_ = ctx;
        }

        void reset() noexcept {
            request_count_ = 0;
            reply_count_ = 0;
            error_reply_count_ = 0;
            queued_reply_count_ = 0;
            drop_count_ = 0;
            last_error_ = errc::ok;
        }

        [[nodiscard]] util::usize request_count() const noexcept {
            return request_count_;
        }

        [[nodiscard]] util::usize reply_count() const noexcept {
            return reply_count_;
        }

        [[nodiscard]] util::usize error_reply_count() const noexcept {
            return error_reply_count_;
        }

        [[nodiscard]] util::usize queued_reply_count() const noexcept {
            return queued_reply_count_;
        }

        [[nodiscard]] util::usize drop_count() const noexcept {
            return drop_count_;
        }

        [[nodiscard]] util::usize route_count() const noexcept {
            util::usize count = 0;
            for (const auto& binding : bindings_) {
                if (binding.used) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
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

            binding->used = true;
            binding->opcode = Op::opcode;
            binding->owner = this;
            binding->user = ctx;
            binding->on_request = &Server::dispatch_request<Op>;
            detail::store_callback(binding->callback, fn);
            return {};
        }

        template <ServiceOperation Op>
        [[nodiscard]] bool clear_route() noexcept {
            return clear_binding(Op::opcode);
        }

        template <ServiceOperation Op>
        [[nodiscard]] bool has_route() const noexcept {
            return find_binding(Op::opcode) != nullptr;
        }

        [[nodiscard]] Result<void> consume(const UdpDatagramInfo& info,
                                           OwnedPacket packet) noexcept {
            const auto parsed = Wire::parse_datagram(packet.view());
            if (!parsed || parsed.value().header.kind != Wire::request_kind()) {
                ++drop_count_;
                return {};
            }

            ++request_count_;
            auto* binding = find_binding(parsed.value().header.opcode);
            if (!binding || !binding->on_request) {
                return send_error_reply(
                    info,
                    parsed.value().header.opcode,
                    parsed.value().header.request_id,
                    Wire::unsupported_status());
            }

            return binding->on_request(*binding, info, parsed.value());
        }

    protected:
        template <ServiceOperation Op>
        [[nodiscard]] Result<void> send_typed_reply(const UdpDatagramInfo& info,
                                                    util::u16 request_id,
                                                    const typename Op::Response& response,
                                                    Status status = Wire::ok_status()) noexcept {
            static constexpr util::usize wire_capacity = MaxPayload + Wire::header_size();
            PacketBuffer<wire_capacity> datagram{};
            auto encoded = Wire::template write_response_datagram<Op>(
                datagram,
                request_id,
                status,
                response);
            if (!encoded) {
                report_error(encoded.error());
                return util::unexpected(encoded.error());
            }
            return send_payload(info, datagram.view().payload, status);
        }

        [[nodiscard]] Result<void> send_error_reply(const UdpDatagramInfo& info,
                                                    util::u8 opcode,
                                                    util::u16 request_id,
                                                    Status status) noexcept {
            static constexpr util::usize wire_capacity = MaxPayload + Wire::header_size();
            PacketBuffer<wire_capacity> datagram{};
            auto encoded = Wire::write_error_datagram(datagram, opcode, request_id, status);
            if (!encoded) {
                report_error(encoded.error());
                return util::unexpected(encoded.error());
            }
            return send_payload(info, datagram.view().payload, status);
        }

    private:
        struct RouteBinding {
            bool used{false};
            util::u8 opcode{0};
            Server* owner{nullptr};
            void* user{nullptr};
            Result<void> (*on_request)(RouteBinding& binding,
                                       const UdpDatagramInfo& info,
                                       const DatagramView& datagram) noexcept{nullptr};
            detail::CallbackStorage callback{};
        };

        [[nodiscard]] RouteBinding* ensure_binding(util::u8 opcode) noexcept {
            for (auto& binding : bindings_) {
                if (binding.used && binding.opcode == opcode) {
                    return &binding;
                }
            }

            for (auto& binding : bindings_) {
                if (!binding.used) {
                    return &binding;
                }
            }
            return nullptr;
        }

        [[nodiscard]] RouteBinding* find_binding(util::u8 opcode) noexcept {
            for (auto& binding : bindings_) {
                if (binding.used && binding.opcode == opcode) {
                    return &binding;
                }
            }
            return nullptr;
        }

        [[nodiscard]] const RouteBinding* find_binding(util::u8 opcode) const noexcept {
            for (const auto& binding : bindings_) {
                if (binding.used && binding.opcode == opcode) {
                    return &binding;
                }
            }
            return nullptr;
        }

        [[nodiscard]] bool clear_binding(util::u8 opcode) noexcept {
            for (auto& binding : bindings_) {
                if (binding.used && binding.opcode == opcode) {
                    binding = {};
                    return true;
                }
            }
            return false;
        }

        template <ServiceOperation Op>
        static Result<void> dispatch_request(RouteBinding& binding,
                                             const UdpDatagramInfo& info,
                                             const DatagramView& datagram) noexcept {
            auto* owner = binding.owner;
            if (!owner) {
                return util::unexpected(errc::bad_state);
            }

            const auto callback = detail::load_callback<RouteFn<Op>>(binding.callback);
            if (!callback) {
                owner->notify_error(errc::bad_state);
                return owner->send_error_reply(
                    info,
                    Op::opcode,
                    datagram.header.request_id,
                    Wire::internal_error_status());
            }

            auto request = Wire::template decode_request<Op>(datagram);
            if (!request) {
                return owner->send_error_reply(
                    info,
                    Op::opcode,
                    datagram.header.request_id,
                    Wire::bad_request_status());
            }

            typename Op::Response response{};
            const auto status = callback(binding.user, request.value(), response);
            if (status != Wire::ok_status()) {
                return owner->send_error_reply(info, Op::opcode, datagram.header.request_id, status);
            }
            return owner->template send_typed_reply<Op>(info, datagram.header.request_id, response);
        }

        [[nodiscard]] Result<void> send_payload(const UdpDatagramInfo& info,
                                                ByteView payload,
                                                Status status) noexcept {
            if (!sender_) {
                report_error(errc::bad_state);
                return util::unexpected(errc::bad_state);
            }

            auto sent = sender_(sender_ctx_, info.local, info.peer, payload);
            if (!sent) {
                report_error(sent.error());
                return util::unexpected(sent.error());
            }

            ++reply_count_;
            if (status != Wire::ok_status()) {
                ++error_reply_count_;
            }
            if (sent.value() == UdpSendDisposition::queued) {
                ++queued_reply_count_;
            }
            return {};
        }

        void notify_error(errc error) noexcept {
            last_error_ = error;
            if (error_) {
                error_(error_ctx_, error);
            }
        }

        void report_error(errc error) noexcept {
            notify_error(error);
        }

        SendFn sender_{nullptr};
        void* sender_ctx_{nullptr};
        ErrorFn error_{nullptr};
        void* error_ctx_{nullptr};
        std::array<RouteBinding, MaxRoutes> bindings_{};
        util::usize request_count_{0};
        util::usize reply_count_{0};
        util::usize error_reply_count_{0};
        util::usize queued_reply_count_{0};
        util::usize drop_count_{0};
        errc last_error_{errc::ok};
    };
}
