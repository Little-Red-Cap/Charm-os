module;

#include "vivid_features.generated.hpp"

#include <cstddef>
#include <cstdint>

export module charm.ui.scene.scene_evidence;

import charm.ui.scene;
import charm.gfx.draw_cmd_evidence;

export namespace ui::scene {
    struct SceneCmdEvidence {
        std::uint32_t cmd_count{0};
        std::uint32_t cmd_capacity{0};
        std::uint32_t cmd_bytes{0};
        std::uint32_t text_used{0};
        std::uint32_t text_capacity{0};
        std::uint32_t blob_used{0};
        std::uint32_t blob_capacity{0};
        std::uint32_t batch_shrink{0};
        std::uint32_t batch_shrink_line{0};
        std::uint32_t batch_shrink_path{0};
        std::uint32_t batch_shrink_rect{0};
        std::uint32_t batch_shrink_round{0};
        std::uint32_t batch_shrink_image{0};
        std::uint32_t batch_shrink_focus{0};
        std::uint32_t cmd_overflowed{0};
        std::uint32_t text_overflowed{0};
        std::uint32_t blob_overflowed{0};
    };

    struct SceneExecEvidence {
        std::uint32_t cmd_count{0};
        std::uint32_t cmd_bytes{0};
        std::uint32_t failed_cmds{0};
        std::uint32_t clip_pushes{0};
        std::uint32_t clip_pops{0};
        std::uint32_t clip_push_overflow{0};
        std::uint32_t clip_pop_underflow{0};
        std::uint32_t clip_invalid{0};
        std::uint32_t fail_blob{0};
        std::uint32_t fail_path{0};
        std::uint32_t fail_clip{0};
        std::uint32_t fail_other{0};
        std::uint32_t dispatch_groups{0};
        std::uint32_t batch_flushes{0};
        std::uint32_t group_rect{0};
        std::uint32_t group_text{0};
        std::uint32_t group_image{0};
        std::uint32_t group_line{0};
        std::uint32_t group_path{0};
        std::uint32_t group_other{0};
        std::uint32_t cmd_text{0};
        std::uint32_t cmd_rect{0};
        std::uint32_t cmd_image{0};
        std::uint32_t cmd_line{0};
        std::uint32_t cmd_path{0};
        std::uint32_t cmd_other{0};
        std::uint32_t fail_text{0};
        std::uint32_t fail_image{0};
        std::uint32_t alpha_blend_count{0};
        std::uint32_t overflowed{0};
    };

    struct SceneLayerEvidence {
        std::uint32_t snapshot_count{0};
        std::uint32_t snapshot_rebuild_count{0};
        std::uint32_t stale_snapshot_count{0};
        std::uint32_t layer_bytes{0};
        std::uint32_t composite_pixels{0};
        std::uint32_t pixel_blit_count{0};
        std::uint32_t pixel_blit_pixels{0};
    };

    struct SceneTimingEvidence {
        std::uint32_t available{0};
        std::uint32_t record_us{0};
        std::uint32_t execute_us{0};
        std::uint32_t render_us{0};
    };

    struct SceneEvidence {
        SceneCmdEvidence cmd{};
        SceneExecEvidence exec{};
        SceneLayerEvidence layer{};
        SceneTimingEvidence timing{};
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        ui::draw_cmd::DrawCmdDetailEvidence draw_detail{};
#endif
    };

    inline constexpr std::uint32_t clamp_scene_evidence_u32(std::uint64_t value) noexcept {
        constexpr std::uint64_t max_u32 = 0xFFFFFFFFULL;
        return static_cast<std::uint32_t>(value > max_u32 ? max_u32 : value);
    }

    inline constexpr std::uint32_t clamp_scene_evidence_size(std::size_t value) noexcept {
        return clamp_scene_evidence_u32(static_cast<std::uint64_t>(value));
    }

