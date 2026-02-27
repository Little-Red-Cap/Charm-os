module;

#include <array>

export module charm.widgets.dropdown;



import charm.core.object;

import charm.core.string;

import charm.gfx.color;

import charm.gfx.render;

import charm.core.event;

import charm.core.style;

import charm.core.style_sheet;

import charm.widgets.label;



using namespace ui::render;



// Simple dropdown control (data + selection only).

export

class Dropdown : public WidgetBase<Dropdown> {

public:

    Dropdown() {

        set_focusable(true);

        set_size(160, 28);

        options_[0].assign("Option 1");

        option_count_ = 1;

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

    int option_count() const noexcept { return option_count_; }

    const char* option_text(int idx) const noexcept {

        if (idx < 0 || idx >= option_count_) return nullptr;

        return options_[idx].c_str();

    }



    void set_on_change(Callback cb) noexcept { on_change_ = cb; }

    void set_on_open(Callback cb) noexcept { on_open_ = cb; }



    void draw(CanvasBase& cvs) {

        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Dropdown>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::Dropdown, state, base, st_scratch);
        const auto r = get_rect();



        rgba bg{};

        rgba border{};

        rgba font{};


        resolve_colors(st, state, bg, border, font);



        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);

        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);



        const char* text = (selected_ >= 0 && selected_ < option_count_) ? options_[selected_].c_str() : "";

        Label lbl{text};

        lbl.set_color(font);

        lbl.set_font(resolve_font(st));

        const int baseline_y = r.y + (r.h - lbl.line_height()) / 2 + lbl.baseline();

        lbl.set_baseline_pos(r.x + st.padding, baseline_y);

        lbl.draw(cvs);



        // simple arrow

        const int ax = r.x + r.w - st.padding - 6;

        const int ay = r.y + r.h / 2 - 3;

        draw_line(cvs, ax, ay, ax + 6, ay + 6, border);

        draw_line(cvs, ax + 6, ay + 6, ax + 12, ay, border);

    }



    bool on_event(const Event& e) {

        if (!is_enabled()) return false;

        if (e.type == Event::Type::Click) {

            if (get_rect().contains(e.x, e.y) || has_state(State::Focused)) {

                if (on_open_) { on_open_(); return true; }

                cycle(1);

                return true;

            }

        } else if (e.type == Event::Type::KeyDown) {

            if (e.key_code == Event::Key::Enter || e.key_code == Event::Key::Space) {

                if (on_open_) { on_open_(); return true; }

                cycle(1);

                return true;

            } else if (e.key_code == Event::Key::Down) {

                cycle(1);

                return true;

            } else if (e.key_code == Event::Key::Up) {

                cycle(-1);

                return true;

            }

        }

        return false;

    }



private:

    void cycle(int delta) {

        if (option_count_ == 0) return;

        selected_ = (selected_ + delta + option_count_) % option_count_;

        if (on_change_) on_change_();

    }



    static constexpr int max_options = 8;

    StaticString<32> options_[max_options]{};

    int option_count_{0};

    int selected_{0};

    Callback on_change_{};

    Callback on_open_{};

};









