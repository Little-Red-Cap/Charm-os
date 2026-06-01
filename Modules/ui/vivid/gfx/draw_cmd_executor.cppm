module;

#include "vivid_features.generated.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module charm.gfx.draw_cmd:executor;

import :schema;
import :buffer;
import charm.core.geometry;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.framebuffer;
import charm.gfx.image;
import charm.gfx.path;
import charm.gfx.render_core;
import charm.font;
import charm.font.typography;
import charm.gfx.text_box;
import ui.render_backend;

export namespace ui::draw_cmd {
    struct DrawCmdTileConfig {
        int tile_width{64};
        int tile_height{64};
        rgba clear_color{0, 0, 0, 0};
        bool clear_tile{true};
    };

    struct DrawCmdTileStats {
        int tiles_total{0};
        int tiles_drawn{0};
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
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

    class DrawCmdExecutor {
    public:
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        [[nodiscard]] DrawCmdDetailStats last_detail_stats() const noexcept { return last_detail_stats_; }
#else
        [[nodiscard]] DrawCmdDetailStats last_detail_stats() const noexcept { return {}; }
#endif

        template <class Buffer>
        DrawCmdExecStats execute(CanvasBase& canvas,
                                 const Buffer& buf,
                                 const Rect* initial_clip = nullptr) noexcept {
            DrawCmdExecStats stats{};
            stats.cmd_count = buf.size();
            stats.cmd_bytes = buf.cmd_bytes();
            stats.overflowed = buf.overflowed();
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
            last_detail_stats_ = {};
#endif

            std::array<CanvasBase::ClipState, 64> clip_stack{};
            std::size_t sp = 0;
            const auto base_clip = canvas.save_clip();
            if (initial_clip) {
                canvas.set_clip(*initial_clip);
            }

            const std::size_t cmd_bytes = buf.cmd_bytes();
            std::size_t offset = 0;
            auto read_cmd_at_offset = [&](std::size_t at, DrawCmd& out, std::size_t& stride) noexcept -> bool {
                return buf.read_cmd_at_offset(at, out, stride);
            };
            auto fail_text = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_text++;
            };
            auto fail_image = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_image++;
            };
            auto fail_blob = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_blob++;
            };
            auto fail_path = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_path++;
            };
            auto fail_clip = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_clip++;
            };
            auto fail_other = [&]() noexcept {
                stats.failed_cmds++;
                stats.fail_other++;
            };
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
            auto rect_area_u32 = [](const Rect& r) noexcept -> std::uint32_t {
                const Rect n = rect_normalized(r);
                if (n.w <= 0 || n.h <= 0) return 0U;
                const auto area = static_cast<std::uint64_t>(n.w) * static_cast<std::uint64_t>(n.h);
                return area > 0xFFFFFFFFULL ? 0xFFFFFFFFU : static_cast<std::uint32_t>(area);
            };
            auto scope_bucket = [&](DrawScope scope) noexcept -> DrawScopeDetail* {
                for (auto& bucket : last_detail_stats_.scopes) {
                    if (bucket.active != 0U && bucket.scope_id == scope.id) return &bucket;
                }
                for (auto& bucket : last_detail_stats_.scopes) {
                    if (bucket.active == 0U) {
                        bucket = DrawScopeDetail{};
                        bucket.active = 1U;
                        bucket.scope_id = scope.id;
                        return &bucket;
                    }
                }
                ++last_detail_stats_.scope_overflow;
                return nullptr;
            };
            auto note_detail = [&](const DrawCmd& cur, const Rect& rect, std::uint64_t alpha_delta) noexcept {
                const auto idx = static_cast<std::size_t>(cur.type);
                if (idx >= last_detail_stats_.types.size()) return;
                const std::uint32_t area = rect_area_u32(rect);
                auto& type = last_detail_stats_.types[idx];
                ++type.count;
                type.rect_area += area;
                type.actual_alpha_pixels += alpha_delta;
                if (area > type.max_rect_area) type.max_rect_area = area;
                if (auto* scope = scope_bucket(cur.draw_scope)) {
                    ++scope->cmd_count;
                    scope->rect_area += area;
                    scope->actual_alpha_pixels += alpha_delta;
                }
            };
            auto alpha_before = []() noexcept -> std::uint64_t { return alpha_blend_count(); };
            auto alpha_delta_since = [](std::uint64_t before) noexcept -> std::uint64_t {
                const std::uint64_t after = alpha_blend_count();
                return after >= before ? (after - before) : 0ULL;
            };
            auto note_failed_detail = [&](const DrawCmd& cur, const Rect& rect) noexcept {
                note_detail(cur, rect, 0ULL);
            };
#else
            auto note_failed_detail = [](const DrawCmd&, const Rect&) noexcept {};
