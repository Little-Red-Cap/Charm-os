module;
#include <cstdint>
#include <optional>

export module input.nav;

export import input.intent;

export namespace input {

    struct NavResult {
        bool activated{false};
        bool back{false};
        std::int16_t focus_delta{0};
        std::int16_t amount{0};
    };

    [[nodiscard]] inline NavResult nav_from_intent(const std::optional<Intent>& it) noexcept {
        NavResult r{};
        if (!it) return r;

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
                const int a = it->a;
                r.amount = static_cast<std::int16_t>(a);
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
        return r;
    }

    inline void nav_apply_ring(std::int16_t& focus, std::int16_t count, std::int16_t delta) noexcept {
        if (count <= 0 || delta == 0) return;
        int f = static_cast<int>(focus) + static_cast<int>(delta);
        f %= static_cast<int>(count);
        if (f < 0) f += static_cast<int>(count);
        focus = static_cast<std::int16_t>(f);
    }

    inline void nav_apply_clamp(std::int16_t& focus,
                                const std::int16_t count,
                                const std::int16_t delta) noexcept {
        if (count <= 0 || delta == 0) return;
        int f = static_cast<int>(focus) + static_cast<int>(delta);
        if (f < 0) f = 0;
        if (f >= static_cast<int>(count)) f = static_cast<int>(count) - 1;
        focus = static_cast<std::int16_t>(f);
    }

} // namespace input
