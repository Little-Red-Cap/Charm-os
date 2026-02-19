module;
#include <cstdio>
export module charm.widgets.perf_overlay;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.canvas;
import charm.gfx.render;
import charm.widgets.text;
import charm.font.typography;
import charm.core.style;

using namespace ui::render;

export
class PerfOverlay : public ObjectBase {
public:
    struct Sample {
        int fps{0};
        int draw_ms{0};
        int dirty_count{0};
        int dirty_area{0};
        int nodes{0};
        int depth_hits{0};
        int cycle_hits{0};
    };

    PerfOverlay() {
        set_focusable(false);
    }

    void set_sample(const Sample& s) noexcept {
        sample_ = s;
        has_sample_ = true;
    }

    void clear_sample() noexcept {
        sample_ = {};
        has_sample_ = false;
    }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<PerfOverlay>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);

        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, bg, true);
        for (int i = 0; i < st.border_width; ++i) {
            draw_round_rect(cvs, r.x + i, r.y + i, r.w - 2 * i, r.h - 2 * i, st.corner_radius, border, false);
        }

        const Font& ft = resolve_font(st);
        const int line_h = (ft.line_height > 0) ? ft.line_height : 12;
        const int start_x = r.x + st.padding;
        int y = r.y + st.padding;

        char buf[96]{};
        if (!has_sample_) {
            std::snprintf(buf, sizeof(buf), "perf: n/a");
            draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
            return;
        }

        std::snprintf(buf, sizeof(buf), "fps: %d  draw: %d ms", sample_.fps, sample_.draw_ms);
        draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
        y += line_h;

        std::snprintf(buf, sizeof(buf), "dirty: %d  area: %d", sample_.dirty_count, sample_.dirty_area);
        draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
        y += line_h;

        std::snprintf(buf, sizeof(buf), "nodes: %d  depth: %d  cycle: %d",
                      sample_.nodes, sample_.depth_hits, sample_.cycle_hits);
        draw_text_baseline(cvs, start_x, y + ft.baseline, buf, font, ft);
    }

private:
    Sample sample_{};
    bool has_sample_{false};
};
