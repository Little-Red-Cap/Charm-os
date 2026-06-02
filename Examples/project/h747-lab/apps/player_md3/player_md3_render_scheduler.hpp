#pragma once

#include <cstdint>

namespace h747::apps::player_md3 {

void render_scheduler_mark_dirty() noexcept;
void render_scheduler_set_enabled(bool enabled) noexcept;
[[nodiscard]] bool render_scheduler_should_render(std::uint32_t now_ms) noexcept;
void render_scheduler_note_full_render() noexcept;
void render_scheduler_note_forced_render() noexcept;
void render_scheduler_note_skip() noexcept;
void render_scheduler_print_status() noexcept;

} // namespace h747::apps::player_md3
