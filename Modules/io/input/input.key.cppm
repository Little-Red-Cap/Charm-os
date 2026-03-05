//
// Created by Joho on 2026/03/05.
//

module;
#include <array>
#include <cstdint>
#include <optional>

export module input.key;

export namespace input {
    using KeyId = std::uint8_t;

    enum class KeyEventType : std::uint8_t {
        Down,
        Up,
        Click,
        DoubleClick,
        LongPress,
        LongHold,
    };

    struct KeyEvent {
        KeyId         id{0};
        KeyEventType  type{KeyEventType::Down};
        std::uint32_t ms{0};
    };

    struct KeyConfig {
        std::uint16_t debounce_ms{20};
        std::uint16_t double_click_ms{250};
        std::uint16_t long_press_ms{600};
        std::uint16_t long_hold_repeat_ms{0};
    };

    class KeyEngine {
    public:
        explicit KeyEngine(KeyId id, KeyConfig cfg = {}) noexcept : id_(id), cfg_(cfg) {}

        std::optional<KeyEvent> update(bool raw_down, std::uint32_t now_ms) noexcept {
            if (pending_count_ > 0) {
                return pop_pending();
            }

            const bool prev_down = stable_down_;
            const bool stable_down = debounce(raw_down, now_ms);

            if (state_ == State::WaitSecond && !stable_down) {
                if (cfg_.double_click_ms == 0 || elapsed(now_ms, first_release_ms_) >= cfg_.double_click_ms) {
                    state_ = State::Idle;
                    enqueue(KeyEventType::Click, now_ms);
                    return pop_pending();
                }
            }

            if (stable_down != prev_down) {
                if (stable_down) {
                    on_down(now_ms);
                } else {
                    on_up(now_ms);
                }
                if (pending_count_ > 0) {
                    return pop_pending();
                }
            }

            if (stable_down) {
                on_hold(now_ms);
                if (pending_count_ > 0) {
                    return pop_pending();
                }
            }

            return std::nullopt;
        }

        bool is_down() const noexcept { return stable_down_; }
        KeyId id() const noexcept { return id_; }

        void reset() noexcept {
            state_ = State::Idle;
            stable_down_ = false;
            last_raw_ = false;
            raw_since_ = 0;
            down_since_ = 0;
            first_release_ms_ = 0;
            last_hold_ms_ = 0;
            long_press_fired_ = false;
            pending_head_ = 0;
            pending_count_ = 0;
        }

    private:
        enum class State : std::uint8_t {
            Idle,
            Pressed,
            WaitSecond,
            SecondPressed,
        };

        static std::uint32_t elapsed(std::uint32_t now, std::uint32_t since) noexcept {
            return now - since;
        }

        bool debounce(bool raw_down, std::uint32_t now_ms) noexcept {
            if (cfg_.debounce_ms == 0) {
                stable_down_ = raw_down;
                last_raw_ = raw_down;
                raw_since_ = now_ms;
                return stable_down_;
            }

            if (raw_down != last_raw_) {
                last_raw_ = raw_down;
                raw_since_ = now_ms;
                return stable_down_;
            }

            if (elapsed(now_ms, raw_since_) >= cfg_.debounce_ms) {
                stable_down_ = last_raw_;
            }
            return stable_down_;
        }

        void on_down(std::uint32_t now_ms) noexcept {
            enqueue(KeyEventType::Down, now_ms);
            down_since_ = now_ms;
            last_hold_ms_ = now_ms;
            long_press_fired_ = false;

            if (state_ == State::WaitSecond) {
                if (cfg_.double_click_ms > 0 &&
                    elapsed(now_ms, first_release_ms_) <= cfg_.double_click_ms) {
                    state_ = State::SecondPressed;
                    return;
                }
            }
            state_ = State::Pressed;
        }

        void on_up(std::uint32_t now_ms) noexcept {
            enqueue(KeyEventType::Up, now_ms);

            if (long_press_fired_) {
                state_ = State::Idle;
                return;
            }

            if (state_ == State::SecondPressed) {
                if (cfg_.double_click_ms > 0 &&
                    elapsed(now_ms, first_release_ms_) <= cfg_.double_click_ms) {
                    enqueue(KeyEventType::DoubleClick, now_ms);
                    state_ = State::Idle;
                    return;
                }
            }

            if (cfg_.double_click_ms > 0) {
                state_ = State::WaitSecond;
                first_release_ms_ = now_ms;
            } else {
                enqueue(KeyEventType::Click, now_ms);
                state_ = State::Idle;
            }
        }

        void on_hold(std::uint32_t now_ms) noexcept {
            if (!long_press_fired_ && cfg_.long_press_ms > 0 &&
                elapsed(now_ms, down_since_) >= cfg_.long_press_ms) {
                enqueue(KeyEventType::LongPress, now_ms);
                long_press_fired_ = true;
                last_hold_ms_ = now_ms;
                return;
            }

            if (long_press_fired_ && cfg_.long_hold_repeat_ms > 0 &&
                elapsed(now_ms, last_hold_ms_) >= cfg_.long_hold_repeat_ms) {
                enqueue(KeyEventType::LongHold, now_ms);
                last_hold_ms_ = now_ms;
            }
        }

        void enqueue(KeyEventType type, std::uint32_t now_ms) noexcept {
            if (pending_count_ >= pending_.size()) {
                return;
            }
            const std::size_t idx = (pending_head_ + pending_count_) % pending_.size();
            pending_[idx] = KeyEvent{.id = id_, .type = type, .ms = now_ms};
            pending_count_++;
        }

        std::optional<KeyEvent> pop_pending() noexcept {
            if (pending_count_ == 0) {
                return std::nullopt;
            }
            const KeyEvent ev = pending_[pending_head_];
            pending_head_ = (pending_head_ + 1) % pending_.size();
            pending_count_--;
            return ev;
        }

        KeyId id_{0};
        KeyConfig cfg_{};

        State state_{State::Idle};
        bool stable_down_{false};
        bool last_raw_{false};
        std::uint32_t raw_since_{0};
        std::uint32_t down_since_{0};
        std::uint32_t first_release_ms_{0};
        std::uint32_t last_hold_ms_{0};
        bool long_press_fired_{false};

        std::array<KeyEvent, 4> pending_{};
        std::size_t pending_head_{0};
        std::size_t pending_count_{0};
    };
} // namespace input
