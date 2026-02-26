module;
#include <algorithm>
export module charm.widgets.segmented_control;

import charm.core.object;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.text;

using namespace ui::render;

export
class SegmentedControl : public ObjectBase {
public:
    SegmentedControl() {
        set_focusable(true);
        set_size(220, 28);
    }

    void set_items(const char* const* items, int count) noexcept {
        count_ = (count > kMax) ? kMax : (count < 0 ? 0 : count);
        for (int i = 0; i < count_; ++i) labels_[i] = items[i];
        for (int i = count_; i < kMax; ++i) labels_[i] = nullptr;
        if (selected_ >= count_) selected_ = (count_ > 0) ? (count_ - 1) : 0;
    }

    void set_item(int index, const char* label) noexcept {
        if (index < 0 || index >= kMax) return;
        if (index >= count_) count_ = index + 1;
        labels_[index] = label;
    }

    void set_selected(int index) noexcept {
        if (index < 0 || index >= count_) return;
        if (selected_ == index) return;
        selected_ = index;
        if (on_change_) on_change_();
    }

    int selected() const noexcept { return selected_; }

    void set_on_change(Callback cb) noexcept { on_change_ = cb; }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<SegmentedControl>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::SegmentedControl, state, st);
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, bg, true);
        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.corner_radius, border, false);

        if (count_ <= 0) return;

        const int seg_w = (count_ > 0) ? (r.w / count_) : r.w;
        for (int i = 0; i < count_; ++i) {
            Rect seg{r.x + i * seg_w, r.y, seg_w, r.h};
            if (i == count_ - 1) {
                seg.w = r.x + r.w - seg.x;
            }

            if (i == selected_) {
                const int radius = (i == 0 || i == count_ - 1) ? st.corner_radius : 0;
                if (radius > 0) {
                    draw_round_rect(cvs, seg.x, seg.y, seg.w, seg.h, radius, accent, true);
                } else {
                    draw_rect(cvs, seg.x, seg.y, seg.w, seg.h, accent, true);
                }
            }
            if (i > 0) {
                draw_line(cvs, seg.x, seg.y + 2, seg.x, seg.y + seg.h - 3, border);
            }

            const char* label = labels_[i] ? labels_[i] : "";
            draw_text_box(cvs, seg, label, font, resolve_font(st),
                          TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        }

        draw_focus_ring(cvs, r, st, has_state(State::Focused), 0, st.corner_radius);
    }

    bool on_event(const Event& e) override {
        if (!is_enabled()) return false;
        const auto r = get_rect();
        if (e.type == Event::Type::Click) {
            if (!r.contains(e.x, e.y) || count_ <= 0) return false;
            const int idx = (e.x - r.x) * count_ / std::max(1, r.w);
            set_selected(idx);
            return true;
        } else if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Left && selected_ > 0) {
                set_selected(selected_ - 1);
                return true;
            }
            if (e.key_code == Event::Key::Right && selected_ + 1 < count_) {
                set_selected(selected_ + 1);
                return true;
            }
        }
        return false;
    }

private:
    static constexpr int kMax = 8;
    const char* labels_[kMax]{};
    int count_{0};
    int selected_{0};
    Callback on_change_{};
};


