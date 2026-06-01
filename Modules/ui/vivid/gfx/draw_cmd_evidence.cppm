module;

#include "vivid_features.generated.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

export module charm.gfx.draw_cmd_evidence;

import charm.gfx.draw_cmd;

export namespace ui::draw_cmd {
    struct DrawCmdBufferEvidence {
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

    struct DrawCmdExecEvidence {
        std::uint32_t cmd_count{0};
        std::uint32_t cmd_bytes{0};
        std::uint32_t clip_pushes{0};
        std::uint32_t clip_pops{0};
        std::uint32_t clip_push_overflow{0};
        std::uint32_t clip_pop_underflow{0};
        std::uint32_t clip_invalid{0};
        std::uint32_t failed_cmds{0};
        std::uint32_t fail_text{0};
        std::uint32_t fail_image{0};
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
        std::uint32_t cmd_rect{0};
        std::uint32_t cmd_text{0};
        std::uint32_t cmd_image{0};
        std::uint32_t cmd_line{0};
        std::uint32_t cmd_path{0};
        std::uint32_t cmd_other{0};
        std::uint32_t overflowed{0};
    };

    struct DrawCmdTileEvidence {
        std::uint32_t tiles_total{0};
        std::uint32_t tiles_drawn{0};
        std::uint32_t cmd_count{0};
        std::uint32_t cmd_bytes{0};
        std::uint32_t tile_flush_count{0};
        std::uint32_t clip_push_overflow{0};
        std::uint32_t clip_pop_underflow{0};
        std::uint32_t clip_invalid{0};
        std::uint32_t dispatch_groups{0};
        std::uint32_t batch_flushes{0};
        std::uint32_t failed_cmds{0};
        std::uint32_t fail_text{0};
        std::uint32_t fail_image{0};
        std::uint32_t fail_blob{0};
        std::uint32_t fail_path{0};
        std::uint32_t fail_clip{0};
        std::uint32_t fail_other{0};
        std::uint32_t group_rect{0};
        std::uint32_t group_text{0};
        std::uint32_t group_image{0};
        std::uint32_t group_line{0};
        std::uint32_t group_path{0};
        std::uint32_t group_other{0};
        std::uint32_t cmd_rect{0};
        std::uint32_t cmd_text{0};
        std::uint32_t cmd_image{0};
        std::uint32_t cmd_line{0};
        std::uint32_t cmd_path{0};
        std::uint32_t cmd_other{0};
    };

#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
    struct DrawCmdTypeDetailEvidence {
        std::uint32_t count{0};
        std::uint32_t rect_area{0};
        std::uint32_t actual_alpha_pixels{0};
        std::uint32_t max_rect_area{0};
    };

    struct DrawScopeDetailEvidence {
        std::uint16_t scope_id{kDrawScopeDefault};
        std::uint16_t active{0};
        std::uint32_t cmd_count{0};
        std::uint32_t rect_area{0};
        std::uint32_t actual_alpha_pixels{0};
    };

    using DrawCmdTypeDetailEvidenceTable = std::array<DrawCmdTypeDetailEvidence, kCmdTypeCount>;
    using DrawScopeDetailEvidenceTable = std::array<DrawScopeDetailEvidence, kDrawScopeDetailCapacity>;

    struct DrawCmdDetailEvidence {
        DrawCmdTypeDetailEvidenceTable types{};
        DrawScopeDetailEvidenceTable scopes{};
        std::uint32_t scope_overflow{0};
    };
#else
    struct DrawCmdDetailEvidence {};
#endif

    inline constexpr std::uint32_t clamp_draw_cmd_evidence_u32(std::uint64_t value) noexcept {
        constexpr std::uint64_t max_u32 = 0xFFFFFFFFULL;
        return static_cast<std::uint32_t>(value > max_u32 ? max_u32 : value);
    }

    inline constexpr std::uint32_t clamp_draw_cmd_evidence_size(std::size_t value) noexcept {
        return clamp_draw_cmd_evidence_u32(static_cast<std::uint64_t>(value));
    }

    inline constexpr std::uint32_t clamp_draw_cmd_evidence_int(int value) noexcept {
        return value <= 0 ? 0U : clamp_draw_cmd_evidence_u32(static_cast<std::uint64_t>(value));
    }

