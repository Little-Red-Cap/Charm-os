module;
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
export module charm.gfx.canvas;

export import charm.gfx.framebuffer;
export import charm.core.config;
export import charm.core.geometry;
export import ui.render_backend;
import charm.gfx.pixel_ops;
import service_dirty_rects;
import util.core;

constexpr Rect rect_union(const Rect& a, const Rect& b) noexcept {
        const Rect ra = rect_normalized(a);
        const Rect rb = rect_normalized(b);
        const int left = (ra.x < rb.x) ? ra.x : rb.x;
        const int top = (ra.y < rb.y) ? ra.y : rb.y;
        const int right = ((ra.x + ra.w) > (rb.x + rb.w)) ? (ra.x + ra.w) : (rb.x + rb.w);
        const int bottom = ((ra.y + ra.h) > (rb.y + rb.h)) ? (ra.y + ra.h) : (rb.y + rb.h);
        return Rect{left, top, right - left, bottom - top};
}

constexpr bool rect_overlap_or_touch(const Rect& a, const Rect& b) noexcept {
        const Rect ra = rect_normalized(a);
        const Rect rb = rect_normalized(b);
        const int ax2 = ra.x + ra.w;
        const int ay2 = ra.y + ra.h;
        const int bx2 = rb.x + rb.w;
        const int by2 = rb.y + rb.h;
        return !(ax2 < rb.x - 1 || bx2 < ra.x - 1 || ay2 < rb.y - 1 || by2 < ra.y - 1);
}

constinit std::uint64_t g_alpha_blend_count = 0;
export inline constexpr std::size_t alpha_blend_counter_resident_bytes = sizeof(std::uint64_t);
static_assert(sizeof(g_alpha_blend_count) == alpha_blend_counter_resident_bytes);

export inline void reset_alpha_blend_count() noexcept { g_alpha_blend_count = 0; }
export inline std::uint64_t alpha_blend_count() noexcept { return g_alpha_blend_count; }

export
class CanvasBase {
public:
    struct OriginState {
        int x{0};
        int y{0};
    };

    struct ClipState {
        bool enabled{false};
        Rect rect{};
    };

    struct Ops {
        int (*width)(const void*) noexcept;
        int (*height)(const void*) noexcept;
        std::size_t (*stride_bytes)(const void*) noexcept;
        std::size_t (*bytes_per_pixel)(const void*) noexcept;
        std::byte* (*data)(void*) noexcept;
        const std::byte* (*data_const)(const void*) noexcept;
        const std::byte* (*row_ptr)(const void*, int) noexcept;
        void (*clear)(void*, const rgba&) noexcept;
        void (*set_origin)(void*, int, int) noexcept;
        void (*clear_origin)(void*) noexcept;
        OriginState (*save_origin)(const void*) noexcept;
        void (*restore_origin)(void*, const OriginState&) noexcept;
        void (*set_clip)(void*, const Rect&) noexcept;
        void (*clear_clip)(void*) noexcept;
        ClipState (*save_clip)(const void*) noexcept;
        void (*restore_clip)(void*, const ClipState&) noexcept;
        bool (*in_clip)(const void*, int, int) noexcept;
        void (*set_pixel)(void*, int, int, const rgba&) noexcept;
        void (*draw_hline)(void*, int, int, int, const rgba&) noexcept;
        void (*draw_vline)(void*, int, int, int, const rgba&) noexcept;
        rgba (*get_pixel)(const void*, int, int) noexcept;
        void (*blit_span)(void*, int, int, const std::byte*, std::size_t) noexcept;
        void (*begin_frame)(void*) noexcept;
        void (*end_frame)(void*) noexcept;
        void (*mark_dirty)(void*, const Rect&) noexcept;
    };

    CanvasBase() = default;
    CanvasBase(const Ops* ops, void* self) noexcept : ops_(ops), self_(self) {}

