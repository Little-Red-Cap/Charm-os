module;
#include <cstddef>
#include <cstring>
#include <utility>
export module charm.gfx.canvas;

export import charm.gfx.framebuffer;
export import charm.core.config;
export import charm.core.geometry;
import service_dirty_rects;
import util.core;

export
class CanvasBase {
public:
    struct ClipState {
        bool enabled{false};
        Rect rect{};
    };

    virtual ~CanvasBase() = default;
    virtual int width() const noexcept = 0;
    virtual int height() const noexcept = 0;
    virtual std::size_t stride_bytes() const noexcept = 0;
    virtual std::size_t bytes_per_pixel() const noexcept = 0;
    virtual std::byte* data() noexcept = 0;
    virtual const std::byte* data() const noexcept = 0;
    virtual const std::byte* row_ptr(int y) const noexcept = 0;
    virtual void clear(const rgba& c = {0,0,0,0}) noexcept = 0;
    virtual void set_origin(int ox, int oy) noexcept = 0;
    virtual void clear_origin() noexcept = 0;
    virtual void set_clip(const Rect& r) noexcept = 0;
    virtual void clear_clip() noexcept = 0;
    virtual ClipState save_clip() const noexcept = 0;
    virtual void restore_clip(const ClipState& state) noexcept = 0;
    virtual bool in_clip(int x, int y) const noexcept = 0;
    virtual void set_pixel(int x, int y, const rgba& c) noexcept = 0;
    virtual void draw_hline(int x0, int x1, int y, const rgba& c) noexcept = 0;
    virtual void draw_vline(int x, int y0, int y1, const rgba& c) noexcept = 0;
    virtual rgba get_pixel(int x, int y) const noexcept = 0;
    virtual void blit_span(int x, int y, const std::byte* src, std::size_t bytes) noexcept = 0;
    virtual void begin_frame() noexcept = 0;
    virtual void end_frame() noexcept = 0;
    virtual void mark_dirty(const Rect& r) noexcept = 0;
};

export
template<PixelFormat PF, std::size_t W, std::size_t H>
class Canvas : public CanvasBase {
public:
    using FB = FrameBuffer<PF, W, H>;
    static constexpr util::usize kDirtyCapacity = 16;
    using DirtyList = service::DirtyRectList<Rect, kDirtyCapacity>;

    constexpr Canvas(FB& fb) noexcept : fb_(fb) {}

    int width() const noexcept override { return static_cast<int>(W); }
    int height() const noexcept override { return static_cast<int>(H); }
    std::size_t stride_bytes() const noexcept override { return FB::stride_bytes; }
    std::size_t bytes_per_pixel() const noexcept override { return FB::bytes_per_pixel; }
    std::byte* data() noexcept override { return fb_.data(); }
    const std::byte* data() const noexcept override { return fb_.data(); }
    const std::byte* row_ptr(int y) const noexcept override {
        if (y < 0 || y >= static_cast<int>(H)) return nullptr;
        return fb_.data() + static_cast<std::size_t>(y) * FB::stride_bytes;
    }

    void clear(const rgba& c = {0,0,0,0}) noexcept override { fb_.clear(c); }

    void set_origin(int ox, int oy) noexcept override {
        origin_x_ = ox;
        origin_y_ = oy;
    }

    void clear_origin() noexcept override {
        origin_x_ = 0;
        origin_y_ = 0;
    }

    void set_clip(const Rect& r) noexcept override {
        clip_enabled_ = true;
        clip_ = r;
    }

    void clear_clip() noexcept override { clip_enabled_ = false; }

    ClipState save_clip() const noexcept override { return ClipState{clip_enabled_, clip_}; }
    void restore_clip(const ClipState& state) noexcept override {
        clip_enabled_ = state.enabled;
        clip_ = state.rect;
    }

    bool in_clip(int x, int y) const noexcept override {
        if (!clip_enabled_) return true;
        return x >= clip_.x && y >= clip_.y && x < clip_.x + clip_.w && y < clip_.y + clip_.h;
    }

    void set_pixel(int x, int y, const rgba& c) noexcept override {
        if (!in_clip(x, y)) return;
        const int lx = x + origin_x_;
        const int ly = y + origin_y_;
        if (lx < 0 || ly < 0) return;
        if (lx >= static_cast<int>(W) || ly >= static_cast<int>(H)) return;
        fb_.set_pixel(static_cast<std::size_t>(lx), static_cast<std::size_t>(ly), c);
    }

    void draw_hline(int x0, int x1, int y, const rgba& c) noexcept override {
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

    void draw_vline(int x, int y0, int y1, const rgba& c) noexcept override {
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

    rgba get_pixel(int x, int y) const noexcept override {
        const int lx = x + origin_x_;
        const int ly = y + origin_y_;
        if (lx < 0 || ly < 0) return {};
        if (lx >= static_cast<int>(W) || ly >= static_cast<int>(H)) return {};
        return fb_.get_pixel(static_cast<std::size_t>(lx), static_cast<std::size_t>(ly));
    }

    void blit_span(int x, int y, const std::byte* src, std::size_t bytes) noexcept override {
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

    void begin_frame() noexcept override { dirty_.clear(); }
    void end_frame() noexcept override {}

    void mark_dirty(const Rect& r) noexcept override {
        if (r.w <= 0 || r.h <= 0) return;
        if (dirty_.full()) return;
        if (!dirty_.add(r)) {
            dirty_.set_full(full_rect());
        }
    }

    [[nodiscard]] const DirtyList& dirty_list() const noexcept { return dirty_; }
    [[nodiscard]] bool dirty_full() const noexcept { return dirty_.full(); }

private:
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
