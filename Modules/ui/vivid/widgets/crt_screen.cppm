module;
#include <cstddef>
#include <cstdint>
export module charm.widgets.crt_screen;

import charm.core.object;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.image;
import charm.gfx.render;

using namespace ui::render;

// CRT screen effect (ARM-2D crt_screen inspired)
export
class CrtScreen : public ObjectBase {
public:
    CrtScreen() {
        set_size(220, 160);
    }

    void set_image(const ImageView& img) noexcept { image_ = img; }
    const ImageView& image() const noexcept { return image_; }

    void set_noise_enabled(bool on) noexcept { noise_enabled_ = on; }
    void set_noise_opacity(std::uint8_t a) noexcept { noise_opacity_ = a; }
    void set_noise_strength(int px) noexcept { noise_strength_ = (px > 0) ? px : 1; }

    void set_scan_enabled(bool on) noexcept { scan_enabled_ = on; }
    void set_scan_bar_height(int h0, int h1) noexcept {
        scan_bar_h0_ = (h0 > 0) ? h0 : 1;
        scan_bar_h1_ = (h1 > 0) ? h1 : 1;
    }
    void set_scan_speed(int s0, int s1) noexcept {
        scan_speed0_ = (s0 != 0) ? s0 : 1;
        scan_speed1_ = (s1 != 0) ? s1 : 1;
    }
    void set_scan_opacity(std::uint8_t a0, std::uint8_t a1) noexcept {
        scan_opacity0_ = a0;
        scan_opacity1_ = a1;
    }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<CrtScreen>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        if (image_) {
            draw_image_scaled(cvs, r.x, r.y, r.w, r.h, image_);
        }

        if (noise_enabled_) {
            draw_noise(cvs, r, font);
        }
        if (scan_enabled_) {
            draw_scan_bars(cvs, r, font);
        }
    }

private:
    void draw_noise(DefaultCanvas& cvs, const Rect& r, const rgba& base) noexcept {
        const int count = (r.w * r.h) / (noise_strength_ * noise_strength_ * 64);
        for (int i = 0; i < count; ++i) {
            const int x = r.x + (random_u32() % (r.w > 0 ? r.w : 1));
            const int y = r.y + (random_u32() % (r.h > 0 ? r.h : 1));
            const int w = (random_u32() % noise_strength_) + 1;
            const int h = (random_u32() % noise_strength_) + 1;
            rgba n = base;
            n.a = noise_opacity_;
            draw_rect(cvs, x, y, w, h, n, true);
        }
    }

    void draw_scan_bars(DefaultCanvas& cvs, const Rect& r, const rgba& base) noexcept {
        scan_pos0_ += scan_speed0_;
        scan_pos1_ += scan_speed1_;
        if (scan_pos0_ > r.h) scan_pos0_ = -scan_bar_h0_;
        if (scan_pos1_ > r.h) scan_pos1_ = -scan_bar_h1_;

        rgba b0 = base;
        b0.a = scan_opacity0_;
        rgba b1 = base;
        b1.a = scan_opacity1_;
        draw_rect(cvs, r.x, r.y + scan_pos0_, r.w, scan_bar_h0_, b0, true);
        draw_rect(cvs, r.x, r.y + scan_pos1_, r.w, scan_bar_h1_, b1, true);
    }

    std::uint32_t random_u32() noexcept {
        rng_ = rng_ * 1664525u + 1013904223u;
        return rng_;
    }

    ImageView image_{};
    bool noise_enabled_{true};
    std::uint8_t noise_opacity_{60};
    int noise_strength_{6};
    bool scan_enabled_{true};
    int scan_bar_h0_{22};
    int scan_bar_h1_{4};
    int scan_speed0_{2};
    int scan_speed1_{5};
    std::uint8_t scan_opacity0_{64};
    std::uint8_t scan_opacity1_{120};
    int scan_pos0_{0};
    int scan_pos1_{0};
    std::uint32_t rng_{0x87654321u};
};