    inline SceneCmdEvidence make_scene_cmd_evidence(const CmdStats& stats) noexcept {
        SceneCmdEvidence out{};
        out.cmd_count = clamp_scene_evidence_size(stats.cmd_count);
        out.cmd_capacity = clamp_scene_evidence_size(stats.cmd_capacity);
        out.cmd_bytes = clamp_scene_evidence_size(stats.cmd_bytes);
        out.text_used = clamp_scene_evidence_size(stats.text_used);
        out.text_capacity = clamp_scene_evidence_size(stats.text_capacity);
        out.blob_used = clamp_scene_evidence_size(stats.blob_used);
        out.blob_capacity = clamp_scene_evidence_size(stats.blob_capacity);
        out.batch_shrink = clamp_scene_evidence_size(stats.batch_shrink);
        out.batch_shrink_line = clamp_scene_evidence_size(stats.batch_shrink_line);
        out.batch_shrink_path = clamp_scene_evidence_size(stats.batch_shrink_path);
        out.batch_shrink_rect = clamp_scene_evidence_size(stats.batch_shrink_rect);
        out.batch_shrink_round = clamp_scene_evidence_size(stats.batch_shrink_round);
        out.batch_shrink_image = clamp_scene_evidence_size(stats.batch_shrink_image);
        out.batch_shrink_focus = clamp_scene_evidence_size(stats.batch_shrink_focus);
        out.cmd_overflowed = stats.cmd_overflowed ? 1U : 0U;
        out.text_overflowed = stats.text_overflowed ? 1U : 0U;
        out.blob_overflowed = stats.blob_overflowed ? 1U : 0U;
        return out;
    }

    inline SceneExecEvidence make_scene_exec_evidence(const ExecStats& stats) noexcept {
        SceneExecEvidence out{};
        out.cmd_count = clamp_scene_evidence_size(stats.cmd_count);
        out.cmd_bytes = clamp_scene_evidence_size(stats.cmd_bytes);
        out.failed_cmds = clamp_scene_evidence_size(stats.failed_cmds);
        out.clip_pushes = clamp_scene_evidence_size(stats.clip_pushes);
        out.clip_pops = clamp_scene_evidence_size(stats.clip_pops);
        out.clip_push_overflow = clamp_scene_evidence_size(stats.clip_push_overflow);
        out.clip_pop_underflow = clamp_scene_evidence_size(stats.clip_pop_underflow);
        out.clip_invalid = clamp_scene_evidence_size(stats.clip_invalid);
        out.fail_blob = clamp_scene_evidence_size(stats.fail_blob);
        out.fail_path = clamp_scene_evidence_size(stats.fail_path);
        out.fail_clip = clamp_scene_evidence_size(stats.fail_clip);
        out.fail_other = clamp_scene_evidence_size(stats.fail_other);
        out.dispatch_groups = clamp_scene_evidence_size(stats.dispatch_groups);
        out.batch_flushes = clamp_scene_evidence_size(stats.batch_flushes);
        out.group_rect = clamp_scene_evidence_size(stats.group_rect);
        out.group_text = clamp_scene_evidence_size(stats.group_text);
        out.group_image = clamp_scene_evidence_size(stats.group_image);
        out.group_line = clamp_scene_evidence_size(stats.group_line);
        out.group_path = clamp_scene_evidence_size(stats.group_path);
        out.group_other = clamp_scene_evidence_size(stats.group_other);
        out.cmd_text = clamp_scene_evidence_size(stats.cmd_text);
        out.cmd_rect = clamp_scene_evidence_size(stats.cmd_rect);
        out.cmd_image = clamp_scene_evidence_size(stats.cmd_image);
        out.cmd_line = clamp_scene_evidence_size(stats.cmd_line);
        out.cmd_path = clamp_scene_evidence_size(stats.cmd_path);
        out.cmd_other = clamp_scene_evidence_size(stats.cmd_other);
        out.fail_text = clamp_scene_evidence_size(stats.fail_text);
        out.fail_image = clamp_scene_evidence_size(stats.fail_image);
        out.alpha_blend_count = clamp_scene_evidence_u32(stats.alpha_blend_count);
        out.overflowed = stats.overflowed ? 1U : 0U;
        return out;
    }

    inline SceneEvidence make_scene_evidence(const Scene& scene) noexcept {
        const auto cmd = scene.last_cmd_stats();
        const auto exec = scene.last_exec_stats();
        const auto layer = scene.layer_stats();

        SceneEvidence out{};
        out.cmd = make_scene_cmd_evidence(cmd);
        out.exec = make_scene_exec_evidence(exec);

        out.layer.snapshot_count = layer.snapshot_count;
        out.layer.snapshot_rebuild_count = layer.snapshot_rebuild_count;
        out.layer.stale_snapshot_count = layer.stale_snapshot_count;
        out.layer.layer_bytes = layer.layer_bytes;
        out.layer.composite_pixels = layer.composite_pixels;
        out.layer.pixel_blit_count = layer.pixel_blit_count;
        out.layer.pixel_blit_pixels = layer.pixel_blit_pixels;

        const auto timing = scene.last_render_timing();
        out.timing.available = timing.available;
        out.timing.record_us = timing.record_us;
        out.timing.execute_us = timing.execute_us;
        out.timing.render_us = timing.render_us;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        out.draw_detail = ui::draw_cmd::make_draw_cmd_detail_evidence(scene.last_draw_detail_stats());
#endif
        return out;
    }
}
