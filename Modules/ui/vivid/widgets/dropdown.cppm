module;
#include <cstddef>
#include <span>
#include <type_traits>
export module charm.widgets.dropdown;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.event;
import service.state;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.label;

using namespace ui::render;

// Simple dropdown control (data + selection only).
export
class Dropdown : public WidgetBase<Dropdown> {
public:
    using selected_state_type = service::state<int, 4>;
    using selected_slot_type = typename selected_state_type::slot_type;
    using selected_connection = typename selected_state_type::connection;

    Dropdown() {
        set_focusable(true);
        set_size(160, 28);
    }

    ~Dropdown() noexcept {
        release_option_storage();
    }

    Dropdown(const Dropdown&) = delete;
    Dropdown& operator=(const Dropdown&) = delete;
    Dropdown(Dropdown&&) = delete;
    Dropdown& operator=(Dropdown&&) = delete;

    void attach_option_storage(std::span<const char*> storage) noexcept {
        detach_option_storage();
        for (auto& option : storage) option = nullptr;
        options_ = storage;
    }

    void detach_option_storage() noexcept {
        release_option_storage();
        (void)selected_.set(0);
    }

    [[nodiscard]] std::size_t option_storage_capacity() const noexcept {
        return options_.size();
    }

    [[nodiscard]] bool add_option(const char* txt) noexcept {
        if (static_cast<std::size_t>(option_count_) >= options_.size()) return false;
        options_[static_cast<std::size_t>(option_count_)] = txt ? txt : "";
        ++option_count_;
        return true;
    }

    void set_selected(int idx) noexcept {
        if (idx < 0 || idx >= option_count_) return;
        (void)selected_.set(idx);
        // Legacy on_change is a selection command callback, not a pure state-change signal.
        // Keep firing it for any valid selection request, even when the truth cell stays the same.
        if (on_change_) on_change_();
    }

    [[nodiscard]] int selected() const noexcept {
        const int value = selected_.get();
        return option_count_ > 0 && value >= 0 && value < option_count_ ? value : 0;
    }
    [[nodiscard]] int option_count() const noexcept { return option_count_; }
    const char* option_text(int idx) const noexcept {
        if (idx < 0 || idx >= option_count_) return nullptr;
        return options_[static_cast<std::size_t>(idx)];
    }

    void set_on_change(Callback cb) noexcept { on_change_ = cb; }
    void set_on_open(Callback cb) noexcept { on_open_ = cb; }

    // observe_selected() keeps the same-domain synchronous rules of service::state.
    [[nodiscard]] auto observe_selected(selected_slot_type slot) noexcept {
        return selected_.connect(slot);
    }

    [[nodiscard]] bool unobserve_selected(selected_connection c) noexcept {
        return selected_.disconnect(c);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<Dropdown>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Dropdown, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int current = selected();
        const char* text = option_count_ > 0 ? options_[static_cast<std::size_t>(current)] : "";
        Label lbl{text};
        lbl.set_color(font);
        lbl.set_font(resolve_font(st));
        const int baseline_y = r.y + (r.h - lbl.line_height()) / 2 + lbl.baseline();
        lbl.set_baseline_pos(r.x + st.metrics.padding, baseline_y);
        lbl.draw(cvs);

        // simple arrow
        const int ax = r.x + r.w - st.metrics.padding - 6;
        const int ay = r.y + r.h / 2 - 3;
        draw_line(cvs, ax, ay, ax + 6, ay + 6, border);
        draw_line(cvs, ax + 6, ay + 6, ax + 12, ay, border);
    }

    bool on_event(const Event& e) {
        if (!is_enabled()) return false;
        if (e.type == Event::Type::Click) {
            if (get_rect().contains(e.x, e.y) || has_state(State::Focused)) {
                if (on_open_) { on_open_(); return true; }
                return cycle(1);
            }
        } else if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Enter || e.key_code == Event::Key::Space) {
                if (on_open_) { on_open_(); return true; }
                return cycle(1);
            } else if (e.key_code == Event::Key::Down) {
                return cycle(1);
            } else if (e.key_code == Event::Key::Up) {
                return cycle(-1);
            }
        }
        return false;
    }

private:
    void release_option_storage() noexcept {
        clear_options();
        options_ = {};
    }

    void clear_options() noexcept {
        for (int i = 0; i < option_count_; ++i) {
            options_[static_cast<std::size_t>(i)] = nullptr;
        }
        option_count_ = 0;
    }

    [[nodiscard]] bool cycle(int delta) noexcept {
        if (option_count_ == 0) return false;
        set_selected((selected() + delta + option_count_) % option_count_);
        return true;
    }

    std::span<const char*> options_{};
    int option_count_{0};
    selected_state_type selected_{0};
    Callback on_change_{};
    Callback on_open_{};
};

static_assert(sizeof(Dropdown)
              <= sizeof(ObjectBase) + sizeof(std::span<const char*>) + sizeof(int)
                  + sizeof(Dropdown::selected_state_type) + sizeof(Callback) * 2
                  + alignof(std::span<const char*>),
              "Dropdown must not regain a fixed option pointer table");
static_assert(!std::is_copy_constructible_v<Dropdown>);
static_assert(!std::is_move_constructible_v<Dropdown>);