    inline DrawCmdBufferEvidence make_draw_cmd_buffer_evidence(const DrawCmdStats& stats) noexcept {
        DrawCmdBufferEvidence out{};
        out.cmd_count = clamp_draw_cmd_evidence_size(stats.cmd_count);
        out.cmd_capacity = clamp_draw_cmd_evidence_size(stats.cmd_capacity);
        out.cmd_bytes = clamp_draw_cmd_evidence_size(stats.cmd_bytes);
        out.text_used = clamp_draw_cmd_evidence_size(stats.text_used);
        out.text_capacity = clamp_draw_cmd_evidence_size(stats.text_capacity);
        out.blob_used = clamp_draw_cmd_evidence_size(stats.blob_used);
        out.blob_capacity = clamp_draw_cmd_evidence_size(stats.blob_capacity);
        out.batch_shrink = clamp_draw_cmd_evidence_size(stats.batch_shrink);
        out.batch_shrink_line = clamp_draw_cmd_evidence_size(stats.batch_shrink_line);
        out.batch_shrink_path = clamp_draw_cmd_evidence_size(stats.batch_shrink_path);
        out.batch_shrink_rect = clamp_draw_cmd_evidence_size(stats.batch_shrink_rect);
        out.batch_shrink_round = clamp_draw_cmd_evidence_size(stats.batch_shrink_round);
        out.batch_shrink_image = clamp_draw_cmd_evidence_size(stats.batch_shrink_image);
        out.batch_shrink_focus = clamp_draw_cmd_evidence_size(stats.batch_shrink_focus);
        out.cmd_overflowed = stats.cmd_overflowed ? 1U : 0U;
        out.text_overflowed = stats.text_overflowed ? 1U : 0U;
        out.blob_overflowed = stats.blob_overflowed ? 1U : 0U;
        return out;
    }

    inline DrawCmdExecEvidence make_draw_cmd_exec_evidence(const DrawCmdExecStats& stats) noexcept {
        DrawCmdExecEvidence out{};
        out.cmd_count = clamp_draw_cmd_evidence_size(stats.cmd_count);
        out.cmd_bytes = clamp_draw_cmd_evidence_size(stats.cmd_bytes);
        out.clip_pushes = clamp_draw_cmd_evidence_size(stats.clip_pushes);
        out.clip_pops = clamp_draw_cmd_evidence_size(stats.clip_pops);
        out.clip_push_overflow = clamp_draw_cmd_evidence_size(stats.clip_push_overflow);
        out.clip_pop_underflow = clamp_draw_cmd_evidence_size(stats.clip_pop_underflow);
        out.clip_invalid = clamp_draw_cmd_evidence_size(stats.clip_invalid);
        out.failed_cmds = clamp_draw_cmd_evidence_size(stats.failed_cmds);
        out.fail_text = clamp_draw_cmd_evidence_size(stats.fail_text);
        out.fail_image = clamp_draw_cmd_evidence_size(stats.fail_image);
        out.fail_blob = clamp_draw_cmd_evidence_size(stats.fail_blob);
        out.fail_path = clamp_draw_cmd_evidence_size(stats.fail_path);
        out.fail_clip = clamp_draw_cmd_evidence_size(stats.fail_clip);
        out.fail_other = clamp_draw_cmd_evidence_size(stats.fail_other);
        out.dispatch_groups = clamp_draw_cmd_evidence_size(stats.dispatch_groups);
        out.batch_flushes = clamp_draw_cmd_evidence_size(stats.batch_flushes);
        out.group_rect = clamp_draw_cmd_evidence_size(stats.group_rect);
        out.group_text = clamp_draw_cmd_evidence_size(stats.group_text);
        out.group_image = clamp_draw_cmd_evidence_size(stats.group_image);
        out.group_line = clamp_draw_cmd_evidence_size(stats.group_line);
        out.group_path = clamp_draw_cmd_evidence_size(stats.group_path);
        out.group_other = clamp_draw_cmd_evidence_size(stats.group_other);
        out.cmd_rect = clamp_draw_cmd_evidence_size(stats.cmd_rect);
        out.cmd_text = clamp_draw_cmd_evidence_size(stats.cmd_text);
        out.cmd_image = clamp_draw_cmd_evidence_size(stats.cmd_image);
        out.cmd_line = clamp_draw_cmd_evidence_size(stats.cmd_line);
        out.cmd_path = clamp_draw_cmd_evidence_size(stats.cmd_path);
        out.cmd_other = clamp_draw_cmd_evidence_size(stats.cmd_other);
        out.overflowed = stats.overflowed ? 1U : 0U;
        return out;
    }

