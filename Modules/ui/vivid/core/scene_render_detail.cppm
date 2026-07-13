module;

#include <cstddef>
#include <cstdint>
#include <cstring>

module charm.ui.scene:render_detail;

import charm.core.config;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.draw_cmd;
import charm.gfx.pixel_ops;
import charm.ui.scene.layer_support;

namespace ui::scene::detail {
    rgba decode_snapshot_pixel(const std::byte* src) noexcept {
        if (!src) return {};
        if constexpr (screen_pixel_format == PixelFormat::RGB565) {
            std::uint16_t packed{};
            std::memcpy(&packed, src, sizeof(packed));
            const rgb value = unpack_rgb565(packed);
            return {value.r, value.g, value.b, 255};
        } else if constexpr (screen_pixel_format == PixelFormat::RGB888) {
            return {
                static_cast<std::uint8_t>(src[0]),
                static_cast<std::uint8_t>(src[1]),
                static_cast<std::uint8_t>(src[2]),
                255
            };
        } else {
            std::uint32_t packed{};
            std::memcpy(&packed, src, sizeof(packed));
            return unpack_argb8888(packed);
        }
    }

    std::uint64_t blend_pixel_snapshot_row(CanvasBase& canvas,
                                           int x,
                                           int y,
                                           const std::byte* src,
                                           int width,
                                           std::uint8_t opacity) noexcept {
        if (!src || width <= 0 || opacity == 0) return 0;
        const auto bpp = canvas.bytes_per_pixel();
        std::uint64_t blended = 0;
        for (int i = 0; i < width; ++i) {
            rgba pixel = decode_snapshot_pixel(src + static_cast<std::size_t>(i) * bpp);
            pixel.a = static_cast<std::uint8_t>(
                (static_cast<std::uint16_t>(pixel.a) * opacity) / 255u);
            if (pixel.a > 0 && pixel.a < 255) {
                ++blended;
            }
            canvas.set_pixel(x + i, y, pixel);
        }
        return blended;
    }

    CmdStats to_scene_stats(const ui::draw_cmd::DrawCmdStats& stats) noexcept {
        CmdStats out{};
        out.cmd_count = stats.cmd_count;
        out.cmd_capacity = stats.cmd_capacity;
        out.cmd_bytes = stats.cmd_bytes;
        out.text_used = stats.text_used;
        out.text_capacity = stats.text_capacity;
        out.blob_used = stats.blob_used;
        out.blob_capacity = stats.blob_capacity;
        out.batch_shrink = stats.batch_shrink;
        out.batch_shrink_line = stats.batch_shrink_line;
        out.batch_shrink_path = stats.batch_shrink_path;
        out.batch_shrink_rect = stats.batch_shrink_rect;
        out.batch_shrink_round = stats.batch_shrink_round;
        out.batch_shrink_image = stats.batch_shrink_image;
        out.batch_shrink_focus = stats.batch_shrink_focus;
        out.cmd_overflowed = stats.cmd_overflowed;
        out.text_overflowed = stats.text_overflowed;
        out.blob_overflowed = stats.blob_overflowed;
        out.workspace_overflowed = stats.workspace_overflowed;
        return out;
    }

    ExecStats to_scene_stats(const ui::draw_cmd::DrawCmdExecStats& stats) noexcept {
        ExecStats out{};
        out.cmd_count = stats.cmd_count;
        out.cmd_bytes = stats.cmd_bytes;
        out.clip_pushes = stats.clip_pushes;
        out.clip_pops = stats.clip_pops;
        out.clip_push_overflow = stats.clip_push_overflow;
        out.clip_pop_underflow = stats.clip_pop_underflow;
        out.clip_invalid = stats.clip_invalid;
        out.failed_cmds = stats.failed_cmds;
        out.fail_text = stats.fail_text;
        out.fail_image = stats.fail_image;
        out.fail_blob = stats.fail_blob;
        out.fail_path = stats.fail_path;
        out.fail_clip = stats.fail_clip;
        out.fail_other = stats.fail_other;
        out.dispatch_groups = stats.dispatch_groups;
        out.batch_flushes = stats.batch_flushes;
        out.group_rect = stats.group_rect;
        out.group_text = stats.group_text;
        out.group_image = stats.group_image;
        out.group_line = stats.group_line;
        out.group_path = stats.group_path;
        out.group_other = stats.group_other;
        out.cmd_rect = stats.cmd_rect;
        out.cmd_text = stats.cmd_text;
        out.cmd_image = stats.cmd_image;
        out.cmd_line = stats.cmd_line;
        out.cmd_path = stats.cmd_path;
        out.cmd_other = stats.cmd_other;
        out.overflowed = stats.overflowed;
        return out;
    }

    TileStats to_scene_stats(const ui::draw_cmd::DrawCmdTileStats& stats) noexcept {
        TileStats out{};
        out.tiles_total = stats.tiles_total;
        out.tiles_drawn = stats.tiles_drawn;
        out.cmd_count = stats.cmd_count;
        out.cmd_bytes = stats.cmd_bytes;
        out.tile_flush_count = stats.tile_flush_count;
        out.clip_push_overflow = stats.clip_push_overflow;
        out.clip_pop_underflow = stats.clip_pop_underflow;
        out.clip_invalid = stats.clip_invalid;
        out.dispatch_groups = stats.dispatch_groups;
        out.batch_flushes = stats.batch_flushes;
        out.failed_cmds = stats.failed_cmds;
        out.fail_text = stats.fail_text;
        out.fail_image = stats.fail_image;
        out.fail_blob = stats.fail_blob;
        out.fail_path = stats.fail_path;
        out.fail_clip = stats.fail_clip;
        out.fail_other = stats.fail_other;
        out.group_rect = stats.group_rect;
        out.group_text = stats.group_text;
        out.group_image = stats.group_image;
        out.group_line = stats.group_line;
        out.group_path = stats.group_path;
        out.group_other = stats.group_other;
        out.cmd_rect = stats.cmd_rect;
        out.cmd_text = stats.cmd_text;
        out.cmd_image = stats.cmd_image;
        out.cmd_line = stats.cmd_line;
        out.cmd_path = stats.cmd_path;
        out.cmd_other = stats.cmd_other;
        return out;
    }
}
