module;

#include <array>

export module net.request_session;

import net.common;
import net.frame_session;
import util.core;
import util.error;
import util.expected;

export namespace net {
    template <util::usize MaxPayload, util::usize MaxPending = 4>
    class RequestSession {
    public:
        using ResponseFn = void (*)(void* ctx,
                                    util::u16 request_id,
                                    util::u8 opcode,
                                    bool ok,
                                    ByteView payload) noexcept;
        using TimeoutFn = void (*)(void* ctx,
                                   util::u16 request_id,
                                   util::u8 opcode) noexcept;
        using RequestFn = void (*)(void* ctx,
                                   RequestSession& session,
                                   util::u16 request_id,
                                   util::u8 opcode,
                                   ByteView payload) noexcept;
        using ErrorFn = NetErrorFn;

        void set_sender(StreamSenderRef sender = {}) noexcept {
            frame_.set_sender(sender);
        }

        void set_sender(FrameSendFn fn, void* ctx) noexcept {
            set_sender(StreamSenderRef::raw(fn, ctx));
        }

        void set_request_handler(RequestFn fn, void* ctx) noexcept {
            request_ = fn;
            request_ctx_ = ctx;
        }

        void set_error_handler(NetErrorHandlerRef handler = {}) noexcept {
            error_ = handler;
        }

        void set_error_handler(ErrorFn fn, void* ctx) noexcept {
            set_error_handler(NetErrorHandlerRef::raw(fn, ctx));
        }

        void reset() noexcept {
            frame_.reset();
            clear_pending();
            clear_ignored();
            next_request_id_ = 1;
            last_error_ = errc::ok;
            transport_error_ = errc::ok;
        }

