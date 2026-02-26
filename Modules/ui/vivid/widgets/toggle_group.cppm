module;
#include <algorithm>
export module charm.widgets.toggle_group;

import charm.core.object;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.text;

using namespace ui::render;

export
class ToggleGroup : public ObjectBase {
public:
    ToggleGroup() {
        set_focusable(true);
        set_size(220, 28);
    }

    void set_items(const char* const* items, int count) noexcept {
        count_ = (count > kMax) ? kMax : (count < 0 ? 0 : count);
        for (int i = 0; i < count_; ++i) labels_[i] = items[i];
        for (int i = count_; i < kMax; ++i) labels_[i] = nullptr;
        if (focus_idx_ >= count_) focus_idx_ = (count_ > 0) ? (count_ - 1) : 0;
    }

    void set_item(int index, const char* label) noexcept {
        if (index < 0 || index >= kMax) return;
        if (index >= count_) count_ = index + 1;
        labels_[index] = label;
    }

    void set_single_select(bool on) noexcept { single_select_ = on; }

    void set_checked(int index, bool on) noexcept {
        if (index < 0 || index >= count_) return;
        if (single_select_ && on) {
            for (int i = 0; i < count_; ++i) checked_[i] = false;
        }
        checked_[index] = on;
        if (on_change_) on_change_();
    }

    bool is_checked(int index) const noexcept {
        if (index < 0 || index >= count_) return false;
        return checked_[index];
    }

    void set_on_change(Callback cb) noexcept { on_change_ = cb; }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<ToggleGroup>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::ToggleGroup, state, st);
        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        if (count_ <= 0) return;

        const int seg_w = (count_ > 0) ? (r.w / count_) : r.w;
        for (int i = 0; i < count_; ++i) {
            Rect seg{r.x + i * seg_w, r.y, seg_w, r.h};
            if (i == count_ - 1) {
                seg.w = r.x + r.w - seg.x;
            }

            rgba seg_bg = checked_[i] ? st.bg_pressed : bg;
            rgba seg_border = checked_[i] ? st.border_pressed : border;
            draw_rect(cvs, seg.x, seg.y, seg.w, seg.h, seg_bg, true);
            draw_rect(cvs, seg.x, seg.y, seg.w, seg.h, seg_border, false);
            if (i > 0) {
                draw_line(cvs, seg.x, seg.y + 2, seg.x, seg.y + seg.h - 3, border);
            }

            const char* label = labels_[i] ? labels_[i] : "";
            draw_text_box(cvs, seg, label, font, resolve_font(st),
                          TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);

            if (has_state(State::Focused) && i == focus_idx_) {
                draw_focus_ring(cvs, seg, st, true);
            }
        }
    }

    bool on_event(const Event& e) override {
        if (!is_enabled()) return false;
        const auto r = get_rect();
        if (e.type == Event::Type::Click) {
            if (!r.contains(e.x, e.y) || count_ <= 0) return false;
            const int idx = (e.x - r.x) * count_ / std::max(1, r.w);
            focus_idx_ = idx;
            if (single_select_) {
                set_checked(idx, true);
            } else {
                set_checked(idx, !checked_[idx]);
            }
            return true;
        } else if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Left && focus_idx_ > 0) {
                --focus_idx_;
                return true;
            }
            if (e.key_code == Event::Key::Right && focus_idx_ + 1 < count_) {
                ++focus_idx_;
                return true;
            }
            if (e.key_code == Event::Key::Enter || e.key_code == Event::Key::Space) {
                if (focus_idx_ >= 0 && focus_idx_ < count_) {
                    if (single_select_) {
                        set_checked(focus_idx_, true);
                    } else {
                        set_checked(focus_idx_, !checked_[focus_idx_]);
                    }
                }
                return true;
            }
        }
        return false;
    }

private:
    static constexpr int kMax = 8;
    const char* labels_[kMax]{};
    bool checked_[kMax]{};
    int count_{0};
    int focus_idx_{0};
    bool single_select_{false};
    Callback on_change_{};
};