#endif
            auto draw_linear_gradient_rect = [&](const DrawCmd& cur, const Rect& rect) noexcept {
                const int radius = cur.p0;
                const bool vertical = cur.p1 != 0;
                const rgba start = cur.color;
                const rgba end{
                    static_cast<std::uint8_t>((static_cast<std::uint16_t>(cur.p2) >> 8) & 0xFF),
                    static_cast<std::uint8_t>(static_cast<std::uint16_t>(cur.p2) & 0xFF),
                    static_cast<std::uint8_t>((static_cast<std::uint16_t>(cur.p3) >> 8) & 0xFF),
                    static_cast<std::uint8_t>(static_cast<std::uint16_t>(cur.p3) & 0xFF)
                };
                auto inside_round = [&](int x, int y, const Rect& rr) noexcept {
                    if (radius <= 0) return true;
                    const int left = rr.x;
                    const int top = rr.y;
                    const int right = rr.x + rr.w - 1;
                    const int bottom = rr.y + rr.h - 1;
                    if (x >= left + radius && x <= right - radius) return true;
                    if (y >= top + radius && y <= bottom - radius) return true;
                    const int cx = (x < left + radius) ? (left + radius) : (right - radius);
                    const int cy = (y < top + radius) ? (top + radius) : (bottom - radius);
                    const int dx = x - cx;
                    const int dy = y - cy;
                    return dx * dx + dy * dy <= radius * radius;
                };
                const int span = vertical
                    ? std::max<int>(1, static_cast<int>(rect.h) - 1)
                    : std::max<int>(1, static_cast<int>(rect.w) - 1);
                for (int y = rect.y; y < rect.y + rect.h; ++y) {
                    for (int x = rect.x; x < rect.x + rect.w; ++x) {
                        if (!inside_round(x, y, rect)) continue;
                        const int t = vertical ? (y - rect.y) : (x - rect.x);
                        const auto lerp = [&](std::uint8_t a, std::uint8_t b) noexcept -> std::uint8_t {
                            return static_cast<std::uint8_t>(a + (static_cast<int>(b) - static_cast<int>(a)) * t / span);
                        };
                        canvas.set_pixel(x, y, rgba{
                            lerp(start.r, end.r),
                            lerp(start.g, end.g),
                            lerp(start.b, end.b),
                            lerp(start.a, end.a)
                        });
                    }
                }
            };
            auto draw_rectlike_shape = [&](const DrawCmd& cur, const Rect& rect) noexcept {
                switch (cur.type) {
                case CmdType::FillRect:
                    ui::render::draw_rect(canvas, rect.x, rect.y, rect.w, rect.h, cur.color, true);
                    break;
                case CmdType::FillLinearGradientRect:
                    draw_linear_gradient_rect(cur, rect);
                    break;
                case CmdType::StrokeRect:
                    ui::render::draw_rect(canvas, rect.x, rect.y, rect.w, rect.h, cur.color, false);
                    break;
                case CmdType::FillRoundRect:
                    ui::render::draw_round_rect(canvas, rect.x, rect.y, rect.w, rect.h, cur.p0, cur.color, true);
                    break;
                case CmdType::StrokeRoundRect:
                    ui::render::draw_round_rect(canvas, rect.x, rect.y, rect.w, rect.h, cur.p0, cur.color, false);
                    break;
                case CmdType::FillCircle: {
                    const int radius = cur.p0;
                    const int cx = rect.x + rect.w / 2;
                    const int cy = rect.y + rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cur.color, true);
                    break;
                }
                case CmdType::StrokeCircle: {
                    const int radius = cur.p0;
                    const int cx = rect.x + rect.w / 2;
                    const int cy = rect.y + rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cur.color, false);
                    break;
                }
                case CmdType::FocusRing:
                    ui::render::draw_focus_ring(canvas, rect, cur.color, cur.p0, true, cur.p1, cur.p2);
                    break;
                default:
                    break;
                }
            };
            auto exec_rect_like = [&](const DrawCmd& cur) noexcept {
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                const std::uint64_t before = alpha_before();
#endif
                draw_rectlike_shape(cur, cur.rect);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                note_detail(cur, cur.rect, alpha_delta_since(before));
#endif
            };
            auto rectlike_batchable = [](CmdType type) noexcept {
                switch (type) {
                case CmdType::FillRect:
                case CmdType::FillLinearGradientRect:
                case CmdType::StrokeRect:
                case CmdType::FillRoundRect:
                case CmdType::StrokeRoundRect:
                case CmdType::FillCircle:
                case CmdType::StrokeCircle:
                case CmdType::FocusRing:
                    return true;
                default:
                    return false;
                }
            };
            auto rectlike_params_match = [&](const DrawCmd& a, const DrawCmd& b) noexcept {
                if (a.type != b.type) return false;
                if (!rgba_equal(a.color, b.color)) return false;
                switch (a.type) {
                case CmdType::FillRect:
                case CmdType::StrokeRect:
                    return true;
                case CmdType::FillLinearGradientRect:
                    return a.p0 == b.p0 && a.p1 == b.p1 && a.p2 == b.p2 && a.p3 == b.p3;
                case CmdType::FillRoundRect:
                case CmdType::StrokeRoundRect:
                case CmdType::FillCircle:
                case CmdType::StrokeCircle:
                    return a.p0 == b.p0;
                case CmdType::FocusRing:
                    return a.p0 == b.p0 && a.p1 == b.p1 && a.p2 == b.p2;
                default:
                    return false;
                }
            };
            auto exec_rectlike_item = [&](const DrawCmd& cur, const Rect& rect) noexcept {
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                const std::uint64_t before = alpha_before();
#endif
                draw_rectlike_shape(cur, rect);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                note_detail(cur, rect, alpha_delta_since(before));
#endif
            };

            auto draw_imagelike_shape = [&](const DrawCmd& cur, const Rect& rect) noexcept {
                const auto* image = resolve_image(cur.image);
                if (!image || !(*image)) {
                    fail_image();
                    note_failed_detail(cur, rect);
                    return;
                }

                switch (cur.type) {
                case CmdType::DrawImage:
                    if (rect.w > 0 && rect.h > 0) {
                        ui::render::draw_image_scaled(canvas, rect.x, rect.y, rect.w, rect.h, *image);
                    } else {
                        ui::render::draw_image(canvas, rect.x, rect.y, *image);
                    }
                    break;
                case CmdType::DrawImageRoundRect:
                    if (rect.w > 0 && rect.h > 0) {
                        ui::render::draw_image_scaled_shaped(
                            canvas,
                            rect.x,
                            rect.y,
                            rect.w,
                            rect.h,
                            *image,
                            cur.p0,
                            static_cast<ui::render::ImageShapeKind>(cur.p1),
                            cur.p2);
                    } else {
                        ui::render::draw_image_shaped(
                            canvas,
                            rect.x,
                            rect.y,
                            *image,
                            cur.p0,
                            static_cast<ui::render::ImageShapeKind>(cur.p1),
                            cur.p2);
                    }
                    break;
                case CmdType::DrawImageNineSlice:
                    ui::render::draw_image_nine_slice(canvas,
                                                      rect.x, rect.y,
                                                      rect.w, rect.h,
                                                      *image,
                                                      cur.p0, cur.p1, cur.p2, cur.p3);
                    break;
                default:
                    break;
                }
            };

            auto exec_image_like = [&](const DrawCmd& cur) noexcept {
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                const std::uint64_t before = alpha_before();
#endif
                draw_imagelike_shape(cur, cur.rect);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                note_detail(cur, cur.rect, alpha_delta_since(before));
#endif
            };
            auto imagelike_batchable = [](CmdType type) noexcept {
                switch (type) {
                case CmdType::DrawImage:
                case CmdType::DrawImageRoundRect:
                case CmdType::DrawImageNineSlice:
                    return true;
                default:
                    return false;
                }
            };
            auto imagelike_params_match = [&](const DrawCmd& a, const DrawCmd& b) noexcept {
                if (a.type != b.type) return false;
                if (a.image != b.image) return false;
                switch (a.type) {
                case CmdType::DrawImage:
                    return true;
                case CmdType::DrawImageRoundRect:
                    return a.p0 == b.p0 && a.p1 == b.p1 && a.p2 == b.p2;
                case CmdType::DrawImageNineSlice:
                    return a.p0 == b.p0 && a.p1 == b.p1 && a.p2 == b.p2 && a.p3 == b.p3;
                default:
                    return false;
                }
            };
            auto exec_imagelike_item = [&](const DrawCmd& cur, const Rect& rect) noexcept {
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                const std::uint64_t before = alpha_before();
#endif
                draw_imagelike_shape(cur, rect);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                note_detail(cur, rect, alpha_delta_since(before));
#endif
            };

            auto exec_draw_line = [&](const DrawCmd& cur) noexcept {
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                const std::uint64_t before = alpha_before();
#endif
                ui::render::draw_line(canvas,
                                      cur.rect.x,
                                      cur.rect.y,
                                      cur.rect.w,
                                      cur.rect.h,
                                      cur.color);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                note_detail(cur, line_bounds(cur.rect.x, cur.rect.y, cur.rect.w, cur.rect.h), alpha_delta_since(before));
#endif
            };

            auto exec_draw_path = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p0;
                if (count < 2) {
                    fail_path();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                const auto points = ui::gfx::path::decode_points(blob, count);
                if (points.empty()) {
                    fail_path();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const bool closed = (cur.p1 != 0);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                const std::uint64_t before = alpha_before();
#endif
                ui::gfx::path::stroke_path(canvas, points, closed, cur.color);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                note_detail(cur, cur.rect, alpha_delta_since(before));
#endif
            };

            auto exec_line_batch = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p0;
                if (count <= 0) {
                    fail_other();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(LineBatchItem)) {
                    fail_blob();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto items = std::span<const LineBatchItem>(
                    reinterpret_cast<const LineBatchItem*>(blob.data()), count);
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    const std::uint64_t before = alpha_before();
#endif
                    ui::render::draw_line(canvas, item.x0, item.y0, item.x1, item.y1, cur.color);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, line_bounds(item.x0, item.y0, item.x1, item.y1), alpha_delta_since(before));
#endif
                }
            };

            auto exec_path_batch = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p0;
                if (count <= 0) {
                    fail_other();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(PathBatchItem)) {
                    fail_blob();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto items = std::span<const PathBatchItem>(
                    reinterpret_cast<const PathBatchItem*>(blob.data()), count);
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
                    if (item.count < 2) {
                        fail_path();
                        note_failed_detail(cur, cur.rect);
                        continue;
                    }
                    const auto path_blob = buf.blob_at(item.blob);
                    const auto points = ui::gfx::path::decode_points(path_blob, item.count);
                    if (points.empty()) {
                        fail_path();
                        note_failed_detail(cur, cur.rect);
                        continue;
                    }
                    const bool closed = (item.closed != 0);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    Rect item_bounds{};
                    (void)ui::gfx::path::compute_bounds(points.data(), item.count, item_bounds);
                    const std::uint64_t before = alpha_before();
#endif
                    ui::gfx::path::stroke_path(canvas, points, closed, cur.color);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, item_bounds, alpha_delta_since(before));
