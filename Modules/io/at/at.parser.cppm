module;

#include <array>
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

    using EventFn = void (*)(void* ctx, const Event& ev) noexcept;

    template <util::usize LineCap>
    class Parser {
    public:
        void set_handler(EventFn fn, void* ctx) noexcept {
            handler_ = fn;
            ctx_ = ctx;
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
            if (handler_) handler_(ctx_, ev);
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
        EventFn handler_{nullptr};
        void* ctx_{nullptr};
    };
}
