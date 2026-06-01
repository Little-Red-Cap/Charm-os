module;

#include <cstddef>
#include <cstdint>

export module charm.ui.scene.scene_evidence;

import charm.ui.scene;

export namespace ui::scene {
    struct SceneCmdEvidence {
        std::uint32_t cmd_count{0};
        std::uint32_t cmd_capacity{0};
        std::uint32_t cmd_bytes{0};
        std::uint32_t text_used{0};
        std::uint32_t text_capacity{0};
        std::uint32_t blob_used{0};
        std::uint32_t blob_capacity{0};
        std::uint32_t cmd_overflowed{0};
        std::uint32_t text_overflowed{0};
        std::uint32_t blob_overflowed{0};
    };

    struct SceneExecEvidence {
        std::uint32_t cmd_count{0};
        std::uint32_t cmd_bytes{0};
        std::uint32_t failed_cmds{0};
        std::uint32_t cmd_text{0};
        std::uint32_t cmd_rect{0};
        std::uint32_t cmd_image{0};
        std::uint32_t fail_text{0};
        std::uint32_t fail_image{0};
        std::uint32_t alpha_blend_count{0};
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

    struct SceneEvidence {
        SceneCmdEvidence cmd{};
        SceneExecEvidence exec{};
        SceneLayerEvidence layer{};
    };

    inline constexpr std::uint32_t clamp_scene_evidence_u32(std::uint64_t value) noexcept {
        constexpr std::uint64_t max_u32 = 0xFFFFFFFFULL;
        return static_cast<std::uint32_t>(value > max_u32 ? max_u32 : value);
    }

    inline SceneEvidence make_scene_evidence(const Scene& scene) noexcept {
        const auto cmd = scene.last_cmd_stats();
        const auto exec = scene.last_exec_stats();
        const auto layer = scene.layer_stats();

        SceneEvidence out{};
        out.cmd.cmd_count = clamp_scene_evidence_u32(cmd.cmd_count);
        out.cmd.cmd_capacity = clamp_scene_evidence_u32(cmd.cmd_capacity);
        out.cmd.cmd_bytes = clamp_scene_evidence_u32(cmd.cmd_bytes);
        out.cmd.text_used = clamp_scene_evidence_u32(cmd.text_used);
        out.cmd.text_capacity = clamp_scene_evidence_u32(cmd.text_capacity);
        out.cmd.blob_used = clamp_scene_evidence_u32(cmd.blob_used);
        out.cmd.blob_capacity = clamp_scene_evidence_u32(cmd.blob_capacity);
        out.cmd.cmd_overflowed = cmd.cmd_overflowed ? 1U : 0U;
        out.cmd.text_overflowed = cmd.text_overflowed ? 1U : 0U;
        out.cmd.blob_overflowed = cmd.blob_overflowed ? 1U : 0U;

        out.exec.cmd_count = clamp_scene_evidence_u32(exec.cmd_count);
        out.exec.cmd_bytes = clamp_scene_evidence_u32(exec.cmd_bytes);
        out.exec.failed_cmds = clamp_scene_evidence_u32(exec.failed_cmds);
        out.exec.cmd_text = clamp_scene_evidence_u32(exec.cmd_text);
        out.exec.cmd_rect = clamp_scene_evidence_u32(exec.cmd_rect);
        out.exec.cmd_image = clamp_scene_evidence_u32(exec.cmd_image);
        out.exec.fail_text = clamp_scene_evidence_u32(exec.fail_text);
        out.exec.fail_image = clamp_scene_evidence_u32(exec.fail_image);
        out.exec.alpha_blend_count = clamp_scene_evidence_u32(exec.alpha_blend_count);

        out.layer.snapshot_count = layer.snapshot_count;
        out.layer.snapshot_rebuild_count = layer.snapshot_rebuild_count;
        out.layer.stale_snapshot_count = layer.stale_snapshot_count;
        out.layer.layer_bytes = layer.layer_bytes;
        out.layer.composite_pixels = layer.composite_pixels;
        out.layer.pixel_blit_count = layer.pixel_blit_count;
        out.layer.pixel_blit_pixels = layer.pixel_blit_pixels;
        return out;
    }
}
