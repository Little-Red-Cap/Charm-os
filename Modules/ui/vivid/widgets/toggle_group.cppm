module;
#include <algorithm>
#include <cstddef>
#include <span>
#include <type_traits>
export module charm.widgets.toggle_group;

import charm.core.object;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.text_box;

using namespace ui::render;

export
class ToggleGroup : public WidgetBase<ToggleGroup> {
public:
    struct Item {
        const char* label{nullptr};
        bool checked{false};
    };

    static constexpr std::size_t max_items = 8;

    ToggleGroup() {
        set_focusable(true);
        set_size(220, 28);
    }

    ToggleGroup(const ToggleGroup&) = delete;
    ToggleGroup& operator=(const ToggleGroup&) = delete;
    ToggleGroup(ToggleGroup&&) = delete;
    ToggleGroup& operator=(ToggleGroup&&) = delete;

    [[nodiscard]] bool set_items(std::span<Item> items) noexcept {
        if (items.size() > max_items) return false;
        items_ = items;
        const int count = item_count();
        if (focus_idx_ >= count) focus_idx_ = count > 0 ? count - 1 : 0;
        return true;
    }

    [[nodiscard]] bool set_item(int index, const char* label) noexcept {
        if (index < 0 || index >= item_count()) return false;
        items_[static_cast<std::size_t>(index)].label = label;
        return true;
    }

    void set_single_select(bool on) noexcept { single_select_ = on; }

    void set_checked(int index, bool on) noexcept {
        const int count = item_count();
        if (index < 0 || index >= count) return;
        if (single_select_ && on) {
            for (auto& item : items_) item.checked = false;
        }
        items_[static_cast<std::size_t>(index)].checked = on;
        if (on_change_) on_change_();
    }

    bool is_checked(int index) const noexcept {
        if (index < 0 || index >= item_count()) return false;
        return items_[static_cast<std::size_t>(index)].checked;
    }

    [[nodiscard]] int item_count() const noexcept {
        return static_cast<int>(items_.size());
    }

    void set_on_change(Callback cb) noexcept { on_change_ = cb; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused));
        const Style& base = Theme::instance().get<ToggleGroup>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::ToggleGroup, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int count = item_count();
        if (count <= 0) return;

        const int seg_w = r.w / count;
        for (int i = 0; i < count; ++i) {
            Rect seg{r.x + i * seg_w, r.y, seg_w, r.h};
            if (i == count - 1) {
                seg.w = r.x + r.w - seg.x;
            }

            const auto& item = items_[static_cast<std::size_t>(i)];
            rgba seg_bg = item.checked ? accent : bg;
            rgba seg_border = item.checked ? accent : border;
            draw_rect(cvs, seg.x, seg.y, seg.w, seg.h, seg_bg, true);
            draw_rect(cvs, seg.x, seg.y, seg.w, seg.h, seg_border, false);
            if (i > 0) {
                draw_line(cvs, seg.x, seg.y + 2, seg.x, seg.y + seg.h - 3, border);
            }

            const char* label = item.label ? item.label : "";
            draw_text_box(cvs, seg, label, font, resolve_font(st),
                          TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);

            if (has_state(State::Focused) && i == focus_idx_) {
                draw_focus_ring(cvs, seg, st, true);
            }
        }
    }

    bool on_event(const Event& e) {
        if (!is_enabled()) return false;
        const auto r = get_rect();
        if (e.type == Event::Type::Click) {
            const int count = item_count();
            if (!r.contains(e.x, e.y) || count <= 0) return false;
            const int idx = (e.x - r.x) * count / std::max(1, static_cast<int>(r.w));
            focus_idx_ = idx;
            if (single_select_) {
                set_checked(idx, true);
            } else {
                set_checked(idx, !is_checked(idx));
            }
            return true;
        } else if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Left && focus_idx_ > 0) {
                --focus_idx_;
                return true;
            }
            if (e.key_code == Event::Key::Right && focus_idx_ + 1 < item_count()) {
                ++focus_idx_;
                return true;
            }
            if (e.key_code == Event::Key::Enter || e.key_code == Event::Key::Space) {
                if (focus_idx_ < 0 || focus_idx_ >= item_count()) return false;
                if (single_select_) {
                    set_checked(focus_idx_, true);
                } else {
                    set_checked(focus_idx_, !is_checked(focus_idx_));
                }
                return true;
            }
        }
        return false;
    }

private:
    std::span<Item> items_{};
    int focus_idx_{0};
    bool single_select_{false};
    Callback on_change_{};
};

static_assert(std::is_trivially_copyable_v<ToggleGroup::Item>);
static_assert(sizeof(ToggleGroup::Item)
              <= sizeof(const char*) + sizeof(bool) + alignof(const char*),
              "ToggleGroup item must remain a label pointer and checked bit");
static_assert(sizeof(ToggleGroup)
              <= sizeof(ObjectBase) + sizeof(std::span<ToggleGroup::Item>)
                  + sizeof(int) + sizeof(bool) + sizeof(Callback)
                  + alignof(std::span<ToggleGroup::Item>),
              "ToggleGroup must not regain fixed label or checked tables");
static_assert(!std::is_copy_constructible_v<ToggleGroup>);
static_assert(!std::is_move_constructible_v<ToggleGroup>);




