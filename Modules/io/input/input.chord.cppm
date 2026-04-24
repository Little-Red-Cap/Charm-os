//
// Created by Joho on 2026/03/05.
//

module;
#include <cstdint>
#include <optional>

export module input.chord;

import input.key;

export namespace input {
    using KeyMask = std::uint32_t;

    enum class ChordEventType : std::uint8_t {
        Start,
        End,
    };

    struct ChordEvent {
        KeyMask      mask{0};
        ChordEventType type{ChordEventType::Start};
        std::uint32_t ms{0};
    };

    struct ChordConfig {
        std::uint16_t window_ms{100};
    };

    class ChordDetector {
    public:
        explicit ChordDetector(ChordConfig cfg = {}) noexcept : cfg_(cfg) {}

        std::optional<ChordEvent> on_key_event(const KeyEvent& ev) noexcept {
            tick(ev.ms);
            const KeyMask key = mask_for(ev.id);
            if (key == 0) {
                return std::nullopt;
            }

            if (ev.type == KeyEventType::Down) {
                down_mask_ |= key;
                if (active_mask_ != 0) {
                    return std::nullopt;
                }
                if (pending_mask_ == 0) {
                    pending_mask_ = key;
                    pending_since_ms_ = ev.ms;
                    return std::nullopt;
                }
                if (elapsed(ev.ms, pending_since_ms_) <= cfg_.window_ms) {
                    active_mask_ = pending_mask_ | key;
                    pending_mask_ = 0;
                    suppress_mask_ = active_mask_;
                    return ChordEvent{.mask = active_mask_, .type = ChordEventType::Start, .ms = ev.ms};
                }
                pending_mask_ = key;
                pending_since_ms_ = ev.ms;
                return std::nullopt;
            }

            if (ev.type == KeyEventType::Up) {
                down_mask_ &= ~key;
                if (active_mask_ != 0 && (active_mask_ & key)) {
                    if ((down_mask_ & active_mask_) == 0) {
                        const KeyMask ended = active_mask_;
                        active_mask_ = 0;
                        suppress_mask_ = 0;
                        return ChordEvent{.mask = ended, .type = ChordEventType::End, .ms = ev.ms};
                    }
                }
                if (pending_mask_ == key) {
                    pending_mask_ = 0;
                }
            }
            return std::nullopt;
        }

        void tick(std::uint32_t now_ms) noexcept {
            if (pending_mask_ == 0 || cfg_.window_ms == 0) {
                return;
            }
            if (elapsed(now_ms, pending_since_ms_) > cfg_.window_ms) {
                pending_mask_ = 0;
            }
        }

        bool suppress(KeyId id) const noexcept {
            return (suppress_mask_ & mask_for(id)) != 0;
        }

        KeyMask active_mask() const noexcept { return active_mask_; }

        void reset() noexcept {
            pending_mask_ = 0;
            active_mask_ = 0;
            suppress_mask_ = 0;
            down_mask_ = 0;
            pending_since_ms_ = 0;
        }

    private:
        static std::uint32_t elapsed(std::uint32_t now, std::uint32_t since) noexcept {
            return now - since;
        }

        static KeyMask mask_for(KeyId id) noexcept {
            if (id >= 32) {
                return 0;
            }
            return static_cast<KeyMask>(1u) << id;
        }

        ChordConfig cfg_{};
        KeyMask pending_mask_{0};
        KeyMask active_mask_{0};
        KeyMask suppress_mask_{0};
        KeyMask down_mask_{0};
        std::uint32_t pending_since_ms_{0};
    };
} // namespace input
