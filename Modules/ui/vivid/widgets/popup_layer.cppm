module;
export module charm.widgets.popup_layer;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;

using namespace ui::render;

// 简易弹层容器：只负责画背景和裁剪，真正内容由子控件承担
export
class PopupLayer : public ObjectBase {
public:
    PopupLayer() {
        set_visible(false);
    }

    void set_background(rgba bg) noexcept { bg_ = bg; has_bg_ = true; }

    void draw(CanvasBase& cvs) override {
        if (!is_visible()) return;
        const auto r = get_rect();
        if (has_bg_) {
            draw_rect(cvs, r.x, r.y, r.w, r.h, bg_, true);
        }
        // border optional
    }

private:
    bool has_bg_{false};
    rgba bg_{0,0,0,0};
};