        [[nodiscard]] bool has_pending() const noexcept {
            for (const auto& pending : pending_) {
                if (pending.used) return true;
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

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
        }

        void feed(ByteView data) noexcept {
            frame_.feed(data);
        }

        void notify_writable() noexcept {
            frame_.notify_writable();
        }

        [[nodiscard]] Result<util::u16> send_request(util::u8 opcode,
                                                     ByteView payload,
                                                     util::u32 now_ms,
                                                     util::u32 timeout_ms,
                                                     ResponseFn on_response,
                                                     TimeoutFn on_timeout,
                                                     void* user = nullptr) noexcept {
            auto terminal = terminal_error();
            if (terminal != errc::ok) {
                return util::unexpected(terminal);
            }
            if (timeout_ms == 0) {
                return util::unexpected(errc::invalid_arg);
            }
            if (payload.size() > MaxPayload) {
                return util::unexpected(errc::buffer_overflow);
            }

            auto* pending = allocate_pending();
            if (!pending) {
                return util::unexpected(errc::busy);
            }

            pending->request_id = allocate_request_id();
            pending->opcode = opcode;
            pending->start_ms = now_ms;
            pending->timeout_ms = timeout_ms;
            pending->on_response = on_response;
            pending->on_timeout = on_timeout;
            pending->user = user;

            auto sent = send_message(pending->request_id, opcode, 0u, payload);
            if (!sent) {
                *pending = {};
                return util::unexpected(sent.error());
            }
            return pending->request_id;
        }

        [[nodiscard]] Result<void> send_response(util::u16 request_id,
                                                 util::u8 opcode,
                                                 ByteView payload,
                                                 bool ok = true) noexcept {
            auto terminal = terminal_error();
            if (terminal != errc::ok) {
                return util::unexpected(terminal);
            }
            if (payload.size() > MaxPayload) {
                return util::unexpected(errc::buffer_overflow);
            }
            util::u8 flags = kFlagResponse;
            if (!ok) {
                flags |= kFlagError;
            }
            return send_message(request_id, opcode, flags, payload);
        }

        [[nodiscard]] Result<void> send_error(util::u16 request_id,
                                              util::u8 opcode,
                                              ByteView payload = {}) noexcept {
            return send_response(request_id, opcode, payload, false);
        }

        [[nodiscard]] bool cancel_request(util::u16 request_id) noexcept {
            if (request_id == 0) {
                return false;
            }

            for (auto& pending : pending_) {
                if (!pending.used || pending.request_id != request_id) continue;
                remember_ignored(pending.request_id, pending.opcode);
                pending = {};
                return true;
            }
            return false;
        }

        void cancel_all_requests() noexcept {
            for (auto& pending : pending_) {
                if (!pending.used) continue;
                remember_ignored(pending.request_id, pending.opcode);
                pending = {};
            }
        }

        void tick(util::u32 now_ms) noexcept {
            for (auto& pending : pending_) {
                if (!pending.used) continue;
                if ((now_ms - pending.start_ms) < pending.timeout_ms) continue;

                const auto request_id = pending.request_id;
                const auto opcode = pending.opcode;
                auto timeout = pending.on_timeout;
                void* user = pending.user;
                remember_ignored(request_id, opcode);
                pending = {};

                if (timeout) {
                    timeout(user, request_id, opcode);
                } else {
                    notify_error(errc::timeout);
                }
            }
        }

        void on_transport_closed() noexcept {
            transport_error_ = errc::closed;
            last_error_ = errc::closed;
            clear_pending();
            clear_ignored();
            notify_error(last_error_);
        }

        void on_transport_error(errc error) noexcept {
            transport_error_ = error;
            last_error_ = error;
            clear_pending();
            clear_ignored();
            notify_error(error);
        }

        RequestSession() {
            frame_.set_frame_handler(FrameHandlerRef::raw(&RequestSession::on_frame_trampoline, this));
            frame_.set_error_handler(FrameErrorHandlerRef::raw(&RequestSession::on_frame_error_trampoline, this));
        }

    private:
        static constexpr util::usize kWireHeaderSize = 4;
        static constexpr util::u8 kFlagResponse = 0x01u;
        static constexpr util::u8 kFlagError = 0x02u;
        using WireSession = FrameSession<MaxPayload + kWireHeaderSize>;

        struct Pending {
            bool used{false};
            util::u16 request_id{0};
            util::u8 opcode{0};
            util::u32 start_ms{0};
            util::u32 timeout_ms{0};
            ResponseFn on_response{nullptr};
            TimeoutFn on_timeout{nullptr};
            void* user{nullptr};
        };

        struct IgnoredResponse {
            bool used{false};
            util::u16 request_id{0};
            util::u8 opcode{0};
        };

        [[nodiscard]] Pending* allocate_pending() noexcept {
            for (auto& pending : pending_) {
                if (pending.used) continue;
                pending = {};
                pending.used = true;
                return &pending;
            }
            return nullptr;
        }

        [[nodiscard]] util::u16 allocate_request_id() noexcept {
            const auto id = next_request_id_;
            ++next_request_id_;
            if (next_request_id_ == 0) {
                next_request_id_ = 1;
            }
            return id == 0 ? allocate_request_id() : id;
        }

        [[nodiscard]] Result<void> send_message(util::u16 request_id,
                                                util::u8 opcode,
                                                util::u8 flags,
                                                ByteView payload) noexcept {
            auto terminal = terminal_error();
            if (terminal != errc::ok) {
                return util::unexpected(terminal);
            }
            if (request_id == 0) {
                return util::unexpected(errc::invalid_arg);
            }
            if (payload.size() > MaxPayload) {
                return util::unexpected(errc::buffer_overflow);
            }

            std::array<util::u8, MaxPayload + kWireHeaderSize> wire{};
            wire[0] = static_cast<util::u8>((request_id >> 8) & 0xffu);
            wire[1] = static_cast<util::u8>(request_id & 0xffu);
            wire[2] = opcode;
            wire[3] = flags;
            for (util::usize i = 0; i < payload.size(); ++i) {
                wire[kWireHeaderSize + i] = payload.data()[i];
            }
            return frame_.send_frame(ByteView{wire.data(), payload.size() + kWireHeaderSize});
        }

        static void on_frame_trampoline(void* ctx, ByteView payload) noexcept {
            auto* self = static_cast<RequestSession*>(ctx);
            if (self) {
                self->on_frame(payload);
            }
        }

        static void on_frame_error_trampoline(void* ctx, errc error) noexcept {
            auto* self = static_cast<RequestSession*>(ctx);
            if (self) {
                self->last_error_ = error;
                self->notify_error(error);
            }
        }

        void on_frame(ByteView payload) noexcept {
            if (payload.size() < kWireHeaderSize) {
                last_error_ = errc::format_error;
                notify_error(last_error_);
                return;
            }

            const util::u16 request_id = static_cast<util::u16>(
                (static_cast<util::u16>(payload.data()[0]) << 8)
                | static_cast<util::u16>(payload.data()[1]));
            const util::u8 opcode = payload.data()[2];
            const util::u8 flags = payload.data()[3];
            const ByteView body{payload.data() + kWireHeaderSize, payload.size() - kWireHeaderSize};

            if ((flags & kFlagResponse) != 0u) {
                on_response(request_id, opcode, (flags & kFlagError) == 0u, body);
                return;
            }

            if (!request_) {
                last_error_ = errc::not_supported;
                notify_error(last_error_);
                return;
            }
            request_(request_ctx_, *this, request_id, opcode, body);
        }

        void on_response(util::u16 request_id,
                         util::u8 opcode,
                         bool ok,
                         ByteView payload) noexcept {
            for (auto& pending : pending_) {
                if (!pending.used) continue;
                if (pending.request_id != request_id) continue;

                const auto expected_opcode = pending.opcode;
                auto response = pending.on_response;
                void* user = pending.user;
                pending = {};

                if (expected_opcode != opcode) {
                    last_error_ = errc::format_error;
                    notify_error(last_error_);
                    return;
                }
                if (response) {
                    response(user, request_id, opcode, ok, payload);
                }
                return;
            }

            if (consume_ignored(request_id, opcode)) {
                return;
            }

            last_error_ = errc::noent;
            notify_error(last_error_);
        }

        void remember_ignored(util::u16 request_id, util::u8 opcode) noexcept {
            if (request_id == 0 || ignored_.empty()) {
                return;
            }

            ignored_[next_ignored_] = IgnoredResponse{true, request_id, opcode};
            ++next_ignored_;
            if (next_ignored_ >= ignored_.size()) {
                next_ignored_ = 0;
            }
        }

        [[nodiscard]] bool consume_ignored(util::u16 request_id, util::u8 opcode) noexcept {
            for (auto& ignored : ignored_) {
                if (!ignored.used) continue;
                if (ignored.request_id != request_id || ignored.opcode != opcode) continue;
                ignored = {};
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

        [[nodiscard]] errc terminal_error() const noexcept {
            return transport_error_;
        }

        void notify_error(errc error) noexcept {
            error_.notify(error);
        }

        WireSession frame_{};
        std::array<Pending, MaxPending> pending_{};
        std::array<IgnoredResponse, MaxPending> ignored_{};
        util::usize next_ignored_{0};
        RequestFn request_{nullptr};
        void* request_ctx_{nullptr};
        NetErrorHandlerRef error_{};
        util::u16 next_request_id_{1};
        errc last_error_{errc::ok};
        errc transport_error_{errc::ok};
    };
}
