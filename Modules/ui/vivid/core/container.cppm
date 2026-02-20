module;
#include <cwchar>
export module charm.core.container;

export import charm.core.object;
export import charm.gfx.color;
export import charm.gfx.render;
export import charm.gfx.image;

using namespace ui::render;

export
class Container : public ObjectBase {
public:
    void set_background(const rgba& c) noexcept { bg_ = c; has_bg_ = true; }
    void set_background_image(const ImageView& img, int left, int top, int right, int bottom) noexcept {
        bg_image_ = img;
        slice_left_ = left;
        slice_top_ = top;
        slice_right_ = right;
        slice_bottom_ = bottom;
        has_bg_image_ = true;
    }

    void draw(CanvasBase& cvs) override {
        const auto r = get_rect();
        if (has_bg_image_) {
            draw_image_nine_slice(cvs, r.x, r.y, r.w, r.h, bg_image_,
                                  slice_left_, slice_top_, slice_right_, slice_bottom_);
        } else if (has_bg_) {
            draw_rect(cvs, r.x, r.y, r.w, r.h, bg_, true);
        }
    }

private:
    rgba bg_{};
    bool has_bg_{false};
    ImageView bg_image_{};
    bool has_bg_image_{false};
    int slice_left_{0};
    int slice_top_{0};
    int slice_right_{0};
    int slice_bottom_{0};
};
