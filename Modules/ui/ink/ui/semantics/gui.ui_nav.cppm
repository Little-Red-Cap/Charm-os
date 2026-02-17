// Intent -> 导航语义（焦点移动 / 激活 / 返回）
// 设计目标：
// - 页面逻辑不关心“滚轮/按键是什么输入源”，只关心导航语义。
// - 支持环形焦点（Ring）与可选的 Clamp 行为。

module;
#include <cstdint>
#include <optional>

export module gui.ui_nav;

import input.intent;

export namespace gui {

    struct NavResult {
        bool activated{false};   // 本帧是否触发 Activate
        bool back{false};        // 本帧是否触发 Back
        std::int16_t focus_delta{0}; // 焦点变化（通常 -1/ +1）

        // 对“调节”类输入（例如编码器）有时希望额外保留幅度
        // （比如后续做加速滚动/步进），这里先预留。
        std::int16_t amount{0};
    };

    // 将 Intent 映射为“导航语义”
    [[nodiscard]] inline NavResult nav_from_intent(const std::optional<input::Intent>& it) noexcept {
        NavResult r{};
        if (!it) return r;

        using input::IntentType;
#if 0
        switch (it->type) {
        case IntentType::NavPrev:   r.focus_delta = -1; break;
        case IntentType::NavNext:   r.focus_delta = +1; break;
        case IntentType::Adjust:    r.focus_delta = (it->a > 0) ? +1 : (it->a < 0 ? -1 : 0); break;
        case IntentType::Activate:  r.activated = true; break;
        case IntentType::Back:      r.back = true; break;
        default: break;
        }
#else
        switch (it->type) {
        case IntentType::NavPrev:
            r.focus_delta = -1;
            r.amount = -1;
            break;
        case IntentType::NavNext:
            r.focus_delta = +1;
            r.amount = +1;
            break;
        case IntentType::Adjust: {
                // 约定：it->a 为有符号步进
                const int a = it->a;
                r.amount = (std::int16_t)a;
                r.focus_delta = (a > 0) ? +1 : (std::int16_t)(a < 0 ? -1 : 0);
        } break;
        case IntentType::Activate:
            r.activated = true;
            break;
        case IntentType::Back:
            r.back = true;
            break;
        default:
            break;
        }
#endif
        return r;
    }

    // 对焦点索引做环形移动：0..count-1
    // 例：focus=0, delta=-1, count=3 -> 2
    inline void nav_apply_ring(std::int16_t& focus, std::int16_t count, std::int16_t delta) noexcept {
        if (count <= 0 || delta == 0) return;
        int f = (int)focus + (int)delta;
        // while (f < 0) f += count;
        // while (f >= count) f -= count;

        // 处理大跨度（未来可能来自加速滚动）
        f %= (int)count;
        if (f < 0) f += (int)count;
        focus = (std::int16_t)f;
    }

    // 夹紧移动焦点：0..count-1
    inline void nav_apply_clamp(std::int16_t& focus,
    const std::int16_t count,
    const std::int16_t delta) noexcept {
        if (count <= 0 || delta == 0) return;
        int f = (int)focus + (int)delta;
        if (f < 0) f = 0;
        if (f >= (int)count) f = (int)count - 1;
        focus = (std::int16_t)f;
    }

} // namespace gui
