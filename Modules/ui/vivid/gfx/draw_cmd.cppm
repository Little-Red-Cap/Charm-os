module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

export module charm.gfx.draw_cmd;

export import charm.core.geometry;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.framebuffer;
export import charm.gfx.image;
export import charm.gfx.render;
export import charm.font;
export import charm.widgets.text;
export import ui.render_backend;

import util.core;

export namespace ui::draw_cmd {
    constexpr std::size_t bytes_per_pixel(PixelFormat fmt) noexcept {
        switch (fmt) {
            case PixelFormat::RGB565: return PixelTraits<PixelFormat::RGB565>::bytes_per_pixel;
            case PixelFormat::RGB888: return PixelTraits<PixelFormat::RGB888>::bytes_per_pixel;
            case PixelFormat::ARGB8888: return PixelTraits<PixelFormat::ARGB8888>::bytes_per_pixel;
            default: return PixelTraits<PixelFormat::ARGB8888>::bytes_per_pixel;
        }
    }

    enum class CmdType : std::uint8_t {
        PushClip,
        PopClip,
        DrawLine,
        DrawPath,
        FillRect,
        StrokeRect,
        FillRoundRect,
        StrokeRoundRect,
        FillCircle,
        StrokeCircle,
        DrawImage,
        DrawImageNineSlice,
        DrawTextBox,
        FocusRing,
    };

    struct TextSpan {
        std::uint32_t offset{0};
        std::uint16_t length{0};
    };

    struct DrawCmd {
        CmdType type{CmdType::FillRect};
        Rect rect{};
        rgba color{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t p2{0};
        std::int16_t p3{0};
        TextSpan text{};
        const Point* path{nullptr};
        const ImageView* image{nullptr};
        const Font* font{nullptr};
        TextAlignH align_h{TextAlignH::Left};
        TextAlignV align_v{TextAlignV::Top};
        TextWrap wrap{TextWrap::None};
        TextEllipsis ellipsis{TextEllipsis::None};
    };

    static_assert(std::is_trivially_copyable_v<DrawCmd>);

    struct DrawCmdStats {
        std::size_t cmd_count{0};
        std::size_t cmd_capacity{0};
        std::size_t cmd_bytes{0};
        std::size_t text_used{0};
        std::size_t text_capacity{0};
        bool cmd_overflowed{false};
        bool text_overflowed{false};
    };

    struct DrawCmdExecStats {
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::size_t clip_pushes{0};
        std::size_t clip_pops{0};
        bool overflowed{false};
    };

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
    };

    template <std::size_t MaxCmds, std::size_t TextBytes>
    class DrawCmdBuffer {
    public:
        static constexpr std::size_t kMaxCommands = MaxCmds;
        static constexpr std::size_t kTextCapacity = TextBytes;

        void clear() noexcept {
            count_ = 0;
            cmd_overflowed_ = false;
            text_overflowed_ = false;
            text_used_ = 1;
            text_[0] = '\0';
        }

        [[nodiscard]] std::size_t size() const noexcept { return count_; }
        [[nodiscard]] const DrawCmd* data() const noexcept { return cmds_.data(); }
        [[nodiscard]] bool cmd_overflowed() const noexcept { return cmd_overflowed_; }
        [[nodiscard]] bool text_overflowed() const noexcept { return text_overflowed_; }
        [[nodiscard]] bool overflowed() const noexcept { return cmd_overflowed_ || text_overflowed_; }
        [[nodiscard]] const char* text_at(std::uint32_t offset) const noexcept {
            if (offset >= text_used_) return text_.data();
            return text_.data() + offset;
        }

        DrawCmdStats stats() const noexcept {
            return DrawCmdStats{
                count_,
                kMaxCommands,
                count_ * sizeof(DrawCmd),
                text_used_,
                kTextCapacity,
                cmd_overflowed_,
                text_overflowed_
            };
        }

        bool push_clip(const Rect& rect) noexcept { return push_cmd(make_cmd(CmdType::PushClip, rect)); }
        bool pop_clip() noexcept { return push_cmd(make_cmd(CmdType::PopClip, Rect{})); }

        bool draw_line(int x0, int y0, int x1, int y1, const rgba& color) noexcept {
            auto cmd = make_cmd(CmdType::DrawLine, Rect{x0, y0, x1, y1});
            cmd.color = color;
            return push_cmd(cmd);
        }

