module;
#include <cstdint>
export module charm.widgets.roller;

import charm.core.object;
import charm.core.event;
import service.state;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.gfx.text_box;

using namespace ui::render;

export
class Roller : public WidgetBase<Roller> {
public:
    using selected_state_type = service::state<int, 4>;
    using selected_slot_type = typename selected_state_type::slot_type;
    using selected_connection = typename selected_state_type::connection;

    Roller() {
        set_size(140, 80);
        set_focusable(true);
        add_option("Item 1");
        add_option("Item 2");
        add_option("Item 3");
    }

    void add_option(const char* txt) noexcept {
        if (option_count_ >= max_options) return;
        const char* text = txt ? txt : "";
        options_[option_count_] = text;
        option_sizes_[option_count_] = bounded_text_size(text);
        ++option_count_;
    }

    void set_selected(int idx) noexcept {
        if (idx < 0 || idx >= option_count_) return;
        (void)selected_.set(idx);
        // Legacy on_change is a roller command callback, not a pure state-change signal.
        if (on_change_) on_change_();
    }

    [[nodiscard]] int selected() const noexcept { return selected_.get(); }

    void set_on_change(Callback cb) noexcept { on_change_ = cb; }

    // observe_selected() keeps the same-domain synchronous rules of service::state.
    [[nodiscard]] auto observe_selected(selected_slot_type slot) noexcept {
        return selected_.connect(slot);
    }

    [[nodiscard]] bool unobserve_selected(selected_connection c) noexcept {
        return selected_.disconnect(c);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Roller>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Roller, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);

        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.metrics.corner_radius, bg, true);
        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.metrics.corner_radius, border, false);

        const Font& ft = resolve_font(st);
        const int line_h = ft.line_height;
        const int center_y = r.y + r.h / 2;
        const int visible = 2; // above and below
        for (int i = -visible; i <= visible; ++i) {
            const int idx = wrap_index(selected() + i);
            if (idx < 0) continue;
            const int alpha_step = 60;
            const int dist = (i < 0) ? -i : i;
            int alpha = 255 - dist * alpha_step;
            if (alpha < 60) alpha = 60;
            rgba col = font;
            col.a = static_cast<std::uint8_t>(alpha);
            const int baseline_y = center_y + i * line_h + ft.baseline - line_h / 2;
            draw_text_baseline_range(cvs, r.x + st.metrics.padding, baseline_y,
                                     options_[idx], option_sizes_[idx], col, ft);
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
    static constexpr std::uint8_t kMaxOptionBytes = 32;

    static std::uint8_t bounded_text_size(const char* text) noexcept {
        std::uint8_t size = 0;
        while (size < kMaxOptionBytes && text[size] != '\0') ++size;
        return size;
    }

    void step(int delta) {
        if (option_count_ == 0) return;
        set_selected(wrap_index(selected() + delta));
    }

    int wrap_index(int idx) const noexcept {
        if (option_count_ == 0) return -1;
        idx %= option_count_;
        if (idx < 0) idx += option_count_;
        return idx;
    }

    static constexpr int max_options = 16;
    const char* options_[max_options]{};
    std::uint8_t option_sizes_[max_options]{};
    int option_count_{0};
    selected_state_type selected_{0};
    Callback on_change_{};
};

static_assert(sizeof(Roller)
              <= sizeof(ObjectBase) + sizeof(const char*) * 16 + sizeof(std::uint8_t) * 16
                   + sizeof(int) + sizeof(Roller::selected_state_type) + sizeof(Callback)
                   + alignof(Roller) * 3,
              "Roller must not regain per-option inline text storage");





