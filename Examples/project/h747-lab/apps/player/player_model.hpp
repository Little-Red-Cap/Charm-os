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

[[nodiscard]] constexpr PlayerViewModel default_player_view() noexcept {
    return PlayerViewModel{};
}

class PlayerRuntime {
public:
    void reset() noexcept {
        view_ = default_player_view();
        last_tick_ms_ = 0U;
    }

    [[nodiscard]] const PlayerViewModel& view() const noexcept {
        return view_;
    }

    [[nodiscard]] PlayerViewModel& view() noexcept {
        return view_;
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
    PlayerViewModel view_{default_player_view()};
    std::uint32_t last_tick_ms_{0U};
};

} // namespace h747::apps::player