    int width() const noexcept { return ops_->width(self_); }
    int height() const noexcept { return ops_->height(self_); }
    std::size_t stride_bytes() const noexcept { return ops_->stride_bytes(self_); }
    std::size_t bytes_per_pixel() const noexcept { return ops_->bytes_per_pixel(self_); }
    std::byte* data() noexcept { return ops_->data(self_); }
    const std::byte* data() const noexcept { return ops_->data_const(self_); }
    const std::byte* row_ptr(int y) const noexcept { return ops_->row_ptr(self_, y); }
    void clear(const rgba& c = {0,0,0,0}) noexcept { ops_->clear(self_, c); }
    void set_origin(int ox, int oy) noexcept { ops_->set_origin(self_, ox, oy); }
    void clear_origin() noexcept { ops_->clear_origin(self_); }
    OriginState save_origin() const noexcept { return ops_->save_origin(self_); }
    void restore_origin(const OriginState& state) noexcept { ops_->restore_origin(self_, state); }
    void set_clip(const Rect& r) noexcept { ops_->set_clip(self_, r); }
    void clear_clip() noexcept { ops_->clear_clip(self_); }
    ClipState save_clip() const noexcept { return ops_->save_clip(self_); }
    void restore_clip(const ClipState& state) noexcept { ops_->restore_clip(self_, state); }
    bool in_clip(int x, int y) const noexcept { return ops_->in_clip(self_, x, y); }
    void set_pixel(int x, int y, const rgba& c) noexcept { ops_->set_pixel(self_, x, y, c); }
    void draw_hline(int x0, int x1, int y, const rgba& c) noexcept { ops_->draw_hline(self_, x0, x1, y, c); }
    void draw_vline(int x, int y0, int y1, const rgba& c) noexcept { ops_->draw_vline(self_, x, y0, y1, c); }
    rgba get_pixel(int x, int y) const noexcept { return ops_->get_pixel(self_, x, y); }
    void blit_span(int x, int y, const std::byte* src, std::size_t bytes) noexcept {
        ops_->blit_span(self_, x, y, src, bytes);
    }
    void begin_frame() noexcept { ops_->begin_frame(self_); }
    void end_frame() noexcept { ops_->end_frame(self_); }
    void mark_dirty(int x, int y, int w, int h) noexcept { mark_dirty(Rect{x, y, w, h}); }
    void mark_dirty(const Rect& r) noexcept { ops_->mark_dirty(self_, r); }

private:
    static const Ops& null_ops() noexcept {
        static const Ops ops{
            +[](const void*) noexcept { return 0; },
            +[](const void*) noexcept { return 0; },
            +[](const void*) noexcept { return std::size_t{0}; },
            +[](const void*) noexcept { return std::size_t{0}; },
            +[](void*) noexcept { return static_cast<std::byte*>(nullptr); },
            +[](const void*) noexcept { return static_cast<const std::byte*>(nullptr); },
            +[](const void*, int) noexcept { return static_cast<const std::byte*>(nullptr); },
            +[](void*, const rgba&) noexcept {},
            +[](void*, int, int) noexcept {},
            +[](void*) noexcept {},
            +[](const void*) noexcept { return OriginState{}; },
            +[](void*, const OriginState&) noexcept {},
            +[](void*, const Rect&) noexcept {},
            +[](void*) noexcept {},
            +[](const void*) noexcept { return ClipState{}; },
            +[](void*, const ClipState&) noexcept {},
            +[](const void*, int, int) noexcept { return false; },
            +[](void*, int, int, const rgba&) noexcept {},
            +[](void*, int, int, int, const rgba&) noexcept {},
            +[](void*, int, int, int, const rgba&) noexcept {},
            +[](const void*, int, int) noexcept { return rgba{}; },
            +[](void*, int, int, const std::byte*, std::size_t) noexcept {},
            +[](void*) noexcept {},
            +[](void*) noexcept {},
            +[](void*, const Rect&) noexcept {}
        };
        return ops;
    }

