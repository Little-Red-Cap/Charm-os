#include "display_raster.h"
#include "player_md3_diag.hpp"

#include <cstdint>

namespace h747::apps::player_md3 {

void update_smoke_verdict() noexcept {
    const auto raster = display_raster_state();
    auto& st = state();
    const std::uint32_t frame_delta = st.frames - st.smoke_last_frames;
    const std::uint32_t present_delta = raster.present_count - st.smoke_last_present;
    st.smoke_last_frames = st.frames;
    st.smoke_last_present = raster.present_count;
    st.smoke_frame_delta = frame_delta;
    st.smoke_present_delta = present_delta;
    st.smoke_mock_path = 0U;
    st.smoke_boot_ok = (st.display_ready && st.runtime_storage_ready && st.runtime_bootstrapped) ? 1U : 0U;
    st.smoke_render_ok = (st.last_render_ok && frame_delta > 0U) ? 1U : 0U;
    st.smoke_present_ok = (raster.present_ok != 0U && raster.present_count > 0U && present_delta > 0U) ? 1U : 0U;
    st.smoke_content_ok = (st.render_non_bg_pixels > 0U && st.render_content_max_x > st.render_content_min_x
                           && st.render_content_max_y > st.render_content_min_y)
        ? 1U
        : 0U;
    st.smoke_scene_ok = (st.scene_cmd_count > 0U && st.scene_exec_failed == 0U
                         && st.scene_text_overflowed == 0U && st.scene_cmd_overflowed == 0U)
        ? 1U
        : 0U;
    st.smoke_ok = (st.smoke_boot_ok && st.smoke_render_ok && st.smoke_present_ok
                   && st.smoke_content_ok && st.smoke_scene_ok && st.smoke_mock_path == 0U)
        ? 1U
        : 0U;
}

} // namespace h747::apps::player_md3
