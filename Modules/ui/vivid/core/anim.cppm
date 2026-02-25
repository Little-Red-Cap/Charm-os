module;
#include <array>
#include <cstddef>
#include <cstdint>
export module charm.core.anim;

export
enum class Ease {
    Linear,
    InOutQuad
};

export
struct Tween {
    float from{0.0f};
    float to{1.0f};
    std::uint32_t start_ms{0};
    std::uint32_t duration_ms{0};
    bool active{false};
};

export
class Timeline {
public:
    using ApplyFn = void(*)(void*, float);
    static constexpr std::size_t kMaxTracks = 16;

    void clear() noexcept { count_ = 0; }

    bool add(ApplyFn fn, void* ctx,
             float from, float to,
             std::uint32_t start_ms,
             std::uint32_t duration_ms,
             Ease ease = Ease::Linear) noexcept {
        if (count_ >= tracks_.size()) return false;
        Track& t = tracks_[count_++];
        t.fn = fn;
        t.ctx = ctx;
        t.ease = ease;
        t.tween.from = from;
        t.tween.to = to;
        t.tween.start_ms = start_ms;
        t.tween.duration_ms = duration_ms;
        t.tween.active = true;
        return true;
    }

    void tick(std::uint32_t now_ms) noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            Track& t = tracks_[i];
            if (!t.tween.active || !t.fn) continue;
            bool finished = false;
            const float v = eval(t.tween, now_ms, t.ease, finished);
            t.fn(t.ctx, v);
            if (finished) {
                t.tween.active = false;
            }
        }
    }

    bool active() const noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            if (tracks_[i].tween.active) return true;
        }
        return false;
    }

private:
    struct Track {
        Tween tween{};
        Ease ease{Ease::Linear};
        ApplyFn fn{nullptr};
        void* ctx{nullptr};
    };

    static float ease_value(Ease e, float t) noexcept {
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        switch (e) {
        case Ease::InOutQuad:
            if (t < 0.5f) return 2.0f * t * t;
            return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
        default:
            return t;
        }
    }

    static float eval(const Tween& t, std::uint32_t now_ms, Ease ease, bool& finished) noexcept {
        finished = false;
        if (!t.active || t.duration_ms == 0) {
            finished = true;
            return t.to;
        }
        const std::uint32_t elapsed = (now_ms > t.start_ms) ? (now_ms - t.start_ms) : 0;
        float u = static_cast<float>(elapsed) / static_cast<float>(t.duration_ms);
        if (u >= 1.0f) {
            u = 1.0f;
            finished = true;
        }
        const float k = ease_value(ease, u);
        return t.from + (t.to - t.from) * k;
    }

    std::array<Track, kMaxTracks> tracks_{};
    std::size_t count_{0};
};
