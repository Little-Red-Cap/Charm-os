module;

#include <cmath>

#include <cstdint>

export module charm.widgets.roller;



import charm.core.object;

import charm.core.event;

import charm.core.style;

import charm.core.style_sheet;

import charm.core.string;

import charm.gfx.color;

import charm.gfx.render;

import charm.widgets.label;



using namespace ui::render;



export

class Roller : public WidgetBase<Roller> {

public:

    Roller() {

        set_size(140, 80);

        set_focusable(true);

        add_option("Item 1");

        add_option("Item 2");

        add_option("Item 3");

    }



    void add_option(const char* txt) noexcept {

        if (option_count_ >= max_options) return;

        options_[option_count_++].assign(txt ? txt : "");

    }



    void set_selected(int idx) noexcept {

        if (idx < 0 || idx >= option_count_) return;

        selected_ = idx;

        if (on_change_) on_change_();

    }



    int selected() const noexcept { return selected_; }



    void set_on_change(Callback cb) noexcept { on_change_ = cb; }



    void draw(CanvasBase& cvs) {

        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Roller>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::Roller, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};

        rgba border{};

        rgba font{};


        resolve_colors(st, state, bg, border, font);



        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, bg, true);

        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, border, false);



        const int line_h = resolve_font(st).line_height;

        const int center_y = r.y + r.h / 2;

        const int visible = 2; // above and below

        for (int i = -visible; i <= visible; ++i) {

            int idx = wrap_index(selected_ + i);

            if (idx < 0) continue;

            Label lbl{options_[idx].c_str()};

            lbl.set_font(resolve_font(st));

            lbl.set_color(font);

            const int alpha_step = 60;

            int alpha = 255 - std::abs(i) * alpha_step;

            if (alpha < 60) alpha = 60;

            rgba col = font;

            col.a = static_cast<std::uint8_t>(alpha);

            lbl.set_color(col);

            const int baseline_y = center_y + i * line_h + lbl.baseline() - line_h / 2;

            lbl.set_baseline_pos(r.x + st.padding, baseline_y);

            lbl.draw(cvs);

        }



        // focus indicator

        draw_rect(cvs, r.x + 2, center_y - line_h / 2, r.w - 4, line_h, border, false);

    }



    bool on_event(const Event& e) {

        if (!is_enabled()) return false;

        if (e.type == Event::Type::KeyDown) {

            if (e.key_code == Event::Key::Up) {

                step(-1); return true;

            } else if (e.key_code == Event::Key::Down) {

                step(1); return true;

            }

        } else if (e.type == Event::Type::MouseWheel) {

            if (get_rect().contains(e.x, e.y)) {

                step(e.wheel_y < 0 ? 1 : -1);

                return true;

            }

        } else if (e.type == Event::Type::Click) {

            if (get_rect().contains(e.x, e.y)) {

                step(1);

                return true;

            }

        }

        return false;

    }



private:

    void step(int delta) {

        if (option_count_ == 0) return;

        selected_ = wrap_index(selected_ + delta);

        if (on_change_) on_change_();

    }



    int wrap_index(int idx) const noexcept {

        if (option_count_ == 0) return -1;

        idx %= option_count_;

        if (idx < 0) idx += option_count_;

        return idx;

    }



    static constexpr int max_options = 16;

    StaticString<32> options_[max_options]{};

    int option_count_{0};

    int selected_{0};

    Callback on_change_{};

};