    const Ops* ops_{&null_ops()};
    void* self_{nullptr};
};

export
template<PixelFormat PF, std::size_t W, std::size_t H>
class Canvas : public CanvasBase {
public:
    using FB = FrameBuffer<PF, W, H>;
    static constexpr util::usize kDirtyCapacity = 16;
    using DirtyList = service::DirtyRectList<Rect, kDirtyCapacity>;

    Canvas(FB& fb) noexcept : CanvasBase(&ops(), this), fb_(fb) {}

    int width() const noexcept { return static_cast<int>(W); }
    int height() const noexcept { return static_cast<int>(H); }
    std::size_t stride_bytes() const noexcept { return FB::stride_bytes; }
    std::size_t bytes_per_pixel() const noexcept { return FB::bytes_per_pixel; }
    std::byte* data() noexcept { return fb_.data(); }
    const std::byte* data() const noexcept { return fb_.data(); }
    const std::byte* row_ptr(int y) const noexcept {
        if (y < 0 || y >= static_cast<int>(H)) return nullptr;
        return fb_.data() + static_cast<std::size_t>(y) * FB::stride_bytes;
    }

    void clear(const rgba& c = {0,0,0,0}) noexcept { fb_.clear(c); }

    void set_origin(int ox, int oy) noexcept {
        origin_x_ = ox;
        origin_y_ = oy;
    }

    void clear_origin() noexcept {
        origin_x_ = 0;
        origin_y_ = 0;
    }

    OriginState save_origin() const noexcept { return {origin_x_, origin_y_}; }
    void restore_origin(const OriginState& state) noexcept {
        origin_x_ = state.x;
        origin_y_ = state.y;
    }

    void set_clip(const Rect& r) noexcept {
        Rect nr = rect_normalized(r);
        if (!rect_valid(nr)) {
            clear_clip();
            return;
        }
        clip_enabled_ = true;
        clip_ = nr;
    }

    void clear_clip() noexcept { clip_enabled_ = false; }

    ClipState save_clip() const noexcept { return ClipState{clip_enabled_, clip_}; }
    void restore_clip(const ClipState& state) noexcept {
        clip_enabled_ = state.enabled;
        clip_ = state.rect;
    }

    bool in_clip(int x, int y) const noexcept {
        if (clip_enabled_) {
            if (x < clip_.x || y < clip_.y || x >= clip_.x + clip_.w || y >= clip_.y + clip_.h) return false;
        }
        const int lx = x + origin_x_;
        const int ly = y + origin_y_;
        return lx >= 0 && ly >= 0 && lx < static_cast<int>(W) && ly < static_cast<int>(H);
    }

    void set_pixel(int x, int y, const rgba& c) noexcept {
        if (!in_clip(x, y)) return;
        const int lx = x + origin_x_;
        const int ly = y + origin_y_;
        if (lx < 0 || ly < 0) return;
        if (lx >= static_cast<int>(W) || ly >= static_cast<int>(H)) return;
        if (c.a == 0) return;
        if (c.a < 255) {
            ++g_alpha_blend_count;
            const rgba bg = fb_.get_pixel(static_cast<std::size_t>(lx), static_cast<std::size_t>(ly));
            const rgba blended = c.blend_over(bg);
            fb_.set_pixel(static_cast<std::size_t>(lx), static_cast<std::size_t>(ly), blended);
            return;
        }
        fb_.set_pixel(static_cast<std::size_t>(lx), static_cast<std::size_t>(ly), c);
    }

    void draw_hline(int x0, int x1, int y, const rgba& c) noexcept {
        int gx0 = x0;
        int gx1 = x1;
        const int gy = y;
        if (gx0 > gx1) std::swap(gx0, gx1);
        const int ly = gy + origin_y_;
        if (ly < 0 || ly >= static_cast<int>(H)) return;
        int lx0 = gx0 + origin_x_;
        int lx1 = gx1 + origin_x_;
        if (lx0 > lx1) std::swap(lx0, lx1);
        if (lx1 <= 0 || lx0 >= static_cast<int>(W)) return;
        if (lx0 < 0) lx0 = 0;
        if (lx1 > static_cast<int>(W)) lx1 = static_cast<int>(W);
        for (int lx = lx0; lx < lx1; ++lx) {
            const int gx = lx - origin_x_;
            set_pixel(gx, gy, c);
        }
    }

