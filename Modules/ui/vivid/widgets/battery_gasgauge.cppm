module;

#include <cmath>

#include <cstddef>

export module charm.widgets.battery_gasgauge;



import charm.core.object;

import charm.core.style;

import charm.core.style_sheet;

import charm.gfx.color;

import charm.gfx.render;

import alg_arc;



using namespace ui::render;



// Battery gas gauge (ARM-2D battery_gasgauge inspired)

export

class BatteryGasGauge : public WidgetBase<BatteryGasGauge> {

public:

    enum class Status {

        Discharging = -1,

        Idle = 0,

        Charging = 1

    };



    enum class StyleMode {

        NixieTube,

        Liquid

    };



    BatteryGasGauge() {

        set_size(140, 56);

    }



    void set_value(int v) noexcept {

        value_ = alg::arc::clamp_to_range(v, 0, 100);

    }



    int value() const noexcept { return value_; }



    void set_status(Status s) noexcept { status_ = s; }

    Status status() const noexcept { return status_; }



    void set_style_mode(StyleMode m) noexcept { mode_ = m; }

    StyleMode style_mode() const noexcept { return mode_; }



    void set_animation_enabled(bool on) noexcept { anim_enabled_ = on; }

    void set_animation_speed(float s) noexcept { set_wave_speed(s); }



    void set_wave_speed(float s) noexcept { wave_speed_ = s; }

    void set_wave_amplitude(int a) noexcept { wave_amplitude_ = (a >= 0) ? a : 0; }



    void draw(CanvasBase& cvs) {

        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<BatteryGasGauge>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::BatteryGasGauge, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        const rgba on_accent = st.on_accent.a ? st.on_accent : font;


        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);



        const int nub_w = (r.w >= 28) ? (r.w / 12) : 4;

        const int nub_h = (r.h >= 10) ? (r.h / 3) : (r.h / 2);

        const int body_w = r.w - nub_w - 2;

        const int body_h = r.h - 2;

        const int body_x = r.x + 1;

        const int body_y = r.y + 1;

        if (body_w <= 2 || body_h <= 2) return;



        draw_rect(cvs, body_x, body_y, body_w, body_h, border, false);



        const int nub_x = body_x + body_w;

        const int nub_y = r.y + (r.h - nub_h) / 2;

        if (nub_w > 0 && nub_h > 0) {

            draw_rect(cvs, nub_x, nub_y, nub_w, nub_h, border, true);

        }



        const int inner_x = body_x + 2;

        const int inner_y = body_y + 2;

        const int inner_w = body_w - 4;

        const int inner_h = body_h - 4;

        if (inner_w <= 0 || inner_h <= 0) return;



        const float ratio = alg::arc::ratio_from_range(value_, 0, 100);

        const int fill_h = static_cast<int>(inner_h * ratio);

        if (fill_h <= 0) return;



        const rgba fill = accent;
        if (mode_ == StyleMode::NixieTube) {
            draw_rect(cvs, inner_x, inner_y + (inner_h - fill_h), inner_w, fill_h, fill, true);
            if (status_ == Status::Charging) {
                draw_bolt(cvs, inner_x, inner_y, inner_w, inner_h, on_accent);
            }
            return;
        }


        if (anim_enabled_) {

            wave_phase_ += wave_speed_;

            if (wave_phase_ > 6.2831853f) wave_phase_ -= 6.2831853f;

            if (wave_phase_ < 0.0f) wave_phase_ += 6.2831853f;

        }



        const int wave_base = inner_y + (inner_h - fill_h);

        for (int x = 0; x < inner_w; ++x) {

            const float fx = static_cast<float>(x) / static_cast<float>(inner_w);

            const float wave = std::sin(fx * 6.2831853f + wave_phase_) * static_cast<float>(wave_amplitude_);

            int top = wave_base + static_cast<int>(wave);

            if (top < inner_y) top = inner_y;

            if (top > inner_y + inner_h) top = inner_y + inner_h;

            draw_rect(cvs, inner_x + x, top, 1, inner_y + inner_h - top, fill, true);

        }



        if (status_ == Status::Charging) {
            draw_bolt(cvs, inner_x, inner_y, inner_w, inner_h, on_accent);
        }
    }


private:

    int value_{60};

    Status status_{Status::Idle};

    StyleMode mode_{StyleMode::Liquid};

    bool anim_enabled_{true};

    float wave_speed_{0.2f};

    float wave_phase_{0.0f};

    int wave_amplitude_{2};



    static void draw_bolt(CanvasBase& cvs, int x, int y, int w, int h, const rgba& col) {

        const int cx = x + w / 2;

        const int top = y + h / 4;

        const int mid = y + h / 2;

        const int bot = y + h * 3 / 4;

        draw_line(cvs, cx - 4, top, cx + 2, mid, col);

        draw_line(cvs, cx + 2, mid, cx - 2, mid, col);

        draw_line(cvs, cx - 2, mid, cx + 4, bot, col);

    }

};









