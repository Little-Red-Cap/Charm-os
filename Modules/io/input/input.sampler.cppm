//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>
#include <optional>

export module input.sampler;

import input.raw;
import input.intent;
import input.encoder_decoder;

export namespace input {
    struct DebounceCfg {
        bool          enabled{true};
        std::uint16_t ms{20};
    };

    struct RepeatCfg {
        bool          enabled{true};
        std::uint16_t delay_ms{350};
        std::uint16_t interval_ms{120};
    };

    struct SamplerCfg {
        DebounceCfg debounce{};
        RepeatCfg   repeat{};
    };

    // Intent sampler: maps raw inputs to UI intents without UI semantics.
    class Sampler {
    public:
        explicit Sampler(SamplerCfg cfg = {}) noexcept : cfg_(cfg) {}

        // Each poll yields at most 1 intent.
        template <RawSource RawSource>
        std::optional<Intent> poll(RawSource& src, std::uint32_t now_ms) noexcept {
            if (auto it = handle_encoder(src, now_ms)) return it;
            if (auto it = handle_button(src, Button::Up, 0, now_ms, IntentType::NavPrev)) return it;
            if (auto it = handle_button(src, Button::Down, 1, now_ms, IntentType::NavNext)) return it;
            if (auto it = handle_button(src, Button::Enter, 2, now_ms, IntentType::Activate)) return it;
            if (auto it = handle_button(src, Button::Back, 3, now_ms, IntentType::Back)) return it;
            if (auto it = handle_pointer(src, now_ms)) return it;
            return std::nullopt;
        }

    private:
        struct BtnState {
            bool          stable{false};
            bool          last_raw{false};
            std::uint32_t raw_since{0};

            std::uint32_t down_since{0};
            std::uint32_t last_repeat{0};
            bool          repeating{false};
        };

        template <class RawSource>
        bool debounced_down(RawSource& src, Button b, int idx, std::uint32_t now_ms) noexcept {
            const bool raw = src.is_down(b);
            auto&      st  = btn_[idx];

            if (!cfg_.debounce.enabled) {
                st.stable = raw;
                return st.stable;
            }

            if (raw != st.last_raw) {
                st.last_raw  = raw;
                st.raw_since = now_ms;
                return st.stable;
            }

            const std::uint32_t held = now_ms - st.raw_since;
            if (held >= cfg_.debounce.ms) {
                st.stable = raw;
            }
            return st.stable;
        }

        template <class RawSource>
        std::optional<Intent> handle_button(RawSource& src, Button b, int idx,
                                            std::uint32_t now_ms, IntentType mapped) noexcept {
            const bool down = debounced_down(src, b, idx, now_ms);
            auto&      st   = btn_[idx];

            if (down && !prev_down_[idx]) {
                prev_down_[idx] = true;
                st.down_since   = now_ms;
                st.last_repeat  = now_ms;
                st.repeating    = false;
                return Intent{.type = mapped, .a = 0, .b = 0, .ms = now_ms};
            }

            if (!down && prev_down_[idx]) {
                prev_down_[idx] = false;
                st.repeating    = false;
                return std::nullopt;
            }

            if (cfg_.repeat.enabled && down && prev_down_[idx]) {
                const std::uint32_t held = now_ms - st.down_since;
                if (!st.repeating) {
                    if (held >= cfg_.repeat.delay_ms) {
                        st.repeating   = true;
                        st.last_repeat = now_ms;
                        return Intent{.type = mapped, .a = 0, .b = 0, .ms = now_ms};
                    }
                } else {
                    if ((now_ms - st.last_repeat) >= cfg_.repeat.interval_ms) {
                        st.last_repeat = now_ms;
                        return Intent{.type = mapped, .a = 0, .b = 0, .ms = now_ms};
                    }
                }
            }

            return std::nullopt;
        }

        template <class RawSource>
        std::optional<Intent> handle_pointer(RawSource& src, std::uint32_t now_ms) noexcept {
            const PointerRaw p = src.read_pointer();
            if (p.x < 0 || p.y < 0) {
                if (ptr_prev_down_) {
                    ptr_prev_down_ = false;
                    return Intent{.type = IntentType::PointerUp,
                                  .a = ptr_prev_x_, .b = ptr_prev_y_, .ms = now_ms};
                }
                return std::nullopt;
            }

            if (p.down && !ptr_prev_down_) {
                ptr_prev_down_ = true;
                ptr_prev_x_    = p.x;
                ptr_prev_y_    = p.y;
                return Intent{.type = IntentType::PointerDown, .a = p.x, .b = p.y, .ms = now_ms};
            }
            if (p.down && ptr_prev_down_) {
                if (p.x != ptr_prev_x_ || p.y != ptr_prev_y_) {
                    ptr_prev_x_ = p.x;
                    ptr_prev_y_ = p.y;
                    return Intent{.type = IntentType::PointerMove, .a = p.x, .b = p.y, .ms = now_ms};
                }
            }
            if (!p.down && ptr_prev_down_) {
                ptr_prev_down_ = false;
                ptr_prev_x_    = p.x;
                ptr_prev_y_    = p.y;
                return Intent{.type = IntentType::PointerUp, .a = p.x, .b = p.y, .ms = now_ms};
            }
            if (!p.down && !ptr_prev_down_) {
                if (p.x != ptr_prev_x_ || p.y != ptr_prev_y_) {
                    ptr_prev_x_ = p.x;
                    ptr_prev_y_ = p.y;
                    return Intent{.type = IntentType::PointerMove, .a = p.x, .b = p.y, .ms = now_ms};
                }
            }
            return std::nullopt;
        }

        template <class RawSource>
        std::optional<Intent> handle_encoder(RawSource& src, std::uint32_t now_ms) noexcept {
            int phases_in_this_poll = 0;

            while (auto ab = src.pop_encoder_ab()) {
                phases_in_this_poll++;
                if (auto d = enc_.update(*ab)) {
                    return Intent{.type = IntentType::Adjust, .a = d->delta, .b = 0, .ms = now_ms};
                }
            }

            if (phases_in_this_poll > 0) {
            }
            return std::nullopt;
        }

        SamplerCfg cfg_{};
        BtnState   btn_[4]{};
        bool       prev_down_[4]{false, false, false, false};

        bool         ptr_prev_down_{false};
        std::int16_t ptr_prev_x_{0}, ptr_prev_y_{0};

        EncoderDecoder enc_{};
    };
} // namespace input
