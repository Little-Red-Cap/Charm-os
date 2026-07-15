module;
#include <algorithm>
#include <cstddef>
#include <span>
#include <type_traits>
export module charm.widgets.segmented_control;

import charm.core.object;
import charm.core.event;
import service.state;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.text_box;

using namespace ui::render;

export
class SegmentedControl : public WidgetBase<SegmentedControl> {
public:
    using selected_state_type = service::state<int, 4>;
    using selected_slot_type = typename selected_state_type::slot_type;
    using selected_connection = typename selected_state_type::connection;
    static constexpr std::size_t max_items = 8;

    SegmentedControl() {
        set_focusable(true);
        set_size(220, 28);
    }

    SegmentedControl(const SegmentedControl&) = delete;
    SegmentedControl& operator=(const SegmentedControl&) = delete;
    SegmentedControl(SegmentedControl&&) = delete;
    SegmentedControl& operator=(SegmentedControl&&) = delete;

    [[nodiscard]] bool set_items(std::span<const char* const> items) noexcept {
        if (items.size() > max_items) return false;
        labels_ = items;
        const int count = item_count();
        if (selected_.get() >= count) {
            apply_selected(count > 0 ? count - 1 : 0, false);
        }
        return true;
    }

    void set_selected(int index) noexcept {
        if (index < 0 || index >= item_count()) return;
        apply_selected(index, true);
    }

    [[nodiscard]] int selected() const noexcept { return selected_.get(); }
    [[nodiscard]] int item_count() const noexcept {
        return static_cast<int>(labels_.size());
    }

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
        const Style& base = Theme::instance().get<SegmentedControl>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::SegmentedControl, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.metrics.corner_radius, bg, true);
        draw_round_rect(cvs, r.x, r.y, r.w, r.h, st.metrics.corner_radius, border, false);

        const int count = item_count();
        if (count <= 0) return;

        const int seg_w = r.w / count;
        for (int i = 0; i < count; ++i) {
            Rect seg{r.x + i * seg_w, r.y, seg_w, r.h};
            if (i == count - 1) {
                seg.w = r.x + r.w - seg.x;
            }

            if (i == selected()) {
                const int radius = (i == 0 || i == count - 1) ? st.metrics.corner_radius : 0;
                if (radius > 0) {
                    draw_round_rect(cvs, seg.x, seg.y, seg.w, seg.h, radius, accent, true);
                } else {
                    draw_rect(cvs, seg.x, seg.y, seg.w, seg.h, accent, true);
                }
            }
            if (i > 0) {
                draw_line(cvs, seg.x, seg.y + 2, seg.x, seg.y + seg.h - 3, border);
            }

            const char* label = labels_[static_cast<std::size_t>(i)]
                ? labels_[static_cast<std::size_t>(i)] : "";
            draw_text_box(cvs, seg, label, font, resolve_font(st),
                          TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
        }

        draw_focus_ring(cvs, r, st, has_state(State::Focused), 0, st.metrics.corner_radius);
    }

    bool on_event(const Event& e) {
        if (!is_enabled()) return false;
        const auto r = get_rect();
        if (e.type == Event::Type::Click) {
            const int count = item_count();
            if (!r.contains(e.x, e.y) || count <= 0) return false;
            const int idx = (e.x - r.x) * count / std::max(1, static_cast<int>(r.w));
            set_selected(idx);
            return true;
        } else if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Left && selected() > 0) {
                set_selected(selected() - 1);
                return true;
            }
            if (e.key_code == Event::Key::Right && selected() + 1 < item_count()) {
                set_selected(selected() + 1);
                return true;
            }
        }
        return false;
    }

private:
    void apply_selected(int index, bool notify_callback) noexcept {
        if (selected_.set(index) && notify_callback && on_change_) {
            on_change_();
        }
    }

    std::span<const char* const> labels_{};
    selected_state_type selected_{0};
    Callback on_change_{};
};

static_assert(sizeof(SegmentedControl)
              <= sizeof(ObjectBase) + sizeof(std::span<const char* const>)
                  + sizeof(SegmentedControl::selected_state_type) + sizeof(Callback)
                  + alignof(std::span<const char* const>),
              "SegmentedControl must not regain a fixed label pointer table");
static_assert(!std::is_copy_constructible_v<SegmentedControl>);
static_assert(!std::is_move_constructible_v<SegmentedControl>);




