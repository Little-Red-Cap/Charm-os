module;

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string_view>

export module at.parser;

import util.core;
import io.channel;

export namespace at {
    using ByteView = io::ByteView;
    using MutByteView = io::MutByteView;

    enum class EventKind : util::u8 {
        line,
        ok,
        error,
        urc,
    };

    struct Event {
        EventKind kind{EventKind::line};
        std::string_view text{};
    };

    class EventHandlerRef {
    public:
        constexpr EventHandlerRef() noexcept = default;

        static constexpr EventHandlerRef raw(
            void (*handler)(void* ctx, const Event& event) noexcept,
            void* ctx) noexcept {
            return EventHandlerRef{handler, ctx};
        }

        template <typename Handler>
            requires(
                requires(Handler& value, const Event& event) {
                    { value.on_event(event) } noexcept -> std::same_as<void>;
                } ||
                requires(Handler& value, const Event& event) {
                    { value(event) } noexcept -> std::same_as<void>;
                })
        static constexpr EventHandlerRef bind(Handler& handler) noexcept {
            return EventHandlerRef{&invoke<Handler>, &handler};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return handler_ != nullptr;
        }

        void notify(const Event& event) const noexcept {
            if (handler_) {
                handler_(ctx_, event);
            }
        }

    private:
        using HandlerFn = void (*)(void* ctx, const Event& event) noexcept;

        constexpr EventHandlerRef(HandlerFn handler, void* ctx) noexcept
            : handler_(handler),
              ctx_(ctx) {
        }

        template <typename Handler>
        static void invoke(void* ctx, const Event& event) noexcept {
            auto* handler = static_cast<Handler*>(ctx);
            if (!handler) {
                return;
            }
            if constexpr (requires(Handler& value, const Event& ev) {
                              { value.on_event(ev) } noexcept -> std::same_as<void>;
                          }) {
                handler->on_event(event);
            } else {
                (*handler)(event);
            }
        }

        HandlerFn handler_{nullptr};
        void* ctx_{nullptr};
    };

    template <util::usize LineCap>
    class Parser {
    public:
        void set_handler(EventHandlerRef handler = {}) noexcept {
            handler_ = handler;
        }

        void reset() noexcept {
            len_ = 0;
            saw_cr_ = false;
            overflow_ = false;
        }

        void feed(ByteView data) noexcept {
            for (util::usize i = 0; i < data.size(); ++i) {
                const char ch = static_cast<char>(data.data()[i]);
                if (ch == '\r') {
                    saw_cr_ = true;
                    continue;
                }
                if (ch == '\n') {
                    emit_line();
                    saw_cr_ = false;
                    continue;
                }
                if (saw_cr_) {
                    emit_line();
                    saw_cr_ = false;
                }
                if (len_ + 1 < LineCap) {
                    line_[len_++] = ch;
                } else {
                    overflow_ = true;
                }
            }
        }

    private:
        void emit_line() noexcept {
            if (len_ == 0) {
                overflow_ = false;
                return;
            }
            line_[len_] = '\0';
            const std::string_view view{line_.data(), len_};
            Event ev{};
            ev.kind = classify(view);
            ev.text = view;
            handler_.notify(ev);
            len_ = 0;
            overflow_ = false;
        }

        static EventKind classify(std::string_view line) noexcept {
            if (line == "OK") return EventKind::ok;
            if (line == "ERROR") return EventKind::error;
            if (!line.empty() && line.front() == '+') return EventKind::urc;
            return EventKind::line;
        }

        std::array<char, LineCap> line_{};
        util::usize len_{0};
        bool saw_cr_{false};
        bool overflow_{false};
        EventHandlerRef handler_{};
    };
}
