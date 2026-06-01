#include <cstddef>
#include <cstdint>

#include "console.h"
#include "player_md3_diag.hpp"

import charm.core.handle;
import charm.core.style_sheet;
import charm.ui.scene.scene_evidence;

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
        st.scene_batch_shrink = 0U;
        st.scene_batch_shrink_rect = 0U;
        st.scene_batch_shrink_text_path = 0U;
        st.scene_exec_clip_failures = 0U;
        st.scene_exec_batch_flushes = 0U;
        st.scene_exec_dispatch_groups = 0U;
        st.scene_exec_group_rect = 0U;
        st.scene_exec_group_text = 0U;
        st.scene_exec_group_image = 0U;
        st.scene_exec_group_other = 0U;
        st.scene_exec_cmd_line = 0U;
        st.scene_exec_cmd_path = 0U;
        st.scene_exec_cmd_other = 0U;
        st.scene_exec_fail_other = 0U;
        st.scene_exec_overflowed = 0U;
        st.style_rule_count = 0U;
        st.style_rule_capacity = 0U;
        st.style_metrics_used = 0U;
        st.style_metrics_capacity = 0U;
        st.style_metrics_overflowed = 0U;
        return;
    }

    const auto scene = ::ui::scene::make_scene_evidence(*shell->scene());
    st.scene_cmd_count = scene.cmd.cmd_count;
    st.scene_cmd_capacity = scene.cmd.cmd_capacity;
    st.scene_cmd_overflowed = scene.cmd.cmd_overflowed;
    st.scene_text_used = scene.cmd.text_used;
    st.scene_text_capacity = scene.cmd.text_capacity;
    st.scene_text_overflowed = scene.cmd.text_overflowed;
    st.scene_exec_failed = scene.exec.failed_cmds;
    st.scene_exec_cmd_text = scene.exec.cmd_text;
    st.scene_exec_cmd_rect = scene.exec.cmd_rect;
    st.scene_exec_cmd_image = scene.exec.cmd_image;
    st.scene_exec_fail_text = scene.exec.fail_text;
    st.scene_exec_fail_image = scene.exec.fail_image;
    st.scene_batch_shrink = scene.cmd.batch_shrink;
    st.scene_batch_shrink_rect = scene.cmd.batch_shrink_rect
        + scene.cmd.batch_shrink_round
        + scene.cmd.batch_shrink_focus;
    st.scene_batch_shrink_text_path = scene.cmd.batch_shrink_line
        + scene.cmd.batch_shrink_path
        + scene.cmd.batch_shrink_image;
    st.scene_exec_clip_failures = scene.exec.clip_push_overflow
        + scene.exec.clip_pop_underflow
        + scene.exec.clip_invalid
        + scene.exec.fail_clip;
    st.scene_exec_batch_flushes = scene.exec.batch_flushes;
    st.scene_exec_dispatch_groups = scene.exec.dispatch_groups;
    st.scene_exec_group_rect = scene.exec.group_rect;
    st.scene_exec_group_text = scene.exec.group_text;
    st.scene_exec_group_image = scene.exec.group_image;
    st.scene_exec_group_other = scene.exec.group_line
        + scene.exec.group_path
        + scene.exec.group_other;
    st.scene_exec_cmd_line = scene.exec.cmd_line;
    st.scene_exec_cmd_path = scene.exec.cmd_path;
    st.scene_exec_cmd_other = scene.exec.cmd_other;
    st.scene_exec_fail_other = scene.exec.fail_blob
        + scene.exec.fail_path
        + scene.exec.fail_clip
        + scene.exec.fail_other;
    st.scene_exec_overflowed = scene.exec.overflowed;

    const auto style = style_sheet_runtime_profile();
    st.style_rule_count = clamp_size_to_u32(style.style_rule_count);
    st.style_rule_capacity = clamp_size_to_u32(style.style_rule_capacity);
    st.style_metrics_used = clamp_size_to_u32(style.metrics_pool_used);
    st.style_metrics_capacity = clamp_size_to_u32(style.metrics_pool_capacity);
    st.style_metrics_overflowed = style.metrics_overflowed ? 1U : 0U;
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
