module;
#include <cstddef>
export module charm.widgets.number_input;

import charm.widgets.text_input;
import charm.core.event;

export
class NumberInput : public TextInput {
public:
    NumberInput() = default;

    void set_allow_decimal(bool on) noexcept { allow_decimal_ = on; }
    void set_allow_negative(bool on) noexcept { allow_negative_ = on; }

    bool on_event(const Event& e) override {
        if (e.type == Event::Type::KeyDown && !is_readonly()) {
            if (e.ch >= 32 && e.ch <= 126) {
                const char c = static_cast<char>(e.ch);
                if (!accept_char(c)) return true;
            }
        }
        return TextInput::on_event(e);
    }

private:
    bool allow_decimal_{false};
    bool allow_negative_{true};

    bool has_char(char needle) const noexcept {
        for (int i = 0; i < len_; ++i) {
            if (buf_[i] == needle) return true;
        }
        return false;
    }

    bool accept_char(char c) const noexcept {
        if (c >= '0' && c <= '9') return true;
        if (c == '-' && allow_negative_) {
            return (cursor_ == 0) && !has_char('-');
        }
        if (c == '.' && allow_decimal_) {
            return !has_char('.');
        }
        return false;
    }
};
