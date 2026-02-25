//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>
#include <optional>

export module input.raw_sampler;

import input.raw;
import input.raw_event;
import input.encoder_decoder;
import input.sampler;
import input.trace;

export namespace input {
    // Raw events only: keep sampling/debounce/repeat, avoid UI semantics.
    class RawSampler {
    public:
        explicit RawSampler(SamplerCfg cfg = {}) noexcept : cfg_(cfg) {}

        // Each poll yields at most 1 raw event.
        template <RawSource RawSource>
        std::optional<RawInputEvent> poll(RawSource& src, std::uint32_t now_ms) noexcept {
            if (auto it = handle_encoder(src, now_ms)) return it;
            if (auto it = handle_button(src, Button::Up, 0, now_ms)) return it;
            if (auto it = handle_button(src, Button::Down, 1, now_ms)) return it;
            if (auto it = handle_button(src, Button::Enter, 2, now_ms)) return it;
            if (auto it = handle_button(src, Button::Back, 3, now_ms)) return it;
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
        std::optional<RawInputEvent> handle_button(RawSource& src, Button b, int idx, std::uint32_t now_ms) noexcept {
            const bool down = debounced_down(src, b, idx, now_ms);
            auto&      st   = btn_[idx];

            if (down && !prev_down_[idx]) {
                prev_down_[idx] = true;
                st.down_since   = now_ms;
                st.last_repeat  = now_ms;
                st.repeating    = false;
                trace::trace_counter_delta(trace::raw_button_id(b), 1);
                return RawInputEvent{
                    .type = RawInputEventType::Button,
                    .ms = now_ms,
                    .button = b,
                    .pressed = true,
                };
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
                        trace::trace_counter_delta(trace::raw_button_id(b), 1);
                        return RawInputEvent{
                            .type = RawInputEventType::Button,
                            .ms = now_ms,
                            .button = b,
                            .pressed = true,
                        };
                    }
                } else {
                    if ((now_ms - st.last_repeat) >= cfg_.repeat.interval_ms) {
                        st.last_repeat = now_ms;
                        trace::trace_counter_delta(trace::raw_button_id(b), 1);
                        return RawInputEvent{
                            .type = RawInputEventType::Button,
                            .ms = now_ms,
                            .button = b,
                            .pressed = true,
                        };
                    }
                }
            }

            return std::nullopt;
        }

        template <class RawSource>
        std::optional<RawInputEvent> handle_pointer(RawSource& src, std::uint32_t now_ms) noexcept {
            const PointerRaw p = src.read_pointer();
            if (p.x < 0 || p.y < 0) {
                if (ptr_prev_down_) {
                    ptr_prev_down_ = false;
                    trace::trace_counter_delta(trace::TraceId::RawPointerUp, 1);
                    return RawInputEvent{
                        .type = RawInputEventType::Pointer,
                        .ms = now_ms,
                        .pointer = PointerRaw{.down = false, .x = ptr_prev_x_, .y = ptr_prev_y_, .id = p.id},
                        .pointer_action = PointerAction::Up,
                    };
                }
                return std::nullopt;
            }

            if (p.down && !ptr_prev_down_) {
                ptr_prev_down_ = true;
                ptr_prev_x_    = p.x;
                ptr_prev_y_    = p.y;
                trace::trace_counter_delta(trace::TraceId::RawPointerDown, 1);
                return RawInputEvent{
                    .type = RawInputEventType::Pointer,
                    .ms = now_ms,
                    .pointer = p,
                    .pointer_action = PointerAction::Down,
                };
            }
            if (p.down && ptr_prev_down_) {
                if (p.x != ptr_prev_x_ || p.y != ptr_prev_y_) {
                    ptr_prev_x_ = p.x;
                    ptr_prev_y_ = p.y;
                    trace::trace_counter_delta(trace::TraceId::RawPointerMove, 1);
                    return RawInputEvent{
                        .type = RawInputEventType::Pointer,
                        .ms = now_ms,
                        .pointer = p,
                        .pointer_action = PointerAction::Move,
                    };
                }
            }
            if (!p.down && ptr_prev_down_) {
                ptr_prev_down_ = false;
                ptr_prev_x_    = p.x;
                ptr_prev_y_    = p.y;
                trace::trace_counter_delta(trace::TraceId::RawPointerUp, 1);
                return RawInputEvent{
                    .type = RawInputEventType::Pointer,
                    .ms = now_ms,
                    .pointer = p,
                    .pointer_action = PointerAction::Up,
                };
            }
            if (!p.down && !ptr_prev_down_) {
                if (p.x != ptr_prev_x_ || p.y != ptr_prev_y_) {
                    ptr_prev_x_ = p.x;
                    ptr_prev_y_ = p.y;
                    trace::trace_counter_delta(trace::TraceId::RawPointerMove, 1);
                    return RawInputEvent{
                        .type = RawInputEventType::Pointer,
                        .ms = now_ms,
                        .pointer = p,
                        .pointer_action = PointerAction::Move,
                    };
                }
            }
            return std::nullopt;
        }

        template <class RawSource>
        std::optional<RawInputEvent> handle_encoder(RawSource& src, std::uint32_t now_ms) noexcept {
            int phases_in_this_poll = 0;

            while (auto ab = src.pop_encoder_ab()) {
                phases_in_this_poll++;
                if (auto d = enc_.update(*ab)) {
                    trace::trace_counter_delta(trace::TraceId::RawEncoder, 1);
                    return RawInputEvent{
                        .type = RawInputEventType::Encoder,
                        .ms = now_ms,
                        .encoder_delta = (std::int16_t)d->delta,
                    };
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
