module;
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
export module charm.widgets.stepper;

import charm.core.object;
import service.state;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.gfx.text_box;
import charm.font.typography;

using namespace ui::render;

export
class Stepper : public WidgetBase<Stepper> {
public:
    static constexpr std::size_t kMaxSteps = 8;
    using current_state_type = service::state<int, 1>;
    using current_slot_type = typename current_state_type::slot_type;
    using current_connection = typename current_state_type::connection;

    Stepper() {
        set_size(240, 48);
        set_focusable(false);
    }

    ~Stepper() noexcept {
        release_label_storage();
    }

    Stepper(const Stepper&) = delete;
    Stepper& operator=(const Stepper&) = delete;
    Stepper(Stepper&&) = delete;
    Stepper& operator=(Stepper&&) = delete;

    [[nodiscard]] bool attach_label_storage(std::span<const char*> labels,
                                            std::span<std::uint8_t> sizes) noexcept {
        if (labels.size() != sizes.size() || labels.size() > kMaxSteps) return false;
        detach_label_storage();
        for (auto& label : labels) label = nullptr;
        for (auto& size : sizes) size = 0;
        labels_ = labels.data();
        label_sizes_ = sizes.data();
        label_capacity_ = static_cast<std::uint8_t>(labels.size());
        return true;
    }

    void detach_label_storage() noexcept {
        release_label_storage();
        (void)current_.set(0);
    }

    [[nodiscard]] std::size_t label_storage_capacity() const noexcept {
        return label_capacity_;
    }

    [[nodiscard]] bool set_steps(int count) noexcept {
        if (count < 1 || count > label_capacity_) return false;
        count_ = static_cast<std::uint8_t>(count);
        if (current() >= count_) (void)current_.set(count_ - 1);
        return true;
    }

    void set_current(int index) noexcept {
        if (count_ <= 0) return;
        if (index < 0) index = 0;
        if (index >= count_) index = count_ - 1;
        (void)current_.set(index);
    }

    [[nodiscard]] int current() const noexcept {
        const int value = current_.get();
        return count_ > 0 && value >= 0 && value < count_ ? value : 0;
    }

    [[nodiscard]] bool set_label(int index, const char* text) noexcept {
        if (index < 0 || index >= label_capacity_) return false;
        const char* label = text ? text : "";
        labels_[index] = label;
        label_sizes_[index] = bounded_text_size(label);
        return true;
    }

    // observe_current() keeps the same-domain synchronous rules of service::state.
    [[nodiscard]] auto observe_current(current_slot_type slot) noexcept {
        return current_.connect(slot);
    }

    [[nodiscard]] bool unobserve_current(current_connection c) noexcept {
        return current_.disconnect(c);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused));
        const Style& base = Theme::instance().get<Stepper>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::Stepper, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);
        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        if (count_ <= 0) return;
        const int left = r.x + st.metrics.padding;
        const int right = r.x + r.w - st.metrics.padding;
        const int center_y = r.y + r.h / 2;
        const int span = right - left;
        const int radius = (r.h / 2) - st.metrics.padding;
        const int draw_r = (radius > 2) ? radius : 2;
        if (count_ > 1) {
            draw_line(cvs, left, center_y, right, center_y, border);
        }

        const Font& ft = resolve_font(st);
        const int label_y = center_y + draw_r + 2;
        for (int i = 0; i < count_; ++i) {
            const int cx = (count_ == 1)
                ? (left + right) / 2
                : left + (span * i) / (count_ - 1);
            const bool done = i < current();
            const bool is_current = i == current();
            const rgba fill = is_current ? accent : (done ? border : bg);
            draw_circle(cvs, cx, center_y, draw_r, fill, true);
            draw_circle(cvs, cx, center_y, draw_r, is_current ? accent : border, false);

            if (label_sizes_[i] > 0) {
                const char* text = labels_[i];
                const int text_w = measure_text_width(text, label_sizes_[i], ft);
                const int tx = cx - text_w / 2;
                draw_text_baseline_range(cvs, tx, label_y + ft.baseline,
                                         text, label_sizes_[i], font, ft);
            }
        }
    }

private:
    static constexpr std::uint8_t kMaxLabelBytes = 16;

    void release_label_storage() noexcept {
        for (std::uint8_t i = 0; i < label_capacity_; ++i) {
            labels_[i] = nullptr;
            label_sizes_[i] = 0;
        }
        labels_ = nullptr;
        label_sizes_ = nullptr;
        label_capacity_ = 0;
        count_ = 0;
    }

    static std::uint8_t bounded_text_size(const char* text) noexcept {
        std::uint8_t size = 0;
        while (size < kMaxLabelBytes && text[size] != '\0') ++size;
        return size;
    }

    const char** labels_{nullptr};
    std::uint8_t* label_sizes_{nullptr};
    current_state_type current_{0};
    std::uint8_t label_capacity_{0};
    std::uint8_t count_{0};
};

static_assert(sizeof(Stepper)
              <= sizeof(ObjectBase) + sizeof(void*) * 2 + sizeof(Stepper::current_state_type)
                   + sizeof(std::uint8_t) * 2 + alignof(Stepper) * 3,
              "Stepper must not regain a fixed label pointer or length table");
static_assert(!std::is_copy_constructible_v<Stepper>);
static_assert(!std::is_move_constructible_v<Stepper>);