    void draw_vline(int x, int y0, int y1, const rgba& c) noexcept {
        const int gx = x;
        int gy0 = y0;
        int gy1 = y1;
        if (gy0 > gy1) std::swap(gy0, gy1);
        const int lx = gx + origin_x_;
        if (lx < 0 || lx >= static_cast<int>(W)) return;
        int ly0 = gy0 + origin_y_;
        int ly1 = gy1 + origin_y_;
        if (ly0 > ly1) std::swap(ly0, ly1);
        if (ly1 <= 0 || ly0 >= static_cast<int>(H)) return;
        if (ly0 < 0) ly0 = 0;
        if (ly1 > static_cast<int>(H)) ly1 = static_cast<int>(H);
        for (int ly = ly0; ly < ly1; ++ly) {
            const int gy = ly - origin_y_;
            set_pixel(gx, gy, c);
        }
    }

    rgba get_pixel(int x, int y) const noexcept {
        const int lx = x + origin_x_;
        const int ly = y + origin_y_;
        if (lx < 0 || ly < 0) return {};
        if (lx >= static_cast<int>(W) || ly >= static_cast<int>(H)) return {};
        return fb_.get_pixel(static_cast<std::size_t>(lx), static_cast<std::size_t>(ly));
    }

    void blit_span(int x, int y, const std::byte* src, std::size_t bytes) noexcept {
        if (!src || bytes == 0) return;
        if (!in_clip(x, y)) return;
        const int lx = x + origin_x_;
        const int ly = y + origin_y_;
        if (lx < 0 || ly < 0) return;
        if (lx >= static_cast<int>(W) || ly >= static_cast<int>(H)) return;
        const std::size_t offset = static_cast<std::size_t>(ly) * FB::stride_bytes
            + static_cast<std::size_t>(lx) * FB::bytes_per_pixel;
        std::memcpy(fb_.data() + offset, src, bytes);
    }

    void begin_frame() noexcept { dirty_.clear(); }
    void end_frame() noexcept {}

    void mark_dirty(const Rect& r) noexcept {
        Rect nr = rect_normalized(r);
        nr.x += origin_x_;
        nr.y += origin_y_;
        Rect clipped{};
        if (!rect_intersect(nr, full_rect(), clipped)) return;
        if (dirty_.full()) return;
        const util::usize count = dirty_.size();
        for (util::usize i = 0; i < count; ++i) {
            auto& cur = dirty_[i];
            if (rect_overlap_or_touch(cur, clipped)) {
                cur = rect_union(cur, clipped);
                return;
            }
        }
        if (!dirty_.add(clipped)) {
            dirty_.set_full(full_rect());
        }
    }

