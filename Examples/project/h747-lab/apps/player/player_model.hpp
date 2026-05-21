#pragma once

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

enum class PlayerCommandKind : std::uint8_t {
    toggle_play,
    seek_relative,
    next_track,
    previous_track,
};

struct PlayerCommand {
    PlayerCommandKind kind{PlayerCommandKind::toggle_play};
    std::int8_t delta_percent{};
};

[[nodiscard]] constexpr PlayerViewModel default_player_view() noexcept {
    return PlayerViewModel{};
}

class PlayerRuntime {
public:
    void reset() noexcept {
        view_ = default_player_view();
        last_tick_ms_ = 0U;
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
        board_ = snapshot;
        has_board_snapshot_ = true;
        apply_board_snapshot();
    }

    void dispatch(const PlayerCommand command) noexcept {
        switch (command.kind) {
        case PlayerCommandKind::toggle_play:
            user_play_override_ = !view_.playing;
            has_user_play_override_ = true;
            view_.playing = user_play_override_;
            break;
        case PlayerCommandKind::seek_relative:
            view_.progress_percent = clamp_progress(static_cast<std::int16_t>(view_.progress_percent) +
                                                     command.delta_percent);
            break;
        case PlayerCommandKind::next_track:
            view_.progress_percent = 0U;
            break;
        case PlayerCommandKind::previous_track:
            view_.progress_percent = 0U;
            break;
        }
    }

    [[nodiscard]] bool advance(const std::uint32_t now_ms) noexcept {
        if ((now_ms - last_tick_ms_) < 1000U) {
            return false;
        }
        last_tick_ms_ = now_ms;
        view_.progress_percent = static_cast<std::uint8_t>((view_.progress_percent + 7U) % 101U);
        return true;
    }

private:
    [[nodiscard]] static constexpr std::uint8_t clamp_progress(const std::int16_t value) noexcept {
        if (value <= 0) {
            return 0U;
        }
        if (value >= 100) {
            return 100U;
        }
        return static_cast<std::uint8_t>(value);
    }

    void apply_board_snapshot() noexcept {
        view_.playing = has_user_play_override_ ? user_play_override_ : board_.raster_ready();
        view_.storage_ready = board_.resource_storage_ready();
        view_.cover_ready = board_.resource_storage_ready();

        if (board_.raster_ready() && board_.resource_storage_ready()) {
            view_.subtitle = "Display + storage ready";
        } else if (board_.raster_ready()) {
            view_.subtitle = "Display ready, storage probing";
        } else if (board_.display_ready) {
            view_.subtitle = "Display init, framebuffer pending";
        } else {
            view_.subtitle = "Board resources probing";
        }
    }

    PlayerViewModel view_{default_player_view()};
    PlayerBoardSnapshot board_{};
    bool has_board_snapshot_{false};
    bool has_user_play_override_{false};
    bool user_play_override_{false};
    std::uint32_t last_tick_ms_{0U};
};

} // namespace h747::apps::player
