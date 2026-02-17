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


export namespace input
{
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

    // 只做“通用手势引擎”，不依赖任何平台
    class Sampler {
    public:
        explicit Sampler(SamplerCfg cfg = {}) noexcept : cfg_(cfg) {}

        // 每次 poll 最多生成 1 个 Intent（确定性、简单）
        template <RawSource RawSource>
        std::optional<Intent> poll(RawSource& src, std::uint32_t now_ms) noexcept
        {
            // src.update(now_ms);

            // 0) Encoder -> Adjust
            if (auto it = handle_encoder(src, now_ms)) return it;

            // 1) Buttons -> Nav/Activate/Back
            if (auto it = handle_button(src, Button::Up, 0, now_ms, IntentType::NavPrev)) return it;
            if (auto it = handle_button(src, Button::Down, 1, now_ms, IntentType::NavNext)) return it;
            if (auto it = handle_button(src, Button::Enter, 2, now_ms, IntentType::Activate)) return it;
            if (auto it = handle_button(src, Button::Back, 3, now_ms, IntentType::Back)) return it;

            // 2) Pointer -> Pointer intents（先原样透传，手势识别后续加）
            if (auto it = handle_pointer(src, now_ms)) return it;

            // 3) Axis -> Adjust（先不做，等你接摇杆/编码器再启用）
            // if (auto it = handle_axis(src, now_ms)) return it;

            return std::nullopt;
        }

    private:
        struct BtnState {
            bool          stable{false};   // 去抖后的稳定电平（down?）
            bool          last_raw{false}; // 上一次原始电平
            std::uint32_t raw_since{0};    // 原始电平保持多久

            std::uint32_t down_since{0};
            std::uint32_t last_repeat{0};
            bool          repeating{false};
        };

        template <class RawSource>
        bool debounced_down(RawSource& src, Button b, int idx, std::uint32_t now_ms) noexcept
        {
            const bool raw = src.is_down(b);
            auto&      st  = btn_[idx];

            if (!cfg_.debounce.enabled) {
                st.stable = raw;
                return st.stable;
            }

            if (raw != st.last_raw) {
                st.last_raw  = raw;
                st.raw_since = now_ms;
                // 不立刻改变 stable，等待时间窗
                return st.stable;
            }

            // raw 一直保持
            const std::uint32_t held = now_ms - st.raw_since;
            if (held >= cfg_.debounce.ms) {
                st.stable = raw;
            }
            return st.stable;
        }

        template <class RawSource>
        std::optional<Intent> handle_button(RawSource&    src, Button        b, int idx,
                                            std::uint32_t now_ms, IntentType mapped) noexcept
        {
            const bool down = debounced_down(src, b, idx, now_ms);
            auto&      st   = btn_[idx];

            // 上升沿：触发一次意图
            if (down && !prev_down_[idx]) {
                prev_down_[idx] = true;
                st.down_since   = now_ms;
                st.last_repeat  = now_ms;
                st.repeating    = false;
                return Intent{.type = mapped, .a = 0, .b = 0, .ms = now_ms};
            }

            // 释放
            if (!down && prev_down_[idx]) {
                prev_down_[idx] = false;
                st.repeating    = false;
                return std::nullopt;
            }

            // 长按连发（对 NavPrev/NavNext 特别有用）
            if (cfg_.repeat.enabled && down && prev_down_[idx]) {
                const std::uint32_t held = now_ms - st.down_since;
                if (!st.repeating) {
                    if (held >= cfg_.repeat.delay_ms) {
                        st.repeating   = true;
                        st.last_repeat = now_ms;
                        return Intent{.type = mapped, .a = 0, .b = 0, .ms = now_ms};
                    }
                }
                else {
                    if ((now_ms - st.last_repeat) >= cfg_.repeat.interval_ms) {
                        st.last_repeat = now_ms;
                        return Intent{.type = mapped, .a = 0, .b = 0, .ms = now_ms};
                    }
                }
            }

            return std::nullopt;
        }

        template <class RawSource>
        std::optional<Intent> handle_pointer(RawSource& src, std::uint32_t now_ms) noexcept
        {
            // 若你的 RawSource 没实现 pointer，可让它返回 down=false
            const PointerRaw p = src.read_pointer();
            if (p.x < 0 || p.y < 0) {
                if (ptr_prev_down_) {
                    ptr_prev_down_ = false;
                    return Intent{.type = IntentType::PointerUp, .a = ptr_prev_x_, .b = ptr_prev_y_, .ms = now_ms};
                }
                return std::nullopt;
            }

            // 简单三态：down/move/up（MVP：只做状态变化与坐标变化）
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
        std::optional<Intent> handle_encoder(RawSource& src, std::uint32_t now_ms) noexcept
        {
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