    [[nodiscard]] const DirtyList& dirty_list() const noexcept { return dirty_; }
    [[nodiscard]] bool dirty_full() const noexcept { return dirty_.full(); }

private:
    static const Ops& ops() noexcept {
        static const Ops ops{
            +[](const void* self) noexcept { return static_cast<const Canvas*>(self)->width(); },
            +[](const void* self) noexcept { return static_cast<const Canvas*>(self)->height(); },
            +[](const void* self) noexcept { return static_cast<const Canvas*>(self)->stride_bytes(); },
            +[](const void* self) noexcept { return static_cast<const Canvas*>(self)->bytes_per_pixel(); },
            +[](void* self) noexcept { return static_cast<Canvas*>(self)->data(); },
            +[](const void* self) noexcept { return static_cast<const Canvas*>(self)->data(); },
            +[](const void* self, int y) noexcept { return static_cast<const Canvas*>(self)->row_ptr(y); },
            +[](void* self, const rgba& c) noexcept { static_cast<Canvas*>(self)->clear(c); },
            +[](void* self, int ox, int oy) noexcept { static_cast<Canvas*>(self)->set_origin(ox, oy); },
            +[](void* self) noexcept { static_cast<Canvas*>(self)->clear_origin(); },
            +[](const void* self) noexcept { return static_cast<const Canvas*>(self)->save_origin(); },
            +[](void* self, const OriginState& state) noexcept { static_cast<Canvas*>(self)->restore_origin(state); },
            +[](void* self, const Rect& r) noexcept { static_cast<Canvas*>(self)->set_clip(r); },
            +[](void* self) noexcept { static_cast<Canvas*>(self)->clear_clip(); },
            +[](const void* self) noexcept { return static_cast<const Canvas*>(self)->save_clip(); },
            +[](void* self, const ClipState& state) noexcept { static_cast<Canvas*>(self)->restore_clip(state); },
            +[](const void* self, int x, int y) noexcept { return static_cast<const Canvas*>(self)->in_clip(x, y); },
            +[](void* self, int x, int y, const rgba& c) noexcept { static_cast<Canvas*>(self)->set_pixel(x, y, c); },
            +[](void* self, int x0, int x1, int y, const rgba& c) noexcept { static_cast<Canvas*>(self)->draw_hline(x0, x1, y, c); },
            +[](void* self, int x, int y0, int y1, const rgba& c) noexcept { static_cast<Canvas*>(self)->draw_vline(x, y0, y1, c); },
            +[](const void* self, int x, int y) noexcept { return static_cast<const Canvas*>(self)->get_pixel(x, y); },
            +[](void* self, int x, int y, const std::byte* src, std::size_t bytes) noexcept { static_cast<Canvas*>(self)->blit_span(x, y, src, bytes); },
            +[](void* self) noexcept { static_cast<Canvas*>(self)->begin_frame(); },
            +[](void* self) noexcept { static_cast<Canvas*>(self)->end_frame(); },
            +[](void* self, const Rect& r) noexcept { static_cast<Canvas*>(self)->mark_dirty(r); }
        };
        return ops;
    }

    constexpr Rect full_rect() const noexcept {
        return Rect{0, 0, static_cast<int>(W), static_cast<int>(H)};
    }

    FB& fb_;
    int origin_x_{0};
    int origin_y_{0};
    bool clip_enabled_{false};
    Rect clip_{0, 0, static_cast<int>(W), static_cast<int>(H)};
    DirtyList dirty_{};
};

export
using DefaultCanvas = Canvas<screen_pixel_format,
                             static_cast<std::size_t>(screen_width),
                             static_cast<std::size_t>(screen_height)>;

export
class RuntimeCanvas : public CanvasBase {
public:
    using DirtyList = service::DirtyRectList<Rect, 16>;

    RuntimeCanvas(std::byte* data,
                  int width,
                  int height,
                  PixelFormat format,
                  std::size_t stride_bytes = 0) noexcept
        : CanvasBase(&ops(), this),
          data_(data),
          width_(width),
          height_(height),
          format_(format) {
        stride_bytes_ = (stride_bytes != 0) ? stride_bytes
            : static_cast<std::size_t>(width_ * bytes_per_pixel(format_));
        clip_ = Rect{0, 0, width_, height_};
    }

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    std::size_t stride_bytes() const noexcept { return stride_bytes_; }
    std::size_t bytes_per_pixel() const noexcept { return bytes_per_pixel(format_); }
    std::byte* data() noexcept { return data_; }
    const std::byte* data() const noexcept { return data_; }
    const std::byte* row_ptr(int y) const noexcept {
        if (!data_ || y < 0 || y >= height_) return nullptr;
        return data_ + static_cast<std::size_t>(y) * stride_bytes_;
    }

