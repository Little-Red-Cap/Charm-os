module;
#include <chrono>
#include <cstddef>
export module charm.core.input_interaction;

export import charm.core.event;

export
class InteractionStrategy {
public:
    virtual ~InteractionStrategy() = default;
    virtual bool on_event(const Event& e) = 0;
};

export
class DoubleTapRestoreStrategy : public InteractionStrategy {
public:
    using Callback = void(*)(void*);

    void set_callback(Callback fn, void* ctx) noexcept {
        callback_ = fn;
        ctx_ = ctx;
    }

    void set_enabled(bool on) noexcept { enabled_ = on; }

    void set_threshold(int ms, int radius_px) noexcept {
        if (ms < 0) ms = 0;
        if (radius_px < 0) radius_px = 0;
        double_tap_ms_ = ms;
        double_tap_dist_sq_ = radius_px * radius_px;
    }

    bool on_event(const Event& e) override {
        if (!enabled_) return false;
        if (e.type != Event::Type::Click) return false;
        if (!is_double_tap(e.x, e.y)) return false;
        if (callback_) callback_(ctx_);
        return true;
    }

private:
    bool is_double_tap(int x, int y) {
        const auto now = std::chrono::steady_clock::now();
        bool is_double = false;
        if (double_tap_ms_ > 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_tap_time_).count();
            const int dx = x - last_tap_x_;
            const int dy = y - last_tap_y_;
            const int dist_sq = dx * dx + dy * dy;
            if (elapsed >= 0 && elapsed <= double_tap_ms_ && dist_sq <= double_tap_dist_sq_) {
                is_double = true;
            }
        }
        last_tap_time_ = now;
        last_tap_x_ = x;
        last_tap_y_ = y;
        return is_double;
    }

    Callback callback_{nullptr};
    void* ctx_{nullptr};
    bool enabled_{true};
    int double_tap_ms_{300};
    int double_tap_dist_sq_{144};
    int last_tap_x_{0};
    int last_tap_y_{0};
    std::chrono::steady_clock::time_point last_tap_time_{};
};

export
class PinchScrollStrategy : public InteractionStrategy {
public:
    using BeginFn = void(*)(void*);
    using UpdateFn = void(*)(void*, int);
    using EndFn = void(*)(void*);

    void set_callbacks(BeginFn begin_fn, UpdateFn update_fn, EndFn end_fn, void* ctx) noexcept {
        begin_ = begin_fn;
        update_ = update_fn;
        end_ = end_fn;
        ctx_ = ctx;
    }

    void set_enabled(bool on) noexcept { enabled_ = on; }

    bool on_event(const Event& e) override {
        if (!enabled_) return false;
        if (e.type != Event::Type::GesturePinch) return false;
        if (e.gesture_phase == Event::GesturePhase::Begin) {
            if (begin_) begin_(ctx_);
        } else if (e.gesture_phase == Event::GesturePhase::Update) {
            if (update_) update_(ctx_, e.dy);
        } else if (e.gesture_phase == Event::GesturePhase::End) {
            if (end_) end_(ctx_);
        }
        return true;
    }

private:
    BeginFn begin_{nullptr};
    UpdateFn update_{nullptr};
    EndFn end_{nullptr};
    void* ctx_{nullptr};
    bool enabled_{true};
};
