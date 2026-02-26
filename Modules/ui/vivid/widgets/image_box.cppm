module;
#include <cstddef>
export module charm.widgets.image_box;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.geometry;
import charm.gfx.color;
import charm.gfx.image;
import charm.gfx.render;

using namespace ui::render;

// Simple image box (ARM-2D image_box inspired)
export
class ImageBox : public ObjectBase {
public:
    enum class ScaleMode {
        None,
        Fit,
        Fill,
        Stretch
    };

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

    ImageBox() {
        set_size(140, 120);
    }

    void set_image(const ImageView& img) noexcept { image_ = img; }
    const ImageView& image() const noexcept { return image_; }

    void set_scale_mode(ScaleMode m) noexcept { scale_mode_ = m; }
    ScaleMode scale_mode() const noexcept { return scale_mode_; }

    void set_alignment(AlignH h, AlignV v) noexcept {
        align_h_ = h;
        align_v_ = v;
    }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<ImageBox>();
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        const StyleState state{is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)};
        apply_style_sheet(WidgetKind::ImageBox, state, st);
        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int pad = st.padding;
        Rect inner{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
        if (inner.w <= 0 || inner.h <= 0) return;
        if (!image_) return;

        int dst_w = image_.w;
        int dst_h = image_.h;
        if (scale_mode_ == ScaleMode::Stretch) {
            dst_w = inner.w;
            dst_h = inner.h;
        } else if (scale_mode_ == ScaleMode::Fit || scale_mode_ == ScaleMode::Fill) {
            if (image_.w <= 0 || image_.h <= 0) return;
            const float sx = static_cast<float>(inner.w) / static_cast<float>(image_.w);
            const float sy = static_cast<float>(inner.h) / static_cast<float>(image_.h);
            const float s = (scale_mode_ == ScaleMode::Fit) ? ((sx < sy) ? sx : sy) : ((sx > sy) ? sx : sy);
            dst_w = static_cast<int>(image_.w * s);
            dst_h = static_cast<int>(image_.h * s);
        }

        int dst_x = inner.x;
        int dst_y = inner.y;
        if (align_h_ == AlignH::Center) {
            dst_x = inner.x + (inner.w - dst_w) / 2;
        } else if (align_h_ == AlignH::Right) {
            dst_x = inner.x + inner.w - dst_w;
        }
        if (align_v_ == AlignV::Center) {
            dst_y = inner.y + (inner.h - dst_h) / 2;
        } else if (align_v_ == AlignV::Bottom) {
            dst_y = inner.y + inner.h - dst_h;
        }

        auto clip_state = cvs.save_clip();
        cvs.set_clip(inner);
        if (dst_w == image_.w && dst_h == image_.h) {
            draw_image(cvs, dst_x, dst_y, image_);
        } else {
            draw_image_scaled(cvs, dst_x, dst_y, dst_w, dst_h, image_);
        }
        cvs.restore_clip(clip_state);
    }

private:
    ImageView image_{};
    ScaleMode scale_mode_{ScaleMode::Fit};
    AlignH align_h_{AlignH::Center};
    AlignV align_v_{AlignV::Center};
};