#endif
                }
            };

            auto exec_rect_batch = [&](const DrawCmd& cur, bool fill) noexcept {
                const int count = cur.p0;
                if (count <= 0) {
                    fail_other();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(RectBatchItem)) {
                    fail_blob();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto items = std::span<const RectBatchItem>(
                    reinterpret_cast<const RectBatchItem*>(blob.data()), count);
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    const std::uint64_t before = alpha_before();
#endif
                    ui::render::draw_rect(canvas,
                                          item.rect.x, item.rect.y,
                                          item.rect.w, item.rect.h,
                                          cur.color, fill);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, item.rect, alpha_delta_since(before));
#endif
                }
            };

            auto exec_round_batch = [&](const DrawCmd& cur, bool fill) noexcept {
                const int count = cur.p1;
                if (count <= 0) {
                    fail_other();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(RectBatchItem)) {
                    fail_blob();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto items = std::span<const RectBatchItem>(
                    reinterpret_cast<const RectBatchItem*>(blob.data()), count);
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    const std::uint64_t before = alpha_before();
#endif
                    ui::render::draw_round_rect(canvas,
                                                item.rect.x, item.rect.y,
                                                item.rect.w, item.rect.h,
                                                cur.p0,
                                                cur.color,
                                                fill);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, item.rect, alpha_delta_since(before));
#endif
                }
            };

            auto exec_circle_batch = [&](const DrawCmd& cur, bool fill) noexcept {
                const int count = cur.p1;
                if (count <= 0) {
                    fail_other();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(RectBatchItem)) {
                    fail_blob();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto items = std::span<const RectBatchItem>(
                    reinterpret_cast<const RectBatchItem*>(blob.data()), count);
                const int radius = cur.p0;
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
                    const int cx = item.rect.x + item.rect.w / 2;
                    const int cy = item.rect.y + item.rect.h / 2;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    const std::uint64_t before = alpha_before();
#endif
                    ui::render::draw_circle(canvas, cx, cy, radius, cur.color, fill);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, item.rect, alpha_delta_since(before));
#endif
                }
            };

            auto exec_focus_batch = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p3;
                if (count <= 0) {
                    fail_other();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(RectBatchItem)) {
                    fail_blob();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto items = std::span<const RectBatchItem>(
                    reinterpret_cast<const RectBatchItem*>(blob.data()), count);
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    const std::uint64_t before = alpha_before();
#endif
                    ui::render::draw_focus_ring(canvas, item.rect, cur.color, cur.p0, true, cur.p1, cur.p2);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, item.rect, alpha_delta_since(before));
#endif
                }
            };

            auto exec_glyph_run = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p0;
                if (count <= 0) {
                    fail_other();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(GlyphRunItem)) {
                    fail_blob();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto items = std::span<const GlyphRunItem>(
                    reinterpret_cast<const GlyphRunItem*>(blob.data()), count);
                const Font& font = cur.font_ptr ? *cur.font_ptr : get_font(cur.font);
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
                    if (!buf.text_span_valid(item.text)) {
                        fail_text();
                        note_failed_detail(cur, item.rect);
                        continue;
                    }
                    const char* text = buf.text_at(item.text.offset);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    const std::uint64_t before = alpha_before();
#endif
                    draw_text_box(canvas, item.rect, text, cur.color, font,
                                  cur.align_h, cur.align_v, cur.wrap, cur.ellipsis);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, item.rect, alpha_delta_since(before));
