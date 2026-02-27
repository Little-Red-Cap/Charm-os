module;
#include <cstddef>
#include <cstdint>
export module charm.widgets.progress_bar_drill;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render;
import alg_arc;

using namespace ui::render;

// Drill progress bar (ARM-2D progress_bar_drill inspired)
export
class ProgressBarDrill : public WidgetBase<ProgressBarDrill> {
public:
    ProgressBarDrill() {
        set_size(200, 18);
    }

    void set_range(int min_v, int max_v) noexcept {
        if (max_v <= min_v) {
            min_ = 0;
            max_ = 100;
        } else {
            min_ = min_v;
            max_ = max_v;
        }
        set_value(value_);
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, min_, max_);
        indeterminate_ = false;
    }

    int value() const noexcept { return value_; }

    void set_indeterminate(bool on) noexcept { indeterminate_ = on; }
    bool is_indeterminate() const noexcept { return indeterminate_; }

    void set_animation_enabled(bool on) noexcept { anim_enabled_ = on; }
    void set_animation_speed(float px) noexcept { set_flow_speed(static_cast<int>(px)); }

    void set_flow_speed(int px) noexcept { flow_speed_ = (px > 0) ? px : 1; }
    void set_hole_radius(int px) noexcept { hole_radius_ = (px > 0) ? px : 1; }
    void set_hole_spacing(int px) noexcept { hole_spacing_ = (px > 0) ? px : 1; }

    void set_fill_color(const rgba& c) noexcept { fill_color_ = c; }
    void set_track_color(const rgba& c) noexcept { track_color_ = c; }
    void set_hole_color(const rgba& c) noexcept { hole_color_ = c; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<ProgressBarDrill>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::ProgressBarDrill, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int pad = st.metrics.padding;
        Rect inner{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
        if (inner.w <= 0 || inner.h <= 0) return;

        const rgba track = track_color_.a ? track_color_ : border;
        const rgba fill = fill_color_.a ? fill_color_ : accent;
        const rgba hole = hole_color_.a ? hole_color_ : bg;

        draw_round_rect(cvs, inner.x, inner.y, inner.w, inner.h, st.metrics.corner_radius, track, true);

        int fill_x = inner.x;
        int fill_w = inner.w;
        if (indeterminate_) {
            const int span = (inner.w / 3 > 4) ? inner.w / 3 : 4;
            int start = inner.x + (flow_offset_ % (inner.w + span)) - span;
            int end = start + span;
            if (end <= inner.x || start >= inner.x + inner.w) {
                if (anim_enabled_) {
                    flow_offset_ += flow_speed_;
                    if (flow_offset_ > inner.w + span) flow_offset_ = flow_offset_ % (inner.w + span);
                }
                return;
            }
            if (start < inner.x) start = inner.x;
            if (end > inner.x + inner.w) end = inner.x + inner.w;
            fill_x = start;
            fill_w = end - start;
        } else {
            const int range = max_ - min_;
            if (range > 0) {
                const int clamped = alg::arc::clamp_to_range(value_, min_, max_);
                const std::int64_t num = static_cast<std::int64_t>(inner.w) * (clamped - min_);
                fill_w = static_cast<int>(num / range);
            } else {
                fill_w = 0;
            }
            if (fill_w < 0) fill_w = 0;
            if (fill_w > inner.w) fill_w = inner.w;
            if (fill_w == 0) return;
        }

        draw_round_rect(cvs, fill_x, inner.y, fill_w, inner.h, st.metrics.corner_radius, fill, true);

        const int hole_step = hole_spacing_ + hole_radius_ * 2;
        if (hole_step <= 0 || hole_radius_ <= 0) return;

        const int center_y = inner.y + inner.h / 2;
        int hole_start = fill_x + hole_radius_ - (hole_offset_ % hole_step);
        auto clip_state = cvs.save_clip();
        cvs.set_clip(inner);
        for (int x = hole_start; x < fill_x + fill_w; x += hole_step) {
            draw_circle(cvs, x, center_y, hole_radius_, hole, true);
        }
        cvs.restore_clip(clip_state);

        if (anim_enabled_) {
            flow_offset_ += flow_speed_;
            if (flow_offset_ > inner.w + hole_step) flow_offset_ = flow_offset_ % (inner.w + hole_step);
            hole_offset_ += flow_speed_;
            if (hole_offset_ > hole_step) hole_offset_ = hole_offset_ % hole_step;
        }
    }

private:
    int min_{0};
    int max_{100};
    int value_{0};
    bool indeterminate_{false};
    bool anim_enabled_{true};
    int flow_speed_{2};
    int flow_offset_{0};
    int hole_radius_{3};
    int hole_spacing_{6};
    int hole_offset_{0};
    rgba fill_color_{0, 0, 0, 0};
    rgba track_color_{0, 0, 0, 0};
    rgba hole_color_{0, 0, 0, 0};
};