    void clear(const rgba& c = {0,0,0,0}) noexcept {
        if (!data_) return;
        // Clear should ignore origin/clip so partial-buffer tiles reset correctly.
        const std::size_t bpp = bytes_per_pixel();
        for (int y = 0; y < height_; ++y) {
            auto* row = data_ + static_cast<std::size_t>(y) * stride_bytes_;
            for (int x = 0; x < width_; ++x) {
                write_pixel(row + static_cast<std::size_t>(x) * bpp, c);
            }
        }
    }

    void set_origin(int ox, int oy) noexcept {
        origin_x_ = ox;
        origin_y_ = oy;
    }

    void clear_origin() noexcept {
        origin_x_ = 0;
        origin_y_ = 0;
    }

    OriginState save_origin() const noexcept { return {origin_x_, origin_y_}; }
    void restore_origin(const OriginState& state) noexcept {
        origin_x_ = state.x;
        origin_y_ = state.y;
    }

    void set_clip(const Rect& r) noexcept {
        Rect nr = rect_normalized(r);
        if (!rect_valid(nr)) {
            clear_clip();
            return;
        }
        clip_enabled_ = true;
        clip_ = nr;
    }

    void clear_clip() noexcept { clip_enabled_ = false; }

    ClipState save_clip() const noexcept { return ClipState{clip_enabled_, clip_}; }
    void restore_clip(const ClipState& state) noexcept {
        clip_enabled_ = state.enabled;
        clip_ = state.rect;
    }

    bool in_clip(int x, int y) const noexcept {
        if (clip_enabled_) {
            if (x < clip_.x || y < clip_.y || x >= clip_.x + clip_.w || y >= clip_.y + clip_.h) return false;
        }
        const int lx = x + origin_x_;
        const int ly = y + origin_y_;
        return lx >= 0 && ly >= 0 && lx < width_ && ly < height_;
    }

    void set_pixel(int x, int y, const rgba& c) noexcept {
        if (!data_) return;
        if (!in_clip(x, y)) return;
        const int lx = x + origin_x_;
        const int ly = y + origin_y_;
        if (lx < 0 || ly < 0 || lx >= width_ || ly >= height_) return;
        if (c.a == 0) return;
        auto* dst = data_ + static_cast<std::size_t>(ly) * stride_bytes_
            + static_cast<std::size_t>(lx) * bytes_per_pixel();
        if (c.a < 255) {
            ++g_alpha_blend_count;
            const rgba bg = read_pixel(dst);
            const rgba blended = c.blend_over(bg);
            write_pixel(dst, blended);
            return;
        }
        write_pixel(dst, c);
    }

    void draw_hline(int x0, int x1, int y, const rgba& c) noexcept {
        int gx0 = x0;
        int gx1 = x1;
        const int gy = y;
        if (gx0 > gx1) std::swap(gx0, gx1);
        const int ly = gy + origin_y_;
        if (ly < 0 || ly >= height_) return;
        int lx0 = gx0 + origin_x_;
        int lx1 = gx1 + origin_x_;
        if (lx0 > lx1) std::swap(lx0, lx1);
        if (lx1 <= 0 || lx0 >= width_) return;
        if (lx0 < 0) lx0 = 0;
        if (lx1 > width_) lx1 = width_;
        for (int lx = lx0; lx < lx1; ++lx) {
            const int gx = lx - origin_x_;
            set_pixel(gx, gy, c);
        }
    }

    void draw_vline(int x, int y0, int y1, const rgba& c) noexcept {
        const int gx = x;
        int gy0 = y0;
        int gy1 = y1;
        if (gy0 > gy1) std::swap(gy0, gy1);
        const int lx = gx + origin_x_;
        if (lx < 0 || lx >= width_) return;
        int ly0 = gy0 + origin_y_;
        int ly1 = gy1 + origin_y_;
        if (ly0 > ly1) std::swap(ly0, ly1);
        if (ly1 <= 0 || ly0 >= height_) return;
        if (ly0 < 0) ly0 = 0;
        if (ly1 > height_) ly1 = height_;
        for (int ly = ly0; ly < ly1; ++ly) {
            const int gy = ly - origin_y_;
            set_pixel(gx, gy, c);
        }
    }