#endif
                }
            };

            auto exec_image_batch = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p0;
                if (count <= 0) {
                    fail_other();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto* image = resolve_image(cur.image);
                if (!image || !(*image)) {
                    fail_image();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(ImageBatchItem)) {
                    fail_blob();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto items = std::span<const ImageBatchItem>(
                    reinterpret_cast<const ImageBatchItem*>(blob.data()), count);
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    const std::uint64_t before = alpha_before();
#endif
                    if (item.rect.w > 0 && item.rect.h > 0) {
                        ui::render::draw_image_scaled(canvas, item.rect.x, item.rect.y,
                                                      item.rect.w, item.rect.h, *image);
                    } else {
                        ui::render::draw_image(canvas, item.rect.x, item.rect.y, *image);
                    }
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, item.rect, alpha_delta_since(before));
#endif
                }
            };

            auto exec_image_round_batch = [&](const DrawCmd& cur) noexcept {
                const int count = cur.p3;
                if (count <= 0) {
                    fail_other();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto* image = resolve_image(cur.image);
                if (!image || !(*image)) {
                    fail_image();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < static_cast<std::size_t>(count) * sizeof(ImageBatchItem)) {
                    fail_blob();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto items = std::span<const ImageBatchItem>(
                    reinterpret_cast<const ImageBatchItem*>(blob.data()), count);
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    const std::uint64_t before = alpha_before();
#endif
                    if (item.rect.w > 0 && item.rect.h > 0) {
                        ui::render::draw_image_scaled_shaped(
                            canvas,
                            item.rect.x,
                            item.rect.y,
                            item.rect.w,
                            item.rect.h,
                            *image,
                            cur.p0,
                            static_cast<ui::render::ImageShapeKind>(cur.p1),
                            cur.p2);
                    } else {
                        ui::render::draw_image_shaped(
                            canvas,
                            item.rect.x,
                            item.rect.y,
                            *image,
                            cur.p0,
                            static_cast<ui::render::ImageShapeKind>(cur.p1),
                            cur.p2);
                    }
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, item.rect, alpha_delta_since(before));
#endif
                }
            };

            auto exec_image_nine_batch = [&](const DrawCmd& cur) noexcept {
                const auto* image = resolve_image(cur.image);
                if (!image || !(*image)) {
                    fail_image();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto blob = buf.blob_at(cur.blob);
                if (blob.size() < sizeof(ImageBatchItem)
                    || (blob.size() % sizeof(ImageBatchItem)) != 0) {
                    fail_blob();
                    note_failed_detail(cur, cur.rect);
                    return;
                }
                const auto count = static_cast<int>(blob.size() / sizeof(ImageBatchItem));
                const auto items = std::span<const ImageBatchItem>(
                    reinterpret_cast<const ImageBatchItem*>(blob.data()), count);
                for (std::size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    const std::uint64_t before = alpha_before();
#endif
                    ui::render::draw_image_nine_slice(canvas,
                                                      item.rect.x, item.rect.y,
                                                      item.rect.w, item.rect.h,
                                                      *image,
                                                      cur.p0, cur.p1, cur.p2, cur.p3);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, item.rect, alpha_delta_since(before));
#endif
                }
            };

            enum class GroupKind : std::uint8_t {
                None,
                RectLike,
                TextBox,
                ImageLike,
                DrawLine,
                DrawPath
            };

            auto group_kind = [](CmdType type) noexcept -> GroupKind {
                switch (type) {
                case CmdType::FillRect:
                case CmdType::FillLinearGradientRect:
                case CmdType::StrokeRect:
                case CmdType::FillRoundRect:
                case CmdType::StrokeRoundRect:
                case CmdType::FillCircle:
                case CmdType::StrokeCircle:
                case CmdType::FillRectBatch:
                case CmdType::StrokeRectBatch:
                case CmdType::FillRoundRectBatch:
                case CmdType::StrokeRoundRectBatch:
                case CmdType::FillCircleBatch:
                case CmdType::StrokeCircleBatch:
                case CmdType::FocusRing:
                case CmdType::FocusRingBatch:
                    return GroupKind::RectLike;
                case CmdType::DrawTextBox:
                case CmdType::GlyphRun:
                    return GroupKind::TextBox;
                case CmdType::DrawImage:
                case CmdType::DrawImageRoundRect:
                case CmdType::DrawImageNineSlice:
                case CmdType::DrawImageBatch:
                case CmdType::DrawImageRoundRectBatch:
                case CmdType::DrawImageNineSliceBatch:
                    return GroupKind::ImageLike;
                case CmdType::DrawLine:
                case CmdType::DrawLineBatch:
                    return GroupKind::DrawLine;
                case CmdType::DrawPath:
                case CmdType::DrawPathBatch:
                    return GroupKind::DrawPath;
                default:
                    return GroupKind::None;
                }
            };

            auto exec_group_cmd = [&](const DrawCmd& cur, GroupKind kind) noexcept {
                switch (kind) {
                case GroupKind::RectLike:
                    switch (cur.type) {
                    case CmdType::FillRect:
                    case CmdType::FillLinearGradientRect:
                    case CmdType::StrokeRect:
                    case CmdType::FillRoundRect:
                    case CmdType::StrokeRoundRect:
                    case CmdType::FillCircle:
                    case CmdType::StrokeCircle:
                        exec_rect_like(cur);
                        break;
                    case CmdType::FillRectBatch:
                        exec_rect_batch(cur, true);
                        break;
                    case CmdType::StrokeRectBatch:
                        exec_rect_batch(cur, false);
                        break;
                    case CmdType::FillRoundRectBatch:
                        exec_round_batch(cur, true);
                        break;
                    case CmdType::StrokeRoundRectBatch:
                        exec_round_batch(cur, false);
                        break;
                    case CmdType::FillCircleBatch:
                        exec_circle_batch(cur, true);
                        break;
                    case CmdType::StrokeCircleBatch:
                        exec_circle_batch(cur, false);
                        break;
                    case CmdType::FocusRing:
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    {
                        const std::uint64_t before = alpha_before();
                        ui::render::draw_focus_ring(canvas, cur.rect, cur.color, cur.p0, true, cur.p1, cur.p2);
                        note_detail(cur, cur.rect, alpha_delta_since(before));
                        break;
                    }
#else
                        ui::render::draw_focus_ring(canvas, cur.rect, cur.color, cur.p0, true, cur.p1, cur.p2);
                        break;
#endif
                    case CmdType::FocusRingBatch:
                        exec_focus_batch(cur);
                        break;
                    default:
                        break;
                    }
                    break;
                case GroupKind::TextBox: {
                    if (cur.type == CmdType::GlyphRun) {
                        exec_glyph_run(cur);
                        break;
                    }
                    if (!buf.text_span_valid(cur.text)) {
                        fail_text();
                        note_failed_detail(cur, cur.rect);
                        return;
                    }
                    const char* text = buf.text_at(cur.text.offset);
                    const Font& font = cur.font_ptr ? *cur.font_ptr : get_font(cur.font);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    const std::uint64_t before = alpha_before();
#endif
                    draw_text_box(canvas, cur.rect, text, cur.color, font,
                                  cur.align_h, cur.align_v, cur.wrap, cur.ellipsis);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                    note_detail(cur, cur.rect, alpha_delta_since(before));
#endif
                    break;
                }
                case GroupKind::ImageLike:
                    switch (cur.type) {
                    case CmdType::DrawImage:
                    case CmdType::DrawImageRoundRect:
                    case CmdType::DrawImageNineSlice:
                        exec_image_like(cur);
                        break;
                    case CmdType::DrawImageBatch:
                        exec_image_batch(cur);
                        break;
                    case CmdType::DrawImageRoundRectBatch:
                        exec_image_round_batch(cur);
                        break;
                    case CmdType::DrawImageNineSliceBatch:
                        exec_image_nine_batch(cur);
                        break;
                    default:
                        break;
                    }
                    break;
                case GroupKind::DrawLine:
                    if (cur.type == CmdType::DrawLineBatch) {
                        exec_line_batch(cur);
                    } else {
                        exec_draw_line(cur);
                    }
                    break;
                case GroupKind::DrawPath:
                    if (cur.type == CmdType::DrawPathBatch) {
                        exec_path_batch(cur);
                    } else {
                        exec_draw_path(cur);
                    }
                    break;
                case GroupKind::None:
                default:
                    break;
                }
            };

            auto exec_ungrouped_cmd = [&](const DrawCmd& cur) noexcept {
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                note_detail(cur, cur.rect, 0ULL);
#endif
                if (cur.type == CmdType::PushClip) {
                    if (cur.rect.w <= 0 || cur.rect.h <= 0) {
                        stats.clip_invalid++;
                        fail_clip();
                        return;
                    }
                    if (sp < clip_stack.size()) {
                        clip_stack[sp++] = canvas.save_clip();
                        canvas.set_clip(cur.rect);
                        stats.clip_pushes++;
                    } else {
                        stats.clip_push_overflow++;
                        fail_clip();
                    }
                    return;
                }

                if (cur.type == CmdType::PopClip) {
                    if (sp > 0) {
                        canvas.restore_clip(clip_stack[--sp]);
                        stats.clip_pops++;
                    } else {
                        stats.clip_pop_underflow++;
                        fail_clip();
                    }
                }
            };

            auto count_group = [&](GroupKind kind) noexcept {
                switch (kind) {
                case GroupKind::RectLike:
                    stats.group_rect++;
                    break;
                case GroupKind::TextBox:
                    stats.group_text++;
                    break;
                case GroupKind::ImageLike:
                    stats.group_image++;
                    break;
                case GroupKind::DrawLine:
                    stats.group_line++;
                    break;
                case GroupKind::DrawPath:
                    stats.group_path++;
                    break;
                case GroupKind::None:
                default:
                    stats.group_other++;
                    break;
                }
            };

            auto count_cmd = [&](GroupKind kind) noexcept {
                switch (kind) {
                case GroupKind::RectLike:
                    stats.cmd_rect++;
                    break;
                case GroupKind::TextBox:
                    stats.cmd_text++;
                    break;
                case GroupKind::ImageLike:
                    stats.cmd_image++;
                    break;
                case GroupKind::DrawLine:
                    stats.cmd_line++;
                    break;
                case GroupKind::DrawPath:
                    stats.cmd_path++;
                    break;
                case GroupKind::None:
                default:
                    stats.cmd_other++;
                    break;
                }
            };
            auto text_params_match = [&](const DrawCmd& a, const DrawCmd& b) noexcept {
                return rgba_equal(a.color, b.color)
                    && draw_cmd_scope_equal(a, b)
                    && a.font_ptr == b.font_ptr
                    && a.font == b.font
                    && a.align_h == b.align_h
                    && a.align_v == b.align_v
                    && a.wrap == b.wrap
                    && a.ellipsis == b.ellipsis;
            };

            while (offset < cmd_bytes) {
                DrawCmd cmd{};
                std::size_t stride = 0;
                if (!read_cmd_at_offset(offset, cmd, stride)) {
                    fail_other();
                    continue;
                }

                const auto kind = group_kind(cmd.type);
                if (kind != GroupKind::None) {
                    stats.dispatch_groups++;
                    count_group(kind);
                    if (kind == GroupKind::RectLike) {
                        constexpr std::size_t kMaxExecBatchItems = 64;
                        std::array<RectBatchItem, kMaxExecBatchItems> rect_items{};
                        while (true) {
                            DrawCmd cur = cmd;
                            std::size_t cur_stride = stride;
                            if (rectlike_batchable(cur.type)) {
                                std::size_t run = 1;
                                rect_items[0].rect = cur.rect;
                                std::size_t scan_offset = offset + cur_stride;
                                while (scan_offset < cmd_bytes && run < kMaxExecBatchItems) {
                                    DrawCmd next{};
                                    std::size_t next_stride = 0;
                                    if (!read_cmd_at_offset(scan_offset, next, next_stride)) {
                                        fail_other();
                                        break;
                                    }
                                    if (group_kind(next.type) != kind) break;
                                    if (!draw_cmd_scope_equal(cur, next)) break;
                                    if (!rectlike_batchable(next.type)) break;
                                    if (!rectlike_params_match(cur, next)) break;
                                    rect_items[run].rect = next.rect;
                                    scan_offset += next_stride;
                                    ++run;
                                }
                                for (std::size_t j = 0; j < run; ++j) {
                                    exec_rectlike_item(cur, rect_items[j].rect);
                                    count_cmd(kind);
                                }
                                offset = scan_offset;
                            } else {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            }
                            if (offset >= cmd_bytes) break;
                            if (!read_cmd_at_offset(offset, cmd, stride)) {
                                fail_other();
                                offset = cmd_bytes;
                                continue;
                            }
                            if (group_kind(cmd.type) != kind) break;
                        }
                        stats.batch_flushes++;
                        continue;
                    }
                    if (kind == GroupKind::ImageLike) {
                        constexpr std::size_t kMaxExecBatchItems = 64;
                        std::array<RectBatchItem, kMaxExecBatchItems> rect_items{};
                        while (true) {
                            DrawCmd cur = cmd;
                            std::size_t cur_stride = stride;
                            if (imagelike_batchable(cur.type)) {
                                std::size_t run = 1;
                                rect_items[0].rect = cur.rect;
                                std::size_t scan_offset = offset + cur_stride;
                                while (scan_offset < cmd_bytes && run < kMaxExecBatchItems) {
                                    DrawCmd next{};
                                    std::size_t next_stride = 0;
                                    if (!read_cmd_at_offset(scan_offset, next, next_stride)) {
                                        fail_other();
                                        break;
                                    }
                                    if (group_kind(next.type) != kind) break;
                                    if (!draw_cmd_scope_equal(cur, next)) break;
                                    if (!imagelike_batchable(next.type)) break;
                                    if (!imagelike_params_match(cur, next)) break;
                                    rect_items[run].rect = next.rect;
                                    scan_offset += next_stride;
                                    ++run;
                                }
                                for (std::size_t j = 0; j < run; ++j) {
                                    exec_imagelike_item(cur, rect_items[j].rect);
                                    count_cmd(kind);
                                }
                                offset = scan_offset;
                            } else {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            }
                            if (offset >= cmd_bytes) break;
                            if (!read_cmd_at_offset(offset, cmd, stride)) {
                                fail_other();
                                offset = cmd_bytes;
                                continue;
                            }
                            if (group_kind(cmd.type) != kind) break;
                        }
                        stats.batch_flushes++;
                        continue;
                    }
                    if (kind == GroupKind::TextBox) {
                        constexpr std::size_t kMaxExecBatchItems = 64;
                        while (true) {
                            DrawCmd cur = cmd;
                            std::size_t cur_stride = stride;
                            if (cur.type == CmdType::GlyphRun) {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            } else if (cur.type == CmdType::DrawTextBox) {
                                if (!buf.text_span_valid(cur.text)) {
                                    fail_text();
                                    offset += cur_stride;
                                } else {
                                    std::size_t run = 1;
                                    std::size_t scan_offset = offset + cur_stride;
                                    while (scan_offset < cmd_bytes && run < kMaxExecBatchItems) {
                                        DrawCmd next{};
                                        std::size_t next_stride = 0;
                                        if (!read_cmd_at_offset(scan_offset, next, next_stride)) {
                                            fail_other();
                                            break;
                                        }
                                        if (group_kind(next.type) != kind) break;
                                        if (!draw_cmd_scope_equal(cur, next)) break;
                                        if (next.type != CmdType::DrawTextBox) break;
                                        if (!buf.text_span_valid(next.text)) break;
                                        if (!text_params_match(cur, next)) break;
                                        scan_offset += next_stride;
                                        ++run;
                                    }
                                    std::size_t item_offset = offset;
                                    for (std::size_t j = 0; j < run; ++j) {
                                        DrawCmd item{};
                                        std::size_t item_stride = 0;
                                        if (!read_cmd_at_offset(item_offset, item, item_stride)) {
                                            fail_other();
                                            break;
                                        }
                                    if (!buf.text_span_valid(item.text)) {
                                        fail_text();
                                        note_failed_detail(item, item.rect);
                                        item_offset += item_stride;
                                        continue;
                                    }
                                    const char* text = buf.text_at(item.text.offset);
                                    const Font& font = item.font_ptr ? *item.font_ptr : get_font(item.font);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                    const std::uint64_t before = alpha_before();
#endif
                                    draw_text_box(canvas, item.rect, text, item.color, font,
                                                  item.align_h, item.align_v, item.wrap, item.ellipsis);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
                                    note_detail(item, item.rect, alpha_delta_since(before));
#endif
                                    count_cmd(kind);
                                        item_offset += item_stride;
                                    }
                                    offset = scan_offset;
                                }
                            } else {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            }
                            if (offset >= cmd_bytes) break;
                            if (!read_cmd_at_offset(offset, cmd, stride)) {
                                fail_other();
                                offset = cmd_bytes;
                                continue;
                            }
                            if (group_kind(cmd.type) != kind) break;
                        }
                        stats.batch_flushes++;
                        continue;
                    }
                    if (kind == GroupKind::DrawLine) {
                        constexpr std::size_t kMaxExecBatchItems = 64;
                        std::array<DrawCmd, kMaxExecBatchItems> line_items{};
                        while (true) {
                            DrawCmd cur = cmd;
                            std::size_t cur_stride = stride;
                            if (cur.type == CmdType::DrawLineBatch) {
                                exec_line_batch(cur);
                                count_cmd(kind);
                                offset += cur_stride;
                            } else if (cur.type == CmdType::DrawLine) {
                                std::size_t run = 1;
                                line_items[0] = cur;
                                std::size_t scan_offset = offset + cur_stride;
                                while (scan_offset < cmd_bytes && run < kMaxExecBatchItems) {
                                    DrawCmd next{};
                                    std::size_t next_stride = 0;
                                    if (!read_cmd_at_offset(scan_offset, next, next_stride)) {
                                        fail_other();
                                        break;
                                    }
                                    if (group_kind(next.type) != kind) break;
                                    if (!draw_cmd_scope_equal(cur, next)) break;
                                    if (next.type != CmdType::DrawLine) break;
                                    if (!rgba_equal(next.color, cur.color)) break;
                                    line_items[run] = next;
                                    scan_offset += next_stride;
                                    ++run;
                                }
                                for (std::size_t j = 0; j < run; ++j) {
                                    exec_draw_line(line_items[j]);
                                    count_cmd(kind);
                                }
                                offset = scan_offset;
                            } else {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            }
                            if (offset >= cmd_bytes) break;
                            if (!read_cmd_at_offset(offset, cmd, stride)) {
                                fail_other();
                                offset = cmd_bytes;
                                continue;
                            }
                            if (group_kind(cmd.type) != kind) break;
                        }
                        stats.batch_flushes++;
                        continue;
                    }
                    if (kind == GroupKind::DrawPath) {
                        constexpr std::size_t kMaxExecBatchItems = 64;
                        std::array<DrawCmd, kMaxExecBatchItems> path_items{};
                        while (true) {
                            DrawCmd cur = cmd;
                            std::size_t cur_stride = stride;
                            if (cur.type == CmdType::DrawPathBatch) {
                                exec_path_batch(cur);
                                count_cmd(kind);
                                offset += cur_stride;
                            } else if (cur.type == CmdType::DrawPath) {
                                std::size_t run = 1;
                                path_items[0] = cur;
                                std::size_t scan_offset = offset + cur_stride;
                                while (scan_offset < cmd_bytes && run < kMaxExecBatchItems) {
                                    DrawCmd next{};
                                    std::size_t next_stride = 0;
                                    if (!read_cmd_at_offset(scan_offset, next, next_stride)) {
                                        fail_other();
                                        break;
                                    }
                                    if (group_kind(next.type) != kind) break;
                                    if (!draw_cmd_scope_equal(cur, next)) break;
                                    if (next.type != CmdType::DrawPath) break;
                                    if (!rgba_equal(next.color, cur.color)) break;
                                    path_items[run] = next;
                                    scan_offset += next_stride;
                                    ++run;
                                }
                                for (std::size_t j = 0; j < run; ++j) {
                                    exec_draw_path(path_items[j]);
                                    count_cmd(kind);
                                }
                                offset = scan_offset;
                            } else {
                                exec_group_cmd(cur, kind);
                                count_cmd(kind);
                                offset += cur_stride;
                            }
                            if (offset >= cmd_bytes) break;
                            if (!read_cmd_at_offset(offset, cmd, stride)) {
                                fail_other();
                                offset = cmd_bytes;
                                continue;
                            }
                            if (group_kind(cmd.type) != kind) break;
                        }
                        stats.batch_flushes++;
                        continue;
                    }
                    exec_group_cmd(cmd, kind);
                    count_cmd(kind);
                    offset += stride;
                    while (offset < cmd_bytes) {
                        DrawCmd cur{};
                        std::size_t cur_stride = 0;
                        if (!read_cmd_at_offset(offset, cur, cur_stride)) {
                            fail_other();
                            offset = cmd_bytes;
                            break;
                        }
                        if (group_kind(cur.type) != kind) break;
                        if (!draw_cmd_scope_equal(cmd, cur)) break;
                        exec_group_cmd(cur, kind);
                        count_cmd(kind);
                        offset += cur_stride;
                    }
                    stats.batch_flushes++;
                    continue;
                }

                stats.dispatch_groups++;
                count_group(GroupKind::None);
                exec_ungrouped_cmd(cmd);
                count_cmd(GroupKind::None);
                stats.batch_flushes++;
                offset += stride;
            }

            if (initial_clip) {
                canvas.restore_clip(base_clip);
            }
            return stats;
        }

        template <ui::RenderBackend Backend, class Buffer>
        DrawCmdTileStats execute_tiles(Backend& backend,
                                       const FrameBufferView& tile_buffer,
                                       const Buffer& buf,
                                       const DrawCmdTileConfig& config) noexcept {
            DrawCmdTileStats stats{};
            stats.cmd_count = buf.size();
            stats.cmd_bytes = buf.cmd_bytes();
            if (!tile_buffer.data) return stats;
            if (config.tile_width <= 0 || config.tile_height <= 0) return stats;

            const int screen_w = backend.width();
            const int screen_h = backend.height();
            const int buffer_w = static_cast<int>(tile_buffer.width);
            const int buffer_h = static_cast<int>(tile_buffer.height);
            if (buffer_w <= 0 || buffer_h <= 0) return stats;

            const int tile_w = (config.tile_width < buffer_w) ? config.tile_width : buffer_w;
            const int tile_h = (config.tile_height < buffer_h) ? config.tile_height : buffer_h;
            const std::size_t stride = (tile_buffer.stride_bytes != 0)
                ? tile_buffer.stride_bytes
                : static_cast<std::size_t>(buffer_w) * bytes_per_pixel(tile_buffer.format);

            RuntimeCanvas tile_canvas(tile_buffer.data,
                                      buffer_w,
                                      buffer_h,
                                      tile_buffer.format,
                                      stride);

            const int tiles_x = (screen_w + tile_w - 1) / tile_w;
            const int tiles_y = (screen_h + tile_h - 1) / tile_h;
            constexpr std::size_t kMaxTileHitEntries = 1024;
            const std::size_t tile_count = static_cast<std::size_t>(tiles_x) * static_cast<std::size_t>(tiles_y);
            std::array<std::uint8_t, kMaxTileHitEntries> tile_hits{};
            const bool use_hit_cache = (tile_count <= kMaxTileHitEntries);
            if (use_hit_cache) {
                Rect screen_rect{0, 0, screen_w, screen_h};
                Rect clipped{};
                auto mark_bounds = [&](const Rect& bounds) noexcept {
                    if (!rect_valid(bounds)) return;
                    if (!rect_intersect(bounds, screen_rect, clipped)) return;
                    int tx0 = clipped.x / tile_w;
                    int ty0 = clipped.y / tile_h;
                    int tx1 = (clipped.x + clipped.w - 1) / tile_w;
                    int ty1 = (clipped.y + clipped.h - 1) / tile_h;
                    if (tx0 < 0) tx0 = 0;
                    if (ty0 < 0) ty0 = 0;
                    if (tx1 >= tiles_x) tx1 = tiles_x - 1;
                    if (ty1 >= tiles_y) ty1 = tiles_y - 1;
                    for (int ty = ty0; ty <= ty1; ++ty) {
                        const std::size_t row = static_cast<std::size_t>(ty) * static_cast<std::size_t>(tiles_x);
                        for (int tx = tx0; tx <= tx1; ++tx) {
                            tile_hits[row + static_cast<std::size_t>(tx)] = 1;
                        }
                    }
                };
                const std::size_t cmd_bytes = buf.cmd_bytes();
                std::size_t offset = 0;
                DrawCmd cmd{};
                while (offset < cmd_bytes) {
                    std::size_t stride = 0;
                    if (!buf.read_cmd_at_offset(offset, cmd, stride)) {
                        break;
                    }
                    if (cmd.type == CmdType::PushClip || cmd.type == CmdType::PopClip) {
                        offset += stride;
                        continue;
                    }
                    if (cmd.type == CmdType::DrawLineBatch) {
                        const int count = cmd.p0;
                        if (count > 0) {
                            const auto blob = buf.blob_at(cmd.blob);
                            if (blob.size() >= static_cast<std::size_t>(count) * sizeof(LineBatchItem)) {
                                const auto items = std::span<const LineBatchItem>(
                                    reinterpret_cast<const LineBatchItem*>(blob.data()), count);
                                for (std::size_t i = 0; i < items.size(); ++i) {
                                    const auto& item = items[i];
                                    mark_bounds(line_bounds(item.x0, item.y0, item.x1, item.y1));
                                }
                            }
                        }
                        offset += stride;
                        continue;
                    }
                    if (cmd.type == CmdType::DrawPathBatch) {
                        const int count = cmd.p0;
                        if (count > 0) {
                            const auto blob = buf.blob_at(cmd.blob);
                            if (blob.size() >= static_cast<std::size_t>(count) * sizeof(PathBatchItem)) {
                                const auto items = std::span<const PathBatchItem>(
                                    reinterpret_cast<const PathBatchItem*>(blob.data()), count);
                                for (std::size_t i = 0; i < items.size(); ++i) {
                                    const auto& item = items[i];
                                    const auto path_blob = buf.blob_at(item.blob);
                                    if (path_blob.empty()) continue;
                                    const auto points = std::span<const Point>(
                                        reinterpret_cast<const Point*>(path_blob.data()),
                                        static_cast<std::size_t>(item.count));
                                    Rect bounds{};
                                    if (ui::gfx::path::compute_bounds(points.data(), item.count, bounds)) {
                                        mark_bounds(bounds);
                                    }
                                }
                            }
                        }
                        offset += stride;
                        continue;
                    }
                    if (cmd.type == CmdType::FocusRingBatch) {
                        const int count = cmd.p3;
                        if (count > 0) {
                            const auto blob = buf.blob_at(cmd.blob);
                            if (blob.size() >= static_cast<std::size_t>(count) * sizeof(RectBatchItem)) {
                                const auto items = std::span<const RectBatchItem>(
                                    reinterpret_cast<const RectBatchItem*>(blob.data()), count);
                                for (std::size_t i = 0; i < items.size(); ++i) {
                                    const auto& item = items[i];
                                    mark_bounds(item.rect);
                                }
                            }
                        }
                        offset += stride;
                        continue;
                    }
                    Rect bounds = cmd.rect;
                    if (cmd.type == CmdType::DrawLine) {
                        bounds = line_bounds(cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h);
                    }
                    mark_bounds(bounds);
                    offset += stride;
                }
            }

            backend.begin_frame();
            for (int ty = 0; ty < tiles_y; ++ty) {
                const int y = ty * tile_h;
                for (int tx = 0; tx < tiles_x; ++tx) {
                    const int x = tx * tile_w;
                    const int w = ((x + tile_w) <= screen_w) ? tile_w : (screen_w - x);
                    const int h = ((y + tile_h) <= screen_h) ? tile_h : (screen_h - y);
                    if (w <= 0 || h <= 0) continue;
                    Rect tile_rect{x, y, w, h};
                    stats.tiles_total++;
                    if (use_hit_cache) {
                        const std::size_t idx = static_cast<std::size_t>(ty) * static_cast<std::size_t>(tiles_x)
                            + static_cast<std::size_t>(tx);
                        if (idx >= tile_hits.size() || tile_hits[idx] == 0) continue;
                    } else {
                        if (!buf.any_draw_hits(tile_rect)) continue;
                    }

                    if (config.clear_tile) {
                        tile_canvas.clear(config.clear_color);
                    }
                    tile_canvas.set_origin(-tile_rect.x, -tile_rect.y);
                    const auto exec_stats = execute(tile_canvas, buf, &tile_rect);
                    stats.dispatch_groups += exec_stats.dispatch_groups;
                    stats.batch_flushes += exec_stats.batch_flushes;
                    stats.failed_cmds += exec_stats.failed_cmds;
                    stats.clip_push_overflow += exec_stats.clip_push_overflow;
                    stats.clip_pop_underflow += exec_stats.clip_pop_underflow;
                    stats.clip_invalid += exec_stats.clip_invalid;
                    stats.fail_text += exec_stats.fail_text;
                    stats.fail_image += exec_stats.fail_image;
                    stats.fail_blob += exec_stats.fail_blob;
                    stats.fail_path += exec_stats.fail_path;
                    stats.fail_clip += exec_stats.fail_clip;
                    stats.fail_other += exec_stats.fail_other;
                    stats.group_rect += exec_stats.group_rect;
                    stats.group_text += exec_stats.group_text;
                    stats.group_image += exec_stats.group_image;
                    stats.group_line += exec_stats.group_line;
                    stats.group_path += exec_stats.group_path;
                    stats.group_other += exec_stats.group_other;
                    stats.cmd_rect += exec_stats.cmd_rect;
                    stats.cmd_text += exec_stats.cmd_text;
                    stats.cmd_image += exec_stats.cmd_image;
                    stats.cmd_line += exec_stats.cmd_line;
                    stats.cmd_path += exec_stats.cmd_path;
                    stats.cmd_other += exec_stats.cmd_other;
                    tile_canvas.clear_origin();

                    const std::size_t row_bytes = static_cast<std::size_t>(w)
                        * bytes_per_pixel(tile_buffer.format);
                    for (int row = 0; row < h; ++row) {
                        const std::byte* src = tile_buffer.data + static_cast<std::size_t>(row) * stride;
                        backend.blit_span(x, y + row, src, row_bytes);
                    }
                    backend.mark_dirty(x, y, w, h);
                    stats.tiles_drawn++;
                    stats.tile_flush_count++;
                }
            }
            backend.end_frame();
            return stats;
        }

#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
    private:
        DrawCmdDetailStats last_detail_stats_{};
#endif
    };
}
