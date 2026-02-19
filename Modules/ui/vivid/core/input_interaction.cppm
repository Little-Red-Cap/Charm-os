module;
#include <chrono>
#include <cstddef>
#include <cstdint>
export module charm.core.input_interaction;

export import charm.core.event;

export
class InteractionStrategy {
public:
    virtual ~InteractionStrategy() = default;
    virtual bool on_event(const Event& e) = 0;
};

export
class InteractionList {
public:
    using EventMask = std::uint64_t;
    static constexpr std::size_t kMax = 4;
    static constexpr EventMask kAll = ~EventMask{0};

    static constexpr EventMask mask(Event::Type type) noexcept {
        return (static_cast<unsigned>(type) < 64)
            ? (EventMask{1} << static_cast<unsigned>(type))
            : EventMask{0};
    }

    bool add(InteractionStrategy* strategy, EventMask mask = kAll) noexcept {
        if (!strategy) return false;
        for (std::size_t i = 0; i < count_; ++i) {
            if (items_[i] == strategy) return true;
        }
        if (count_ >= kMax) return false;
        items_[count_++] = strategy;
        masks_[count_ - 1] = mask;
        return true;
    }

    bool remove(InteractionStrategy* strategy) noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            if (items_[i] == strategy) {
                for (std::size_t j = i + 1; j < count_; ++j) {
                    items_[j - 1] = items_[j];
                    masks_[j - 1] = masks_[j];
                }
                items_[count_ - 1] = nullptr;
                masks_[count_ - 1] = 0;
                --count_;
                return true;
            }
        }
        return false;
    }

    bool on_event(const Event& e) {
        const auto event_mask = mask(e.type);
        for (std::size_t i = 0; i < count_; ++i) {
            auto* strategy = items_[i];
            if (!strategy) continue;
            if ((masks_[i] & event_mask) == 0) continue;
            if (strategy->on_event(e)) {
                return true;
            }
        }
        return false;
    }

private:
    InteractionStrategy* items_[kMax]{};
    EventMask masks_[kMax]{};
    std::size_t count_{0};
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
