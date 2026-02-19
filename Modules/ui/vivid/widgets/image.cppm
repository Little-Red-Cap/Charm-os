module;
#include <cstddef>
export module charm.widgets.image;

import charm.core.object;
import charm.core.geometry;
import charm.gfx.image;
import charm.gfx.render;

using namespace ui::render;

export
class Image : public ObjectBase {
public:
    enum class AlignH {
        Left,
        Center,
        Right
    };

    enum class AlignV {
        Top,
        Center,
        Bottom
    };

    enum class ScaleMode {
        None,
        Stretch,
        Fit,
        Fill
    };

    Image() = default;

    void set_image(const ImageView& img) noexcept {
        image_ = img;
        if (image_) {
            set_size(image_.w, image_.h);
        }
    }

    const ImageView& image() const noexcept { return image_; }

    void set_scale_mode(ScaleMode mode) noexcept { scale_mode_ = mode; }
    ScaleMode scale_mode() const noexcept { return scale_mode_; }

    void set_alignment(AlignH h, AlignV v) noexcept {
        align_h_ = h;
        align_v_ = v;
    }

    void set_crop(const Rect& r) noexcept {
        crop_ = r;
        has_crop_ = true;
    }

    void clear_crop() noexcept { has_crop_ = false; }

    void draw(DefaultCanvas& cvs) override {
        if (!image_) return;
        const auto r = get_rect();
        Rect src{0, 0, image_.w, image_.h};
        if (has_crop_) {
            src.x = (crop_.x < 0) ? 0 : crop_.x;
            src.y = (crop_.y < 0) ? 0 : crop_.y;
            src.w = crop_.w;
            src.h = crop_.h;
            if (src.x + src.w > image_.w) src.w = image_.w - src.x;
            if (src.y + src.h > image_.h) src.h = image_.h - src.y;
            if (src.w <= 0 || src.h <= 0) return;
        }

        const ImageView src_view = make_subview(image_, src);
        if (!src_view) return;

        int dst_w = src_view.w;
        int dst_h = src_view.h;
        if (scale_mode_ == ScaleMode::Stretch) {
            dst_w = r.w;
            dst_h = r.h;
        } else if (scale_mode_ == ScaleMode::Fit) {
            if (src_view.w <= 0 || src_view.h <= 0) return;
            const float sx = static_cast<float>(r.w) / static_cast<float>(src_view.w);
            const float sy = static_cast<float>(r.h) / static_cast<float>(src_view.h);
            const float s = (sx < sy) ? sx : sy;
            dst_w = static_cast<int>(src_view.w * s);
            dst_h = static_cast<int>(src_view.h * s);
        } else if (scale_mode_ == ScaleMode::Fill) {
            if (src_view.w <= 0 || src_view.h <= 0) return;
            const float sx = static_cast<float>(r.w) / static_cast<float>(src_view.w);
            const float sy = static_cast<float>(r.h) / static_cast<float>(src_view.h);
            const float s = (sx > sy) ? sx : sy;
            dst_w = static_cast<int>(src_view.w * s);
            dst_h = static_cast<int>(src_view.h * s);
        }

        int dst_x = r.x;
        int dst_y = r.y;
        if (align_h_ == AlignH::Center) {
            dst_x = r.x + (r.w - dst_w) / 2;
        } else if (align_h_ == AlignH::Right) {
            dst_x = r.x + r.w - dst_w;
        }
        if (align_v_ == AlignV::Center) {
            dst_y = r.y + (r.h - dst_h) / 2;
        } else if (align_v_ == AlignV::Bottom) {
            dst_y = r.y + r.h - dst_h;
        }

        auto clip_state = cvs.save_clip();
        cvs.set_clip(r);
        if (dst_w != src_view.w || dst_h != src_view.h) {
            draw_image_scaled(cvs, dst_x, dst_y, dst_w, dst_h, src_view);
        } else {
            draw_image(cvs, dst_x, dst_y, src_view);
        }
        cvs.restore_clip(clip_state);
    }

private:
    static int bytes_per_pixel(PixelFormat fmt) noexcept {
        switch (fmt) {
        case PixelFormat::RGB565: return 2;
        case PixelFormat::RGB888: return 3;
        case PixelFormat::ARGB8888: return 4;
        default: return 4;
        }
    }

    static ImageView make_subview(const ImageView& img, const Rect& r) noexcept {
        if (!img) return {};
        const int x = (r.x < 0) ? 0 : r.x;
        const int y = (r.y < 0) ? 0 : r.y;
        const int w = r.w;
        const int h = r.h;
        if (w <= 0 || h <= 0) return {};
        const int bpp = bytes_per_pixel(img.format);
        const std::byte* data = img.data + y * img.stride_bytes + x * bpp;
        return make_image_view(img.format, w, h, img.stride_bytes, data, img.premultiplied_alpha, img.force_opaque);
    }

    ImageView image_{};
    ScaleMode scale_mode_{ScaleMode::Stretch};
    AlignH align_h_{AlignH::Center};
    AlignV align_v_{AlignV::Center};
    Rect crop_{};
    bool has_crop_{false};
};