    inline DrawCmdTileEvidence make_draw_cmd_tile_evidence(const DrawCmdTileStats& stats) noexcept {
        DrawCmdTileEvidence out{};
        out.tiles_total = clamp_draw_cmd_evidence_int(stats.tiles_total);
        out.tiles_drawn = clamp_draw_cmd_evidence_int(stats.tiles_drawn);
        out.cmd_count = clamp_draw_cmd_evidence_size(stats.cmd_count);
        out.cmd_bytes = clamp_draw_cmd_evidence_size(stats.cmd_bytes);
        out.tile_flush_count = clamp_draw_cmd_evidence_int(stats.tile_flush_count);
        out.clip_push_overflow = clamp_draw_cmd_evidence_size(stats.clip_push_overflow);
        out.clip_pop_underflow = clamp_draw_cmd_evidence_size(stats.clip_pop_underflow);
        out.clip_invalid = clamp_draw_cmd_evidence_size(stats.clip_invalid);
        out.dispatch_groups = clamp_draw_cmd_evidence_size(stats.dispatch_groups);
        out.batch_flushes = clamp_draw_cmd_evidence_size(stats.batch_flushes);
        out.failed_cmds = clamp_draw_cmd_evidence_size(stats.failed_cmds);
        out.fail_text = clamp_draw_cmd_evidence_size(stats.fail_text);
        out.fail_image = clamp_draw_cmd_evidence_size(stats.fail_image);
        out.fail_blob = clamp_draw_cmd_evidence_size(stats.fail_blob);
        out.fail_path = clamp_draw_cmd_evidence_size(stats.fail_path);
        out.fail_clip = clamp_draw_cmd_evidence_size(stats.fail_clip);
        out.fail_other = clamp_draw_cmd_evidence_size(stats.fail_other);
        out.group_rect = clamp_draw_cmd_evidence_size(stats.group_rect);
        out.group_text = clamp_draw_cmd_evidence_size(stats.group_text);
        out.group_image = clamp_draw_cmd_evidence_size(stats.group_image);
        out.group_line = clamp_draw_cmd_evidence_size(stats.group_line);
        out.group_path = clamp_draw_cmd_evidence_size(stats.group_path);
        out.group_other = clamp_draw_cmd_evidence_size(stats.group_other);
        out.cmd_rect = clamp_draw_cmd_evidence_size(stats.cmd_rect);
        out.cmd_text = clamp_draw_cmd_evidence_size(stats.cmd_text);
        out.cmd_image = clamp_draw_cmd_evidence_size(stats.cmd_image);
        out.cmd_line = clamp_draw_cmd_evidence_size(stats.cmd_line);
        out.cmd_path = clamp_draw_cmd_evidence_size(stats.cmd_path);
        out.cmd_other = clamp_draw_cmd_evidence_size(stats.cmd_other);
        return out;
    }

#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
    inline DrawCmdDetailEvidence make_draw_cmd_detail_evidence(const DrawCmdDetailStats& stats) noexcept {
        DrawCmdDetailEvidence out{};
        for (std::size_t i = 0; i < out.types.size(); ++i) {
            out.types[i].count = stats.types[i].count;
            out.types[i].rect_area = clamp_draw_cmd_evidence_u32(stats.types[i].rect_area);
            out.types[i].actual_alpha_pixels = clamp_draw_cmd_evidence_u32(stats.types[i].actual_alpha_pixels);
            out.types[i].max_rect_area = stats.types[i].max_rect_area;
        }
        for (std::size_t i = 0; i < out.scopes.size(); ++i) {
            out.scopes[i].scope_id = stats.scopes[i].scope_id;
            out.scopes[i].active = stats.scopes[i].active;
            out.scopes[i].cmd_count = stats.scopes[i].cmd_count;
            out.scopes[i].rect_area = clamp_draw_cmd_evidence_u32(stats.scopes[i].rect_area);
            out.scopes[i].actual_alpha_pixels = clamp_draw_cmd_evidence_u32(stats.scopes[i].actual_alpha_pixels);
        }
        out.scope_overflow = stats.scope_overflow;
        return out;
    }
#else
    inline DrawCmdDetailEvidence make_draw_cmd_detail_evidence(const DrawCmdDetailStats&) noexcept {
        return {};
    }
#endif
}