        bool draw_path(const Point* points, int count, bool closed, const rgba& color) noexcept {
            if (!points || count < 2) return false;
            Rect bounds{points[0].x, points[0].y, 0, 0};
            int min_x = points[0].x;
            int max_x = points[0].x;
            int min_y = points[0].y;
            int max_y = points[0].y;
            for (int i = 1; i < count; ++i) {
                const int px = points[i].x;
                const int py = points[i].y;
                if (px < min_x) min_x = px;
                if (px > max_x) max_x = px;
                if (py < min_y) min_y = py;
                if (py > max_y) max_y = py;
            }
            bounds.x = min_x;
            bounds.y = min_y;
            bounds.w = max_x - min_x + 1;
            bounds.h = max_y - min_y + 1;
            auto cmd = make_cmd(CmdType::DrawPath, bounds);
            cmd.color = color;
            cmd.path = points;
            cmd.p0 = static_cast<std::int16_t>(count);
            cmd.p1 = closed ? 1 : 0;
            return push_cmd(cmd);
        }

        bool fill_rect(const Rect& rect, const rgba& color) noexcept {
            auto cmd = make_cmd(CmdType::FillRect, rect);
            cmd.color = color;
            return push_cmd(cmd);
        }

        bool stroke_rect(const Rect& rect, const rgba& color) noexcept {
            auto cmd = make_cmd(CmdType::StrokeRect, rect);
            cmd.color = color;
            return push_cmd(cmd);
        }