    rgba get_pixel(int x, int y) const noexcept {
        if (!data_) return {};
        const int lx = x + origin_x_;
        const int ly = y + origin_y_;
        if (lx < 0 || ly < 0 || lx >= width_ || ly >= height_) return {};
        const auto* src = data_ + static_cast<std::size_t>(ly) * stride_bytes_
            + static_cast<std::size_t>(lx) * bytes_per_pixel();
        return read_pixel(src);
    }

    void blit_span(int x, int y, const std::byte* src, std::size_t bytes) noexcept {
        if (!data_ || !src || bytes == 0) return;
        if (!in_clip(x, y)) return;
        const int lx = x + origin_x_;
        const int ly = y + origin_y_;
        if (lx < 0 || ly < 0 || lx >= width_ || ly >= height_) return;
        const std::size_t offset = static_cast<std::size_t>(ly) * stride_bytes_
            + static_cast<std::size_t>(lx) * bytes_per_pixel();
        std::memcpy(data_ + offset, src, bytes);
    }

    void begin_frame() noexcept { dirty_.clear(); }
    void end_frame() noexcept {}

    void mark_dirty(const Rect& r) noexcept {
        Rect nr = rect_normalized(r);
        nr.x += origin_x_;
        nr.y += origin_y_;
        Rect clipped{};
        if (!rect_intersect(nr, full_rect(), clipped)) return;
        if (dirty_.full()) return;
        const util::usize count = dirty_.size();
        for (util::usize i = 0; i < count; ++i) {
            auto& cur = dirty_[i];
            if (rect_overlap_or_touch(cur, clipped)) {
                cur = rect_union(cur, clipped);
                return;
            }
        }
        if (!dirty_.add(clipped)) {
            dirty_.set_full(full_rect());
        }
    }

    [[nodiscard]] const DirtyList& dirty_list() const noexcept { return dirty_; }
    [[nodiscard]] bool dirty_full() const noexcept { return dirty_.full(); }

private:
    static const Ops& ops() noexcept {
        static const Ops ops{
            +[](const void* self) noexcept { return static_cast<const RuntimeCanvas*>(self)->width(); },
            +[](const void* self) noexcept { return static_cast<const RuntimeCanvas*>(self)->height(); },
            +[](const void* self) noexcept { return static_cast<const RuntimeCanvas*>(self)->stride_bytes(); },
            +[](const void* self) noexcept { return static_cast<const RuntimeCanvas*>(self)->bytes_per_pixel(); },
            +[](void* self) noexcept { return static_cast<RuntimeCanvas*>(self)->data(); },
            +[](const void* self) noexcept { return static_cast<const RuntimeCanvas*>(self)->data(); },
            +[](const void* self, int y) noexcept { return static_cast<const RuntimeCanvas*>(self)->row_ptr(y); },
            +[](void* self, const rgba& c) noexcept { static_cast<RuntimeCanvas*>(self)->clear(c); },
            +[](void* self, int ox, int oy) noexcept { static_cast<RuntimeCanvas*>(self)->set_origin(ox, oy); },
            +[](void* self) noexcept { static_cast<RuntimeCanvas*>(self)->clear_origin(); },
            +[](const void* self) noexcept { return static_cast<const RuntimeCanvas*>(self)->save_origin(); },
            +[](void* self, const OriginState& state) noexcept { static_cast<RuntimeCanvas*>(self)->restore_origin(state); },
            +[](void* self, const Rect& r) noexcept { static_cast<RuntimeCanvas*>(self)->set_clip(r); },
            +[](void* self) noexcept { static_cast<RuntimeCanvas*>(self)->clear_clip(); },
            +[](const void* self) noexcept { return static_cast<const RuntimeCanvas*>(self)->save_clip(); },
            +[](void* self, const ClipState& state) noexcept { static_cast<RuntimeCanvas*>(self)->restore_clip(state); },
            +[](const void* self, int x, int y) noexcept { return static_cast<const RuntimeCanvas*>(self)->in_clip(x, y); },
            +[](void* self, int x, int y, const rgba& c) noexcept { static_cast<RuntimeCanvas*>(self)->set_pixel(x, y, c); },
            +[](void* self, int x0, int x1, int y, const rgba& c) noexcept { static_cast<RuntimeCanvas*>(self)->draw_hline(x0, x1, y, c); },
            +[](void* self, int x, int y0, int y1, const rgba& c) noexcept { static_cast<RuntimeCanvas*>(self)->draw_vline(x, y0, y1, c); },
            +[](const void* self, int x, int y) noexcept { return static_cast<const RuntimeCanvas*>(self)->get_pixel(x, y); },
            +[](void* self, int x, int y, const std::byte* src, std::size_t bytes) noexcept { static_cast<RuntimeCanvas*>(self)->blit_span(x, y, src, bytes); },
            +[](void* self) noexcept { static_cast<RuntimeCanvas*>(self)->begin_frame(); },
            +[](void* self) noexcept { static_cast<RuntimeCanvas*>(self)->end_frame(); },
            +[](void* self, const Rect& r) noexcept { static_cast<RuntimeCanvas*>(self)->mark_dirty(r); }
        };
        return ops;
    }

