module;
#include <cstddef>
#include <algorithm>
#include "vivid_features.generated.hpp"
#if CHARM_VIVID_ENABLE_FLOAT_WIDGETS
#include <cmath>
#endif
export module charm.widgets.primitives_canvas;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import alg_arc;

using namespace ui::render;

export
class PrimitivesCanvas : public WidgetBase<PrimitivesCanvas> {
public:
    PrimitivesCanvas() {
        set_focusable(false);
        set_size(800, 480);
    }

    void set_mode(int m) noexcept { mode_ = m; }
    void set_time(float t) noexcept { time_ = t; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<PrimitivesCanvas>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::PrimitivesCanvas, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int cols = 3;
        const int rows = 2;
        const int cell_w = r.w / cols;
        const int cell_h = r.h / rows;
        const int pad = 10;

        for (int ry = 0; ry < rows; ++ry) {
            for (int cx = 0; cx < cols; ++cx) {
                const int idx = ry * cols + cx;
                const int x = r.x + cx * cell_w + pad;
                const int y = r.y + ry * cell_h + pad;
                const int w = cell_w - pad * 2;
                const int h = cell_h - pad * 2;
                draw_cell(cvs, idx, x, y, w, h, font, border);
            }
        }
    }

private:
    int mode_{0};
    float time_{0.0f};

    static void draw_thick_line(CanvasBase& cvs,
                                int x0, int y0, int x1, int y1,
                                int thickness, const rgba& color) {
        if (thickness <= 1) {
            draw_line(cvs, x0, y0, x1, y1, color);
            return;
        }
        const int dx = x1 - x0;
        const int dy = y1 - y0;
        const int adx = (dx < 0) ? -dx : dx;
        const int ady = (dy < 0) ? -dy : dy;
        const bool offset_y = adx >= ady;
        const int half = thickness / 2;
        for (int i = -half; i <= half; ++i) {
            if (offset_y) {
                draw_line(cvs, x0, y0 + i, x1, y1 + i, color);
            } else {
                draw_line(cvs, x0 + i, y0, x1 + i, y1, color);
            }
        }
    }

    void draw_cell(CanvasBase& cvs, int idx, int x, int y, int w, int h,
                   const rgba& main, const rgba& border) {
        draw_rect(cvs, x, y, w, h, rgba{0,0,0,0}, false);
        const int cx = x + w / 2;
        const int cy = y + h / 2;

        switch (idx) {
        case 0: { // points
            for (int i = 0; i < 12; ++i) {
                const int px = x + 6 + i * 12;
                const int py = y + 10 + (i % 3) * 10;
                draw_rect(cvs, px, py, 2, 2, main, true);
            }
            break;
        }
        case 1: { // lines
            draw_line(cvs, x + 10, y + 10, x + w - 10, y + 10, main);
            draw_line(cvs, x + 10, y + 26, x + w - 10, y + 26, main);
            draw_thick_line(cvs, x + 10, y + 44, x + w - 10, y + 44, 3, main);
            draw_line(cvs, x + 10, y + 64, x + w - 10, y + h - 10, main);
            break;
        }
        case 2: { // rectangles
            draw_rect(cvs, x + 10, y + 10, w - 20, h - 20, border, false);
            draw_rect(cvs, x + 20, y + 24, w - 40, h - 48, main, true);
            draw_round_rect(cvs, x + 36, y + 38, w - 72, h - 76, 8, border, false);
            break;
        }
        case 3: { // circles
            const int r1 = std::min(w, h) / 3;
            draw_circle(cvs, cx, cy, r1, main, false);
            draw_circle(cvs, cx, cy, r1 / 2, main, true);
            break;
        }
        case 4: { // arc / gauge style
            const int r1 = std::min(w, h) / 3;
#if CHARM_VIVID_ENABLE_FLOAT_WIDGETS
            draw_arc(cvs, cx, cy, r1, 4, 135, 405, border);
            const float sweep = 135.0f + std::fmod(time_ * 90.0f, 270.0f);
            draw_arc(cvs, cx, cy, r1, 4, 135, sweep, main);
#else
            draw_circle(cvs, cx, cy, r1, border, false);
#endif
            break;
        }
        case 5: { // animated diagonal
#if CHARM_VIVID_ENABLE_FLOAT_WIDGETS
            const float t = time_;
            const auto wobble = alg::arc::point_on_ellipse_rad(0, 0, w / 4, h / 4, t);
            const int dx = wobble.x;
            const int dy = wobble.y;
            draw_thick_line(cvs, x + 12 + dx, y + 12, x + w - 12, y + h - 12 + dy, 4, main);
#else
            draw_thick_line(cvs, x + 12, y + 12, x + w - 12, y + h - 12, 4, main);
#endif
            break;
        }
        default:
            break;
        }
    }
};




