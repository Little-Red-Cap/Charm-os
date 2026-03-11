module;

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

export module charm.gfx.draw_cmd;

export import charm.core.geometry;
export import charm.gfx.canvas;
export import charm.gfx.color;
export import charm.gfx.framebuffer;
export import charm.gfx.image;
export import charm.gfx.path;
export import charm.gfx.render_core;
export import charm.font;
export import charm.font.typography;
export import charm.gfx.text_box;
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

    struct BlobRef {
        std::uint32_t offset{0};
        std::uint32_t length{0};
    };

    using ImageId = ui::gfx::ImageId;
    using ImageRegisterReason = ui::gfx::ImageRegisterReason;
    using ImageRegistryStats = ui::gfx::ImageRegistryStats;
    using ImageRegistryEntry = ui::gfx::ImageRegistryEntry;
    using ui::gfx::invalid_image_id;
    using ui::gfx::image_id_valid;
    using ui::gfx::image_register_reason_name;
    using ui::gfx::register_image;
    using ui::gfx::register_image_key;
    using ui::gfx::register_image_dedup;
    using ui::gfx::unregister_image;
    using ui::gfx::resolve_image;
    using ui::gfx::clear_image_registry;
    using ui::gfx::register_image_with_id;
    using ui::gfx::image_registry_stats;
    using ui::gfx::set_image_registry_locked;
    using ui::gfx::image_registry_locked;
    using ui::gfx::image_registry_capacity;
    using ui::gfx::image_registry_entry;
    using ui::gfx::image_registry_register_after_lock;
    using ui::gfx::image_registry_first_after_lock_tag;
    using ui::gfx::image_registry_first_after_lock_reason;

    struct DrawCmd {
        CmdType type{CmdType::FillRect};
        Rect rect{};
        rgba color{};
        std::int16_t p0{0};
        std::int16_t p1{0};
        std::int16_t p2{0};
        std::int16_t p3{0};
        TextSpan text{};
        BlobRef blob{};
        ImageId image{};
        FontId font{FontId::Normal};
        TextAlignH align_h{TextAlignH::Left};
        TextAlignV align_v{TextAlignV::Top};
        TextWrap wrap{TextWrap::None};
        TextEllipsis ellipsis{TextEllipsis::None};
    };

    constexpr std::uint32_t kDrawCmdBinaryVersion = 1;

    constexpr std::uint32_t draw_cmd_binary_size() noexcept {
        return static_cast<std::uint32_t>(sizeof(DrawCmd));
    }

    static_assert(sizeof(ImageId) == 4);
    static_assert(std::is_trivially_copyable_v<DrawCmd>);

    struct DrawCmdStats {
        std::size_t cmd_count{0};
        std::size_t cmd_capacity{0};
        std::size_t cmd_bytes{0};
        std::size_t text_used{0};
        std::size_t text_capacity{0};
        std::size_t blob_used{0};
        std::size_t blob_capacity{0};
        bool cmd_overflowed{false};
        bool text_overflowed{false};
        bool blob_overflowed{false};
    };

    struct DrawCmdExecStats {
        std::size_t cmd_count{0};
        std::size_t cmd_bytes{0};
        std::size_t clip_pushes{0};
        std::size_t clip_pops{0};
        std::size_t failed_cmds{0};
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

    constexpr std::size_t align_up(std::size_t value, std::size_t alignment) noexcept {
        if (alignment == 0) return value;
        const std::size_t mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    template <std::size_t Capacity>
    class BlobArena {
    public:
        void reset() noexcept {
            used_ = 0;
            overflowed_ = false;
        }

        [[nodiscard]] std::size_t used() const noexcept { return used_; }
        [[nodiscard]] std::size_t capacity() const noexcept { return Capacity; }
        [[nodiscard]] bool overflowed() const noexcept { return overflowed_; }
        [[nodiscard]] const std::byte* data() const noexcept { return buffer_.data(); }

        BlobRef add_bytes(const void* data, std::size_t len, std::size_t alignment) noexcept {
            if (!data || len == 0) return BlobRef{};
            const std::size_t offset = align_up(used_, alignment);
            const std::size_t next = offset + len;
            if (next > Capacity) {
                overflowed_ = true;
                return BlobRef{};
            }
            std::memcpy(buffer_.data() + offset, data, len);
            used_ = next;
            return BlobRef{static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(len)};
        }

        std::span<const std::byte> bytes(BlobRef ref) const noexcept {
            if (ref.length == 0) return {};
            const std::size_t end = static_cast<std::size_t>(ref.offset) + ref.length;
            if (end > used_) return {};
            return std::span<const std::byte>(buffer_.data() + ref.offset, ref.length);
        }

        bool load(const std::byte* data, std::size_t len) noexcept {
            reset();
            if (!data || len == 0) return true;
            if (len > Capacity) {
                overflowed_ = true;
                return false;
            }
            std::memcpy(buffer_.data(), data, len);
            used_ = len;
            return true;
        }

    private:
        std::array<std::byte, Capacity> buffer_{};
        std::size_t used_{0};
        bool overflowed_{false};
    };

    template <std::size_t MaxCmds, std::size_t TextBytes, std::size_t BlobBytes>
    class DrawCmdBuffer {
    public:
        static constexpr std::size_t kMaxCommands = MaxCmds;
        static constexpr std::size_t kTextCapacity = TextBytes;
        static constexpr std::size_t kBlobCapacity = BlobBytes;

        void clear() noexcept {
            count_ = 0;
            cmd_overflowed_ = false;
            text_overflowed_ = false;
            text_used_ = 1;
            text_[0] = '\0';
            blob_.reset();
        }

        [[nodiscard]] std::size_t size() const noexcept { return count_; }
        [[nodiscard]] const DrawCmd* data() const noexcept { return cmds_.data(); }
        [[nodiscard]] const char* text_data() const noexcept { return text_.data(); }
        [[nodiscard]] std::size_t text_used() const noexcept { return text_used_; }
        [[nodiscard]] const std::byte* blob_data() const noexcept { return blob_.data(); }
        [[nodiscard]] std::size_t blob_used() const noexcept { return blob_.used(); }
        [[nodiscard]] bool cmd_overflowed() const noexcept { return cmd_overflowed_; }
        [[nodiscard]] bool text_overflowed() const noexcept { return text_overflowed_; }
        [[nodiscard]] bool blob_overflowed() const noexcept { return blob_.overflowed(); }
        [[nodiscard]] bool overflowed() const noexcept {
            return cmd_overflowed_ || text_overflowed_ || blob_.overflowed();
        }
        [[nodiscard]] const char* text_at(std::uint32_t offset) const noexcept {
            if (offset >= text_used_) return text_.data();
            return text_.data() + offset;
        }
        [[nodiscard]] std::span<const std::byte> blob_at(BlobRef ref) const noexcept {
            return blob_.bytes(ref);
        }
        [[nodiscard]] bool text_span_valid(TextSpan span) const noexcept {
            if (span.length == 0) return true;
            const std::size_t end = static_cast<std::size_t>(span.offset) + span.length;
            return end <= text_used_;
        }

        bool load(const DrawCmd* cmds,
                  std::size_t cmd_count,
                  const char* text,
                  std::size_t text_bytes,
                  const std::byte* blob,
                  std::size_t blob_bytes) noexcept {
            clear();
            if (!cmds && cmd_count != 0) {
                cmd_overflowed_ = true;
                return false;
            }
            if (cmd_count > kMaxCommands) {
                cmd_overflowed_ = true;
                cmd_count = kMaxCommands;
            }
            if (cmd_count > 0) {
                std::memcpy(cmds_.data(), cmds, cmd_count * sizeof(DrawCmd));
            }
            count_ = cmd_count;
            if (!text || text_bytes == 0) {
                text_[0] = '\0';
                text_used_ = 1;
            } else {
                if (text_bytes > kTextCapacity) {
                    text_overflowed_ = true;
                    text_bytes = kTextCapacity;
                }
                std::memcpy(text_.data(), text, text_bytes);
                text_used_ = text_bytes;
            }
            if (blob && blob_bytes > 0) {
                if (!blob_.load(blob, blob_bytes)) {
                    return false;
                }
            }
            return !overflowed();
        }

        DrawCmdStats stats() const noexcept {
            return DrawCmdStats{
                count_,
                kMaxCommands,
                count_ * sizeof(DrawCmd),
                text_used_,
                kTextCapacity,
                blob_.used(),
                kBlobCapacity,
                cmd_overflowed_,
                text_overflowed_,
                blob_.overflowed()
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
            Rect bounds{};
            if (!ui::gfx::path::compute_bounds(points, count, bounds)) return false;
            auto cmd = make_cmd(CmdType::DrawPath, bounds);
            cmd.color = color;
            const BlobRef blob = blob_.add_bytes(points,
                                                 ui::gfx::path::point_bytes(count),
                                                 alignof(Point));
            if (blob.length == 0) return false;
            cmd.blob = blob;
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

        bool draw_image(const Rect& rect, ImageId image) noexcept {
            if (!image_id_valid(image)) return false;
            auto cmd = make_cmd(CmdType::DrawImage, rect);
            cmd.image = image;
            return push_cmd(cmd);
        }

        bool draw_icon(const Rect& rect, ImageId image) noexcept {
            return draw_image(rect, image);
        }

        bool draw_image_nine_slice(const Rect& rect,
                                   ImageId image,
                                   int left,
                                   int top,
                                   int right,
                                   int bottom) noexcept {
            if (!image_id_valid(image)) return false;
            auto cmd = make_cmd(CmdType::DrawImageNineSlice, rect);
            cmd.image = image;
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
            cmd.font = font_id_from_ptr(&font);
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
        BlobArena<kBlobCapacity> blob_{};
        std::size_t count_{0};
        std::size_t text_used_{1};
        bool cmd_overflowed_{false};
        bool text_overflowed_{false};
    };

    constexpr std::size_t kDefaultCmdCapacity = 1024;
    constexpr std::size_t kDefaultTextCapacity = 4096;
    constexpr std::size_t kDefaultBlobCapacity = 2048;
    using DefaultDrawCmdBuffer = DrawCmdBuffer<kDefaultCmdCapacity, kDefaultTextCapacity, kDefaultBlobCapacity>;

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
                    if (count < 2) {
                        stats.failed_cmds++;
                        break;
                    }
                    const auto blob = buf.blob_at(cmd.blob);
                    const auto points = ui::gfx::path::decode_points(blob, count);
                    if (points.empty()) {
                        stats.failed_cmds++;
                        break;
                    }
                    const bool closed = (cmd.p1 != 0);
                    ui::gfx::path::stroke_path(canvas, points, closed, cmd.color);
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
                    const auto* image = resolve_image(cmd.image);
                    if (!image || !(*image)) {
                        stats.failed_cmds++;
                        break;
                    }
                    if (cmd.rect.w > 0 && cmd.rect.h > 0) {
                        ui::render::draw_image_scaled(canvas, cmd.rect.x, cmd.rect.y,
                                                      cmd.rect.w, cmd.rect.h, *image);
                    } else {
                        ui::render::draw_image(canvas, cmd.rect.x, cmd.rect.y, *image);
                    }
                    break;
                }
                case CmdType::DrawImageNineSlice: {
                    const auto* image = resolve_image(cmd.image);
                    if (!image || !(*image)) {
                        stats.failed_cmds++;
                        break;
                    }
                    ui::render::draw_image_nine_slice(canvas,
                                                      cmd.rect.x, cmd.rect.y,
                                                      cmd.rect.w, cmd.rect.h,
                                                      *image,
                                                      cmd.p0, cmd.p1, cmd.p2, cmd.p3);
                    break;
                }
                case CmdType::DrawTextBox: {
                    if (!buf.text_span_valid(cmd.text)) {
                        stats.failed_cmds++;
                        break;
                    }
                    const char* text = buf.text_at(cmd.text.offset);
                    const Font& font = get_font(cmd.font);
                    draw_text_box(canvas, cmd.rect, text, cmd.color, font,
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
