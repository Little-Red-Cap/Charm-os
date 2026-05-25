#include "console.h"
#include "player_md3_diag.hpp"

#include <cstddef>
#include <cstdint>

import charm.core.handle;

namespace {

std::uint32_t clamp_size_to_u32(const std::size_t value) noexcept {
    constexpr std::size_t max_u32 = 0xFFFFFFFFULL;
    return static_cast<std::uint32_t>(value > max_u32 ? max_u32 : value);
}

} // namespace

namespace h747::apps::player_md3 {

void sample_scene_stats() noexcept {
    auto* shell = shell_ref();
    auto& st = state();
    if (shell == nullptr || shell->scene() == nullptr) {
        st.scene_cmd_count = 0U;
        st.scene_cmd_capacity = 0U;
        st.scene_cmd_overflowed = 0U;
        st.scene_text_used = 0U;
        st.scene_text_capacity = 0U;
        st.scene_text_overflowed = 0U;
        st.scene_exec_failed = 0U;
        st.scene_exec_cmd_text = 0U;
        st.scene_exec_cmd_rect = 0U;
        st.scene_exec_cmd_image = 0U;
        st.scene_exec_fail_text = 0U;
        st.scene_exec_fail_image = 0U;
        return;
    }

    const auto& scene = *shell->scene();
    const auto cmd = scene.last_cmd_stats();
    const auto exec = scene.last_exec_stats();
    st.scene_cmd_count = clamp_size_to_u32(cmd.cmd_count);
    st.scene_cmd_capacity = clamp_size_to_u32(cmd.cmd_capacity);
    st.scene_cmd_overflowed = cmd.cmd_overflowed ? 1U : 0U;
    st.scene_text_used = clamp_size_to_u32(cmd.text_used);
    st.scene_text_capacity = clamp_size_to_u32(cmd.text_capacity);
    st.scene_text_overflowed = cmd.text_overflowed ? 1U : 0U;
    st.scene_exec_failed = clamp_size_to_u32(exec.failed_cmds);
    st.scene_exec_cmd_text = clamp_size_to_u32(exec.cmd_text);
    st.scene_exec_cmd_rect = clamp_size_to_u32(exec.cmd_rect);
    st.scene_exec_cmd_image = clamp_size_to_u32(exec.cmd_image);
    st.scene_exec_fail_text = clamp_size_to_u32(exec.fail_text);
    st.scene_exec_fail_image = clamp_size_to_u32(exec.fail_image);
}

} // namespace h747::apps::player_md3

extern "C" void charm_vivid_soa_unsupported_widget_kind(unsigned kind, unsigned caller) noexcept {
    h747::console::write("player_md3: unsupported widget kind=");
    h747::console::write_dec(kind);
    h747::console::write(" name=");
    h747::console::write(widget_kind_name(static_cast<WidgetKind>(kind)));
    h747::console::write(" caller=");
    h747::console::write_hex32(caller);
    h747::console::write("\n");
}
