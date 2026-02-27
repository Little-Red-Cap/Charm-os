module;
export module charm.widgets.progress_flowing;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;
import alg_arc;

using namespace ui::render;

// Flowing progress bar (0..100 or indeterminate)
export
class ProgressFlowing : public WidgetBase<ProgressFlowing> {
public:
    ProgressFlowing() {
        set_size(180, 16);
    }

    void set_range(int min_v, int max_v) noexcept {
        if (min_v > max_v) {
            const int tmp = min_v;
            min_v = max_v;
            max_v = tmp;
        }
        min_ = min_v;
        max_ = max_v;
        set_value(value_);
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, min_, max_);
        indeterminate_ = false;
    }

    void set_indeterminate(bool on) noexcept { indeterminate_ = on; }
    bool is_indeterminate() const noexcept { return indeterminate_; }

    void set_animation_enabled(bool on) noexcept { set_flow_enabled(on); }
    void set_animation_speed(float px) noexcept { set_flow_speed(static_cast<int>(px)); }

    void set_flow_enabled(bool on) noexcept { flow_enabled_ = on; }
    void set_flow_speed(int px) noexcept { flow_speed_ = (px > 0) ? px : 1; }
    void set_flow_span(int px) noexcept { flow_span_ = (px > 2) ? px : 2; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<ProgressFlowing>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::ProgressFlowing, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int inner_x = r.x + 1;
        const int inner_y = r.y + 1;
        const int inner_w = r.w - 2;
        const int inner_h = r.h - 2;
        if (inner_w <= 0 || inner_h <= 0) return;

        int filled_w = inner_w;
        if (!indeterminate_) {
            const float ratio = alg::arc::ratio_from_range(value_, min_, max_);
            filled_w = static_cast<int>(inner_w * ratio);
            if (filled_w < 0) filled_w = 0;
            if (filled_w > inner_w) filled_w = inner_w;
        }

        if (filled_w > 0) {
            draw_rect(cvs, inner_x, inner_y, filled_w, inner_h, accent, true);
        }

        if (flow_enabled_ && inner_h > 0 && filled_w > 0) {
            const int step = flow_span_ * 2;
            int offset = flow_offset_ % step;
            int start_x = inner_x - offset;
            rgba flow = st.border_focus;
            flow.a = 180;
            for (int x = start_x; x < inner_x + filled_w; x += step) {
                const int seg_x = x;
                int seg_w = flow_span_;
                if (seg_x < inner_x) {
                    seg_w -= (inner_x - seg_x);
                }
                if (seg_x + seg_w > inner_x + filled_w) {
                    seg_w = inner_x + filled_w - seg_x;
                }
                if (seg_w > 0) {
                    const int draw_x = (seg_x < inner_x) ? inner_x : seg_x;
                    draw_rect(cvs, draw_x, inner_y, seg_w, inner_h, flow, true);
                }
            }
            flow_offset_ += flow_speed_;
            if (flow_offset_ > step) flow_offset_ = flow_offset_ % step;
        }
    }

private:
    int min_{0};
    int max_{100};
    int value_{0};
    bool indeterminate_{false};
    bool flow_enabled_{true};
    int flow_speed_{2};
    int flow_span_{10};
    int flow_offset_{0};
};




