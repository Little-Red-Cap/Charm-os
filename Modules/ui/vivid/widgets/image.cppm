module;
export module charm.widgets.image;

import charm.core.object;
import charm.gfx.image;
import charm.gfx.render;

using namespace ui::render;

export
class Image : public ObjectBase {
public:
    Image() = default;

    void set_image(const ImageView& img) noexcept {
        image_ = img;
        if (image_) {
            set_size(image_.w, image_.h);
        }
    }

    const ImageView& image() const noexcept { return image_; }

    void draw(DefaultCanvas& cvs) override {
        if (!image_) return;
        const auto r = get_rect();
        if (r.w != image_.w || r.h != image_.h) {
            draw_image_scaled(cvs, r.x, r.y, r.w, r.h, image_);
        } else {
            draw_image(cvs, r.x, r.y, image_);
        }
    }

private:
    ImageView image_{};
};