    constexpr Rect full_rect() const noexcept { return Rect{0, 0, width_, height_}; }

    static std::size_t bytes_per_pixel(PixelFormat fmt) noexcept {
        switch (fmt) {
        case PixelFormat::RGB565: return PixelTraits<PixelFormat::RGB565>::bytes_per_pixel;
        case PixelFormat::RGB888: return PixelTraits<PixelFormat::RGB888>::bytes_per_pixel;
        case PixelFormat::ARGB8888: return PixelTraits<PixelFormat::ARGB8888>::bytes_per_pixel;
        }
        return 0;
    }

    void write_pixel(std::byte* dst, const rgba& c) noexcept {
        if (!dst) return;
        switch (format_) {
        case PixelFormat::RGB565: {
            auto px = pack_rgb565(rgb{c.r, c.g, c.b});
            std::memcpy(dst, &px, sizeof(px));
            break;
        }
        case PixelFormat::RGB888:
            dst[0] = std::byte{c.r};
            dst[1] = std::byte{c.g};
            dst[2] = std::byte{c.b};
            break;
        case PixelFormat::ARGB8888: {
            auto px = pack_argb8888(c);
            std::memcpy(dst, &px, sizeof(px));
            break;
        }
        }
    }

    rgba read_pixel(const std::byte* src) const noexcept {
        if (!src) return {};
        switch (format_) {
        case PixelFormat::RGB565: {
            uint16_t px{};
            std::memcpy(&px, src, sizeof(px));
            auto rgbv = unpack_rgb565(px);
            return rgba{rgbv.r, rgbv.g, rgbv.b, 255};
        }
        case PixelFormat::RGB888:
            return rgba{
                static_cast<std::uint8_t>(src[0]),
                static_cast<std::uint8_t>(src[1]),
                static_cast<std::uint8_t>(src[2]),
                255
            };
        case PixelFormat::ARGB8888: {
            uint32_t px{};
            std::memcpy(&px, src, sizeof(px));
            return unpack_argb8888(px);
        }
        }
        return {};
    }

    std::byte* data_{nullptr};
    int width_{0};
    int height_{0};
    PixelFormat format_{PixelFormat::RGB888};
    std::size_t stride_bytes_{0};
    int origin_x_{0};
    int origin_y_{0};
    bool clip_enabled_{false};
    Rect clip_{};
    DirtyList dirty_{};
};
