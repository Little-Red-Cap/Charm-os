module;

#include <cstddef>
#include <cstdint>

export module charm.ui.scene.layer_support;

import charm.ui.scene.layer_runtime;
import charm.gfx.color;

export namespace ui::scene {
    struct CmdStats {
        std::size_t cmd_count{0};
        std::size_t cmd_capacity{0};
        std::size_t cmd_bytes{0};
        std::size_t text_used{0};
        std::size_t text_capacity{0};
        std::size_t blob_used{0};
        std::size_t blob_capacity{0};
        std::size_t batch_shrink{0};
        std::size_t batch_shrink_line{0};
        std::size_t batch_shrink_path{0};
        std::size_t batch_shrink_rect{0};
        std::size_t batch_shrink_round{0};
        std::size_t batch_shrink_image{0};
        std::size_t batch_shrink_focus{0};
        bool cmd_overflowed{false};
        bool text_overflowed{false};
        bool blob_overflowed{false};
        bool workspace_overflowed{false};
        bool traversal_phase_conflicted{false};
        bool style_patch_overflowed{false};
        bool semantic_overflowed{false};
    };

    struct ExecStats {
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::uint64_t alpha_blend_count{0};
        std::size_t clip_pushes{0};
        std::size_t clip_pops{0};
        std::size_t clip_push_overflow{0};
        std::size_t clip_pop_underflow{0};
        std::size_t clip_invalid{0};
        std::size_t failed_cmds{0};
        std::size_t fail_text{0};
        std::size_t fail_image{0};
        std::size_t fail_blob{0};
        std::size_t fail_path{0};
        std::size_t fail_clip{0};
        std::size_t fail_other{0};
        std::size_t dispatch_groups{0};
        std::size_t batch_flushes{0};
        std::size_t group_rect{0};
        std::size_t group_text{0};
        std::size_t group_image{0};
        std::size_t group_line{0};
        std::size_t group_path{0};
        std::size_t group_other{0};
        std::size_t cmd_rect{0};
        std::size_t cmd_text{0};
        std::size_t cmd_image{0};
        std::size_t cmd_line{0};
        std::size_t cmd_path{0};
        std::size_t cmd_other{0};
        bool overflowed{false};
    };

    struct CommandReplayCost {
        std::uint32_t tiles_considered{0};
        std::uint32_t tiles_executed{0};
        std::uint32_t tiles_skipped{0};
        std::size_t bounds_command_reads{0};
        std::size_t bounds_item_reads{0};
        std::size_t execute_command_reads{0};
        std::size_t execute_chunks_skipped{0};

        [[nodiscard]] constexpr std::size_t total_command_reads() const noexcept {
            return bounds_command_reads + execute_command_reads;
        }
    };

    struct LayerReplayResult {
        LayerReplayStatus status{LayerReplayStatus::InvalidPlan};
        SnapshotHandle source{};
        SnapshotKind kind{SnapshotKind::EmptyFallback};
        Rect target_bounds{};
        ExecStats stats{};
        CommandReplayCost command_cost{};

        [[nodiscard]] constexpr bool ok() const noexcept {
            return status == LayerReplayStatus::Ok;
        }
    };

    struct LayerCaptureResult {
        LayerCaptureStatus status{LayerCaptureStatus::NoSnapshotSlot};
        SnapshotHandle handle{};
        SnapshotKind kind{SnapshotKind::EmptyFallback};
        std::uint32_t bytes{0};
        std::uint32_t command_count{0};

        [[nodiscard]] constexpr bool ok() const noexcept {
            return status == LayerCaptureStatus::Ok;
        }
    };

    struct TileStats {
        int tiles_total{0};
        int tiles_drawn{0};
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::uint64_t alpha_blend_count{0};
        int tile_flush_count{0};
        std::size_t clip_push_overflow{0};
        std::size_t clip_pop_underflow{0};
        std::size_t clip_invalid{0};
        std::size_t dispatch_groups{0};
        std::size_t batch_flushes{0};
        std::size_t failed_cmds{0};
        std::size_t fail_text{0};
        std::size_t fail_image{0};
        std::size_t fail_blob{0};
        std::size_t fail_path{0};
        std::size_t fail_clip{0};
        std::size_t fail_other{0};
        std::size_t group_rect{0};
        std::size_t group_text{0};
        std::size_t group_image{0};
        std::size_t group_line{0};
        std::size_t group_path{0};
        std::size_t group_other{0};
        std::size_t cmd_rect{0};
        std::size_t cmd_text{0};
        std::size_t cmd_image{0};
        std::size_t cmd_line{0};
        std::size_t cmd_path{0};
        std::size_t cmd_other{0};
    };

    struct TileConfig {
        int tile_width{64};
        int tile_height{64};
        rgba clear_color{0, 0, 0, 0};
        bool clear_tile{true};
    };
}
