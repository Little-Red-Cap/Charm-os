#include "player_md3_render_scheduler.hpp"

#include "console.h"
#include "port.h"
#include "player_md3_runtime.hpp"

namespace h747::apps::player_md3 {

void render_scheduler_mark_dirty() noexcept {
    auto& st = state();
    st.render_throttle_dirty = 1U;
    st.render_throttle_dirty_since_ms = h747::port::tick_ms();
}

void render_scheduler_set_enabled(const bool enabled) noexcept {
    auto& st = state();
    st.render_throttle_enabled = enabled ? 1U : 0U;
    render_scheduler_mark_dirty();
}

bool render_scheduler_should_render(const std::uint32_t now_ms) noexcept {
    const auto& st = state();
    const bool keepalive_due =
        st.render_throttle_last_render_ms == 0U
        || (now_ms - st.render_throttle_last_render_ms) >= st.render_throttle_interval_ms;
    const bool dirty_due =
        st.render_throttle_dirty != 0U
        && (st.render_throttle_dirty_since_ms == 0U
            || (now_ms - st.render_throttle_dirty_since_ms) >= st.render_throttle_defer_ms);
    return st.render_throttle_enabled == 0U || dirty_due || keepalive_due;
}

void render_scheduler_note_full_render() noexcept {
    auto& st = state();
    st.render_throttle_dirty = 0U;
    st.render_throttle_dirty_since_ms = 0U;
    st.render_throttle_last_render_ms = h747::port::tick_ms();
}

void render_scheduler_note_forced_render() noexcept {
    ++state().render_throttle_forced;
}

void render_scheduler_note_skip() noexcept {
    ++state().render_throttle_skipped;
}

void render_scheduler_print_status() noexcept {
    const auto& st = state();
    h747::console::write("render_throttle enabled=");
    h747::console::write_dec(st.render_throttle_enabled);
    h747::console::write(" skipped=");
    h747::console::write_dec(st.render_throttle_skipped);
    h747::console::write(" forced=");
    h747::console::write_dec(st.render_throttle_forced);
    h747::console::write(" interval_ms=");
    h747::console::write_dec(st.render_throttle_interval_ms);
    h747::console::write(" defer_ms=");
    h747::console::write_dec(st.render_throttle_defer_ms);
    h747::console::write(" dirty=");
    h747::console::write_dec(st.render_throttle_dirty);
    h747::console::write(" last_ms=");
    h747::console::write_dec(st.render_throttle_last_render_ms);
    h747::console::write("\n");
}

} // namespace h747::apps::player_md3