        bool fill_round_rect(const Rect& rect, int radius, const rgba& color) noexcept {
            auto cmd = make_cmd(CmdType::FillRoundRect, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(radius);
            return push_cmd(cmd);
        }

        bool stroke_round_rect(const Rect& rect, int radius, const rgba& color) noexcept {
            auto cmd = make_cmd(CmdType::StrokeRoundRect, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(radius);
            return push_cmd(cmd);
        }

        bool fill_circle(int cx, int cy, int radius, const rgba& color) noexcept {
            Rect rect{cx - radius, cy - radius, radius * 2, radius * 2};
            auto cmd = make_cmd(CmdType::FillCircle, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(radius);
            return push_cmd(cmd);
        }

        bool stroke_circle(int cx, int cy, int radius, const rgba& color) noexcept {
            Rect rect{cx - radius, cy - radius, radius * 2, radius * 2};
            auto cmd = make_cmd(CmdType::StrokeCircle, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(radius);
            return push_cmd(cmd);
        }

        bool draw_image(const Rect& rect, const ImageView& image) noexcept {
            auto cmd = make_cmd(CmdType::DrawImage, rect);
            cmd.image = &image;
            return push_cmd(cmd);
        }

        bool draw_icon(const Rect& rect, const ImageView& image) noexcept {
            return draw_image(rect, image);
        }

        bool draw_image_nine_slice(const Rect& rect,
                                   const ImageView& image,
                                   int left,
                                   int top,
                                   int right,
                                   int bottom) noexcept {
            auto cmd = make_cmd(CmdType::DrawImageNineSlice, rect);
            cmd.image = &image;
            cmd.p0 = static_cast<std::int16_t>(left);
            cmd.p1 = static_cast<std::int16_t>(top);
            cmd.p2 = static_cast<std::int16_t>(right);
            cmd.p3 = static_cast<std::int16_t>(bottom);
            return push_cmd(cmd);
        }

        bool draw_text_box(const Rect& rect,
                           const char* text,
                           const rgba& color,
                           const Font& font,
                           TextAlignH align_h,
                           TextAlignV align_v,
                           TextWrap wrap,
                           TextEllipsis ellipsis) noexcept {
            const TextSpan span = add_text(text);
            auto cmd = make_cmd(CmdType::DrawTextBox, rect);
            cmd.color = color;
            cmd.text = span;
            cmd.font = &font;
            cmd.align_h = align_h;
            cmd.align_v = align_v;
            cmd.wrap = wrap;
            cmd.ellipsis = ellipsis;
            return push_cmd(cmd);
        }

        bool focus_ring(const Rect& rect,
                        const rgba& color,
                        int corner_radius,
                        int inset,
                        int radius_override) noexcept {
            auto cmd = make_cmd(CmdType::FocusRing, rect);
            cmd.color = color;
            cmd.p0 = static_cast<std::int16_t>(corner_radius);
            cmd.p1 = static_cast<std::int16_t>(inset);
            cmd.p2 = static_cast<std::int16_t>(radius_override);
            return push_cmd(cmd);
        }

        bool any_draw_hits(const Rect& rect) const noexcept {
            Rect out{};
            for (std::size_t i = 0; i < count_; ++i) {
                const auto& cmd = cmds_[i];
                if (cmd.type == CmdType::PushClip || cmd.type == CmdType::PopClip) {
                    continue;
                }
                if (cmd.type == CmdType::DrawLine) {
                    const int x0 = cmd.rect.x;
                    const int y0 = cmd.rect.y;
                    const int x1 = cmd.rect.w;
                    const int y1 = cmd.rect.h;
                    Rect bounds{
                        (x0 < x1) ? x0 : x1,
                        (y0 < y1) ? y0 : y1,
                        (x0 < x1) ? (x1 - x0 + 1) : (x0 - x1 + 1),
                        (y0 < y1) ? (y1 - y0 + 1) : (y0 - y1 + 1)
                    };
                    if (rect_intersect(bounds, rect, out)) return true;
                    continue;
                }
                if (!rect_valid(cmd.rect)) continue;
                if (rect_intersect(cmd.rect, rect, out)) return true;
            }
            return false;
        }

    private:
        DrawCmd make_cmd(CmdType type, const Rect& rect) noexcept {
            DrawCmd cmd{};
            cmd.type = type;
            cmd.rect = rect;
            return cmd;
        }

        TextSpan add_text(const char* text) noexcept {
            if (!text) text = "";
            const std::size_t len = std::strlen(text);
            if (len + 1 > (kTextCapacity - text_used_)) {
                text_overflowed_ = true;
                return TextSpan{0, 0};
            }
            const std::uint32_t offset = static_cast<std::uint32_t>(text_used_);
            std::memcpy(text_.data() + text_used_, text, len);
            text_[text_used_ + len] = '\0';
            text_used_ += len + 1;
            return TextSpan{offset, static_cast<std::uint16_t>(len)};
        }

        bool push_cmd(const DrawCmd& cmd) noexcept {
            if (count_ >= kMaxCommands) {
                cmd_overflowed_ = true;
                return false;
            }
            cmds_[count_++] = cmd;
            return true;
        }

        std::array<DrawCmd, kMaxCommands> cmds_{};
        std::array<char, kTextCapacity> text_{};
        std::size_t count_{0};
        std::size_t text_used_{1};
        bool cmd_overflowed_{false};
        bool text_overflowed_{false};
    };

    constexpr std::size_t kDefaultCmdCapacity = 1024;
    constexpr std::size_t kDefaultTextCapacity = 4096;
    using DefaultDrawCmdBuffer = DrawCmdBuffer<kDefaultCmdCapacity, kDefaultTextCapacity>;

    class DrawCmdExecutor {
    public:
        template <class Buffer>
        DrawCmdExecStats execute(CanvasBase& canvas,
                                 const Buffer& buf,
                                 const Rect* initial_clip = nullptr) noexcept {
            DrawCmdExecStats stats{};
            stats.cmd_count = buf.size();
            stats.cmd_bytes = buf.size() * sizeof(DrawCmd);
            stats.overflowed = buf.overflowed();

            std::array<CanvasBase::ClipState, 64> clip_stack{};
            std::size_t sp = 0;
            const auto base_clip = canvas.save_clip();
            if (initial_clip) {
                canvas.set_clip(*initial_clip);
            }

            const DrawCmd* cmds = buf.data();
            const std::size_t count = buf.size();
            for (std::size_t i = 0; i < count; ++i) {
                const auto& cmd = cmds[i];
                switch (cmd.type) {
                case CmdType::PushClip:
                    if (sp < clip_stack.size()) {
                        clip_stack[sp++] = canvas.save_clip();
                        canvas.set_clip(cmd.rect);
                        stats.clip_pushes++;
                    }
                    break;
                case CmdType::PopClip:
                    if (sp > 0) {
                        canvas.restore_clip(clip_stack[--sp]);
                        stats.clip_pops++;
                    }
                    break;
                case CmdType::DrawLine:
                    ui::render::draw_line(canvas,
                                          cmd.rect.x,
                                          cmd.rect.y,
                                          cmd.rect.w,
                                          cmd.rect.h,
                                          cmd.color);
                    break;
                case CmdType::DrawPath: {
                    const int count = cmd.p0;
                    if (!cmd.path || count < 2) break;
                    const bool closed = (cmd.p1 != 0);
                    for (int i = 1; i < count; ++i) {
                        ui::render::draw_line(canvas,
                                              cmd.path[i - 1].x,
                                              cmd.path[i - 1].y,
                                              cmd.path[i].x,
                                              cmd.path[i].y,
                                              cmd.color);
                    }
                    if (closed) {
                        ui::render::draw_line(canvas,
                                              cmd.path[count - 1].x,
                                              cmd.path[count - 1].y,
                                              cmd.path[0].x,
                                              cmd.path[0].y,
                                              cmd.color);
                    }
                    break;
                }
                case CmdType::FillRect:
                    ui::render::draw_rect(canvas, cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h, cmd.color, true);
                    break;
                case CmdType::StrokeRect:
                    ui::render::draw_rect(canvas, cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h, cmd.color, false);
                    break;
                case CmdType::FillRoundRect:
                    ui::render::draw_round_rect(canvas, cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h, cmd.p0, cmd.color, true);
                    break;
                case CmdType::StrokeRoundRect:
                    ui::render::draw_round_rect(canvas, cmd.rect.x, cmd.rect.y, cmd.rect.w, cmd.rect.h, cmd.p0, cmd.color, false);
                    break;
                case CmdType::FillCircle: {
                    const int radius = cmd.p0;
                    const int cx = cmd.rect.x + cmd.rect.w / 2;
                    const int cy = cmd.rect.y + cmd.rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cmd.color, true);
                    break;
                }
                case CmdType::StrokeCircle: {
                    const int radius = cmd.p0;
                    const int cx = cmd.rect.x + cmd.rect.w / 2;
                    const int cy = cmd.rect.y + cmd.rect.h / 2;
                    ui::render::draw_circle(canvas, cx, cy, radius, cmd.color, false);
                    break;
                }
                case CmdType::DrawImage: {
                    if (!cmd.image || !(*cmd.image)) break;
                    if (cmd.rect.w > 0 && cmd.rect.h > 0) {
                        ui::render::draw_image_scaled(canvas, cmd.rect.x, cmd.rect.y,
                                                      cmd.rect.w, cmd.rect.h, *cmd.image);
                    } else {
                        ui::render::draw_image(canvas, cmd.rect.x, cmd.rect.y, *cmd.image);
                    }
                    break;
                }
                case CmdType::DrawImageNineSlice: {
                    if (!cmd.image || !(*cmd.image)) break;
                    ui::render::draw_image_nine_slice(canvas,
                                                      cmd.rect.x, cmd.rect.y,
                                                      cmd.rect.w, cmd.rect.h,
                                                      *cmd.image,
                                                      cmd.p0, cmd.p1, cmd.p2, cmd.p3);
                    break;
                }
                case CmdType::DrawTextBox: {
                    const char* text = buf.text_at(cmd.text.offset);
                    if (!cmd.font) break;
                    draw_text_box(canvas, cmd.rect, text, cmd.color, *cmd.font,
                                  cmd.align_h, cmd.align_v, cmd.wrap, cmd.ellipsis);
                    break;
                }
                case CmdType::FocusRing:
                    ui::render::draw_focus_ring(canvas, cmd.rect, cmd.color, cmd.p0, true, cmd.p1, cmd.p2);
                    break;
                }
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
            stats.cmd_bytes = buf.size() * sizeof(DrawCmd);
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
                const DrawCmd* cmds = buf.data();
                const std::size_t count = buf.size();
                for (std::size_t i = 0; i < count; ++i) {
                    const auto& cmd = cmds[i];
                    if (cmd.type == CmdType::PushClip || cmd.type == CmdType::PopClip) {
                        continue;
                    }
                    Rect bounds = cmd.rect;
                    if (cmd.type == CmdType::DrawLine) {
                        const int x0 = cmd.rect.x;
                        const int y0 = cmd.rect.y;
                        const int x1 = cmd.rect.w;
                        const int y1 = cmd.rect.h;
                        bounds = Rect{
                            (x0 < x1) ? x0 : x1,
                            (y0 < y1) ? y0 : y1,
                            (x0 < x1) ? (x1 - x0 + 1) : (x0 - x1 + 1),
                            (y0 < y1) ? (y1 - y0 + 1) : (y0 - y1 + 1)
                        };
                    }
                    if (!rect_valid(bounds)) continue;
                    if (!rect_intersect(bounds, screen_rect, clipped)) continue;

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
                    (void)execute(tile_canvas, buf, &tile_rect);
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
    };
}
