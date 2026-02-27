module;

#include <cstddef>

export module charm.widgets.histogram_view;



import charm.core.object;

import charm.core.style;

import charm.core.style_sheet;

import charm.gfx.color;

import charm.gfx.render;



using namespace ui::render;



// Simple histogram view (fixed buffer)

export

class HistogramView : public WidgetBase<HistogramView> {

public:

    static constexpr std::size_t kMax = 32;



    HistogramView() {

        set_size(220, 120);

    }



    void set_values(const int* values, int count) {

        if (!values || count <= 0) { count_ = 0; return; }

        const int cap = (count < static_cast<int>(kMax)) ? count : static_cast<int>(kMax);

        for (int i = 0; i < cap; ++i) values_[i] = values[i];

        count_ = cap;

    }



    void set_range(int min_v, int max_v) noexcept {

        if (min_v > max_v) {

            const int tmp = min_v;

            min_v = max_v;

            max_v = tmp;

        }

        min_v_ = min_v;

        max_v_ = max_v;

        has_range_ = true;

    }



    void clear_range() noexcept { has_range_ = false; }



    void draw(CanvasBase& cvs) {

        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<HistogramView>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::HistogramView, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);

        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        if (count_ <= 0) return;



        int min_v = has_range_ ? min_v_ : values_[0];

        int max_v = has_range_ ? max_v_ : values_[0];

        if (!has_range_) {

            for (int i = 1; i < count_; ++i) {

                if (values_[i] < min_v) min_v = values_[i];

                if (values_[i] > max_v) max_v = values_[i];

            }

        }

        const int range = (max_v - min_v) == 0 ? 1 : (max_v - min_v);



        const int left = r.x + 4;

        const int right = r.x + r.w - 4;

        const int top = r.y + 4;

        const int bottom = r.y + r.h - 4;

        const int inner_w = right - left;

        const int inner_h = bottom - top;

        if (inner_w <= 0 || inner_h <= 0) return;



        for (int i = 0; i < count_; ++i) {

            const int x0 = left + inner_w * i / count_;

            const int x1 = left + inner_w * (i + 1) / count_;

            int w = x1 - x0 - 1;

            if (w < 1) w = 1;

            const int h = inner_h * (values_[i] - min_v) / range;

            if (h <= 0) continue;

            draw_rect(cvs, x0, bottom - h, w, h, accent, true);
        }
    }



private:

    int values_[kMax]{};

    int count_{0};

    int min_v_{0};

    int max_v_{0};

    bool has_range_{false};

};









