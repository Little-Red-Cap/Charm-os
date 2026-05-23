#pragma once

#include "capabilities/input.hpp"

#include <cstdint>
#include <string_view>

namespace h747::apps::player {

struct PlayerViewModel {
    std::string_view title{"Charm Player"};
    std::string_view subtitle{"H747 Lab portability shell"};
    std::uint8_t progress_percent{33U};
    bool playing{true};
    bool storage_ready{false};
    bool cover_ready{false};
};

struct PlayerBoardSnapshot {
    bool display_ready{false};
    bool framebuffer_ready{false};
    bool sdram_ready{false};
    bool sdram_smoke_ok{false};
    bool qspi_power_good{false};
    bool qspi_jedec_ok{false};
    bool qspi_read_ok{false};

    [[nodiscard]] constexpr bool raster_ready() const noexcept {
        return display_ready && framebuffer_ready && sdram_ready && sdram_smoke_ok;
    }

    [[nodiscard]] constexpr bool resource_storage_ready() const noexcept {
        return qspi_power_good && (qspi_jedec_ok || qspi_read_ok);
    }
};

[[nodiscard]] constexpr PlayerViewModel default_player_view() noexcept {
    return PlayerViewModel{};
}

[[nodiscard]] constexpr bool same_player_view(const PlayerViewModel& lhs,
                                              const PlayerViewModel& rhs) noexcept {
    return (lhs.title == rhs.title) &&
           (lhs.subtitle == rhs.subtitle) &&
           (lhs.progress_percent == rhs.progress_percent) &&
           (lhs.playing == rhs.playing) &&
           (lhs.storage_ready == rhs.storage_ready) &&
           (lhs.cover_ready == rhs.cover_ready);
}

class PlayerRuntime {
public:
    void reset() noexcept {
        view_ = default_player_view();
        last_tick_ms_ = 0U;
        input_ = {};
        manual_play_override_enabled_ = false;
        manual_playing_ = false;
        prev_encoder1_pressed_ = false;
        prev_encoder2_pressed_ = false;
        prev_pointer_down_ = false;
        dirty_ = false;
        if (has_board_snapshot_) {
            apply_board_snapshot();
        }
    }

    [[nodiscard]] const PlayerViewModel& view() const noexcept {
        return view_;
    }

    [[nodiscard]] PlayerViewModel& view() noexcept {
        return view_;
    }

    void observe_board(const PlayerBoardSnapshot snapshot) noexcept {
        const auto before = view_;
        board_ = snapshot;
        has_board_snapshot_ = true;
        apply_board_snapshot();
        dirty_ = dirty_ || !same_player_view(before, view_);
    }

    void observe_input(const charm::cap::InputFrame frame) noexcept {
        const auto before = view_;
        input_ = frame;
        apply_input_snapshot();
        dirty_ = dirty_ || !same_player_view(before, view_);
    }

    [[nodiscard]] bool consume_dirty() noexcept {
        const bool result = dirty_;
        dirty_ = false;
        return result;
    }

    [[nodiscard]] bool advance(const std::uint32_t now_ms) noexcept {
        if (!view_.playing) {
            return false;
        }
        if ((now_ms - last_tick_ms_) < 1000U) {
            return false;
        }
        last_tick_ms_ = now_ms;
        view_.progress_percent = static_cast<std::uint8_t>((view_.progress_percent + 7U) % 101U);
        return true;
    }

private:
    void apply_board_snapshot() noexcept {
        view_.playing = manual_play_override_enabled_ ? manual_playing_ : board_.raster_ready();
        view_.storage_ready = board_.resource_storage_ready();
        view_.cover_ready = board_.resource_storage_ready();

        if (board_.raster_ready() && board_.resource_storage_ready()) {
            view_.subtitle = (manual_play_override_enabled_ && !manual_playing_)
                ? "Display + storage ready (paused)"
                : "Display + storage ready";
        } else if (board_.raster_ready()) {
            view_.subtitle = (manual_play_override_enabled_ && !manual_playing_)
                ? "Display ready, storage probing (paused)"
                : "Display ready, storage probing";
        } else if (board_.display_ready) {
            view_.subtitle = "Display init, framebuffer pending";
        } else {
            view_.subtitle = "Board resources probing";
        }
    }

    void apply_input_snapshot() noexcept {
        auto bump_progress = [this](const std::int32_t delta) noexcept {
            std::int32_t next = static_cast<std::int32_t>(view_.progress_percent) + delta;
            if (next < 0) {
                next = 0;
            }
            if (next > 100) {
                next = 100;
            }
            view_.progress_percent = static_cast<std::uint8_t>(next);
        };

        if (input_.encoder1.detent_delta != 0) {
            bump_progress(static_cast<std::int32_t>(input_.encoder1.detent_delta));
        }
        if (input_.encoder2.detent_delta != 0) {
            bump_progress(static_cast<std::int32_t>(input_.encoder2.detent_delta) * 5);
        }

        const bool encoder1_pressed = input_.encoder1.pressed;
        if (encoder1_pressed && !prev_encoder1_pressed_) {
            manual_play_override_enabled_ = true;
            manual_playing_ = !view_.playing;
            apply_board_snapshot();
        }
        prev_encoder1_pressed_ = encoder1_pressed;

        const bool encoder2_pressed = input_.encoder2.pressed;
        if (encoder2_pressed && !prev_encoder2_pressed_) {
            manual_play_override_enabled_ = false;
            manual_playing_ = false;
            apply_board_snapshot();
        }
        prev_encoder2_pressed_ = encoder2_pressed;

        if (input_.pointer.detected && input_.pointer.down && !prev_pointer_down_) {
            manual_play_override_enabled_ = true;
            manual_playing_ = !manual_playing_;
            apply_board_snapshot();
        }
        prev_pointer_down_ = input_.pointer.down;

        if (input_.pointer.detected && input_.pointer.down && (input_.pointer.max_x != 0U)) {
            const std::uint32_t x = (input_.pointer.x > input_.pointer.max_x) ? input_.pointer.max_x
                                                                               : input_.pointer.x;
            view_.progress_percent = static_cast<std::uint8_t>((x * 100U) / input_.pointer.max_x);
        }

        if (manual_play_override_enabled_) {
            view_.playing = manual_playing_;
        }
    }

    PlayerViewModel view_{default_player_view()};
    PlayerBoardSnapshot board_{};
    charm::cap::InputFrame input_{};
    bool has_board_snapshot_{false};
    bool manual_play_override_enabled_{false};
    bool manual_playing_{false};
    bool prev_encoder1_pressed_{false};
    bool prev_encoder2_pressed_{false};
    bool prev_pointer_down_{false};
    bool dirty_{false};
    std::uint32_t last_tick_ms_{0U};
};

} // namespace h747::apps::player
