module;

#include <cstddef>

export module charm.widgets.timeline;



import charm.core.object;

import charm.core.string;

import charm.core.style;

import charm.core.style_sheet;

import charm.gfx.color;

import charm.gfx.render;

import charm.widgets.text;

import charm.font.typography;



using namespace ui::render;



export

class Timeline : public WidgetBase<Timeline> {

public:

    static constexpr std::size_t kMaxItems = 16;



    Timeline() {

        set_size(240, 160);

        set_focusable(false);

    }



    void set_item_count(int count) noexcept {

        if (count < 0) count = 0;

        if (count > static_cast<int>(kMaxItems)) count = static_cast<int>(kMaxItems);

        count_ = count;

        if (current_ >= count_) current_ = (count_ > 0) ? (count_ - 1) : 0;

    }



    void set_item_text(int index, const char* text) {

        if (index < 0 || index >= static_cast<int>(kMaxItems)) return;

        items_[index].assign(text ? text : "");

    }



    void set_current(int index) noexcept {

        if (count_ <= 0) return;

        if (index < 0) index = 0;

        if (index >= count_) index = count_ - 1;

        current_ = index;

    }



    void set_row_height(int h) noexcept { row_h_ = (h > 0) ? h : row_h_; }



    void draw(CanvasBase& cvs) {

        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Timeline>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::Timeline, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);

        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);



        if (count_ <= 0) return;

        const Font& ft = resolve_font(st);

        const int padding = st.padding;

        const int radius = (row_h_ / 2) - 2;

        const int line_x = r.x + padding + radius;

        const int top_y = r.y + padding + row_h_ / 2;

        const int bottom_y = r.y + padding + (count_ - 1) * row_h_ + row_h_ / 2;

        draw_line(cvs, line_x, top_y, line_x, bottom_y, border);



        for (int i = 0; i < count_; ++i) {

            const int cy = r.y + padding + i * row_h_ + row_h_ / 2;
            const bool done = i < current_;
            const bool current = i == current_;
            const rgba fill = current ? accent : (done ? border : bg);
            draw_circle(cvs, line_x, cy, radius, fill, true);
            draw_circle(cvs, line_x, cy, radius, current ? accent : border, false);


            if (items_[i].size() > 0) {

                const char* text = items_[i].c_str();

                const int tx = line_x + radius + padding;

                draw_text_baseline(cvs, tx, cy + ft.baseline - row_h_ / 2, text, font, ft);

            }

        }

    }



private:

    int count_{0};

    int current_{0};

    int row_h_{24};

    StaticString<32> items_[kMaxItems]{};

};









