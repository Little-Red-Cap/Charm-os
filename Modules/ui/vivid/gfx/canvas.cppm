module;
#include <cstddef>
#include <utility>
export module charm.gfx.canvas;

export import charm.gfx.framebuffer;
export import charm.core.config;
export import charm.core.geometry;
import service_dirty_rects;
import util.core;

export
template<PixelFormat PF, std::size_t W, std::size_t H>
class Canvas {
public:
    using FB = FrameBuffer<PF, W, H>;
    static constexpr util::usize kDirtyCapacity = 16;
    using DirtyList = service::DirtyRectList<Rect, kDirtyCapacity>;

    constexpr Canvas(FB& fb) noexcept : fb_(fb) {}

    constexpr std::size_t width() const noexcept { return W; }
    constexpr std::size_t height() const noexcept { return H; }

    void clear(const rgba& c = {0,0,0,0}) noexcept { fb_.clear(c); }

    void set_clip(const Rect& r) noexcept {
        clip_enabled_ = true;
        clip_ = r;
    }

    void clear_clip() noexcept { clip_enabled_ = false; }

    struct ClipState {
        bool enabled{false};
        Rect rect{};
    };

    ClipState save_clip() const noexcept { return ClipState{clip_enabled_, clip_}; }
    void restore_clip(const ClipState& state) noexcept {
        clip_enabled_ = state.enabled;
        clip_ = state.rect;
    }

    bool in_clip(int x, int y) const noexcept {
        if (!clip_enabled_) return true;
        return x >= clip_.x && y >= clip_.y && x < clip_.x + clip_.w && y < clip_.y + clip_.h;
    }

    void set_pixel(int x, int y, const rgba& c) noexcept {
        if (!in_clip(x, y)) return;
        fb_.set_pixel(static_cast<std::size_t>(x), static_cast<std::size_t>(y), c);
    }

    void draw_hline(std::size_t x0, std::size_t x1, std::size_t y, const rgba& c) noexcept {
        if (y >= H) return;
        if (x0 > x1) std::swap(x0, x1);
        if (x1 > W) x1 = W;
        for (std::size_t x = x0; x < x1; ++x) {
            set_pixel(static_cast<int>(x), static_cast<int>(y), c);
        }
    }

    void draw_vline(std::size_t x, std::size_t y0, std::size_t y1, const rgba& c) noexcept {
        if (x >= W) return;
        if (y0 > y1) std::swap(y0, y1);
        if (y1 > H) y1 = H;
        for (std::size_t y = y0; y < y1; ++y) {
            set_pixel(static_cast<int>(x), static_cast<int>(y), c);
        }
    }

    void begin_frame() noexcept { dirty_.clear(); }
    void end_frame() noexcept {}

    void mark_dirty(const Rect& r) noexcept {
        if (r.w <= 0 || r.h <= 0) return;
        if (dirty_.full()) return;
        if (!dirty_.add(r)) {
            dirty_.set_full(full_rect());
        }
    }

    [[nodiscard]] const DirtyList& dirty_list() const noexcept { return dirty_; }
    [[nodiscard]] bool dirty_full() const noexcept { return dirty_.full(); }

    FB& raw_buffer() noexcept { return fb_; }
    const FB& raw_buffer() const noexcept { return fb_; }

private:
    constexpr Rect full_rect() const noexcept {
        return Rect{0, 0, static_cast<int>(W), static_cast<int>(H)};
    }

    FB& fb_;
    bool clip_enabled_{false};
    Rect clip_{0, 0, static_cast<int>(W), static_cast<int>(H)};
    DirtyList dirty_{};
};

export
using DefaultCanvas = Canvas<screen_pixel_format,
                             static_cast<std::size_t>(screen_width),
                             static_cast<std::size_t>(screen_height)>;
