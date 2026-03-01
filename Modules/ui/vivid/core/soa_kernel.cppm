module;
#include <array>
#include <cstddef>
#include <cstdint>

export module charm.core.soa_kernel;

export import charm.core.handle;
export import charm.core.geometry;
export import charm.core.config;

namespace {
    constexpr std::uint16_t kInvalidIndex = 0xFFFF;
}

#ifdef CHARM_VIVID_SOA_MAX_NODES
export constexpr std::size_t soa_max_nodes = CHARM_VIVID_SOA_MAX_NODES;
#else
export constexpr std::size_t soa_max_nodes = 256;
#endif

export
enum class SoaNodeFlag : std::uint8_t {
    Used = 1 << 0,
    Visible = 1 << 1,
    Enabled = 1 << 2,
    Focusable = 1 << 3,
    HitTest = 1 << 4,
    ClipChildren = 1 << 5
};

export
enum class SoaStateFlag : std::uint8_t {
    Hovered = 1 << 0,
    Pressed = 1 << 1,
    Focused = 1 << 2
};

export
class SoaKernel {
public:
    static constexpr std::size_t kMaxNodes = soa_max_nodes;

    SoaKernel() noexcept {
        free_head_ = 0;
        for (std::uint16_t i = 0; i < kMaxNodes; ++i) {
            free_next_[i] = (i + 1 < kMaxNodes) ? static_cast<std::uint16_t>(i + 1) : kInvalidIndex;
            kind_[i] = WidgetKind::None;
            generation_[i] = 1;
            flags_[i] = 0;
            state_flags_[i] = 0;
            variant_[i] = 0;
            rects_[i] = Rect{};
            paint_bounds_[i] = Rect{};
            parent_[i] = kInvalidIndex;
            first_child_[i] = kInvalidIndex;
            last_child_[i] = kInvalidIndex;
            next_sibling_[i] = kInvalidIndex;
            prev_sibling_[i] = kInvalidIndex;
            child_count_[i] = 0;
            text_[i] = nullptr;
            value_[i] = 0;
            min_value_[i] = 0;
            max_value_[i] = 100;
            checked_[i] = 0;
        }
    }

    WidgetHandle create(WidgetKind kind) noexcept {
        if (kind == WidgetKind::None) return {};
        if (free_head_ == kInvalidIndex) return {};
        const std::uint16_t idx = free_head_;
        free_head_ = free_next_[idx];
        kind_[idx] = kind;
        flags_[idx] = static_cast<std::uint8_t>(SoaNodeFlag::Used)
            | static_cast<std::uint8_t>(SoaNodeFlag::Visible)
            | static_cast<std::uint8_t>(SoaNodeFlag::Enabled)
            | static_cast<std::uint8_t>(SoaNodeFlag::HitTest);
        state_flags_[idx] = 0;
        variant_[idx] = 0;
        rects_[idx] = Rect{};
        paint_bounds_[idx] = Rect{};
        parent_[idx] = kInvalidIndex;
        first_child_[idx] = kInvalidIndex;
        last_child_[idx] = kInvalidIndex;
        next_sibling_[idx] = kInvalidIndex;
        prev_sibling_[idx] = kInvalidIndex;
        child_count_[idx] = 0;
        text_[idx] = nullptr;
        value_[idx] = 0;
        min_value_[idx] = 0;
        max_value_[idx] = 100;
        checked_[idx] = 0;
        return WidgetHandle{kind, idx, generation_[idx]};
    }

    void destroy(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        detach_from_parent(idx);
        detach_children(idx);
        kind_[idx] = WidgetKind::None;
        flags_[idx] = 0;
        state_flags_[idx] = 0;
        variant_[idx] = 0;
        rects_[idx] = Rect{};
        paint_bounds_[idx] = Rect{};
        text_[idx] = nullptr;
        value_[idx] = 0;
        min_value_[idx] = 0;
        max_value_[idx] = 100;
        checked_[idx] = 0;
        generation_[idx] = static_cast<std::uint16_t>(generation_[idx] + 1);
        free_next_[idx] = free_head_;
        free_head_ = idx;
    }

    bool valid(WidgetHandle h) const noexcept {
        return index_of(h) != kInvalidIndex;
    }

    WidgetKind kind(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? WidgetKind::None : kind_[idx];
    }

    Rect rect(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? Rect{} : rects_[idx];
    }

    void set_rect(WidgetHandle h, const Rect& r) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        rects_[idx] = r;
        if (!rect_valid(paint_bounds_[idx])) {
            paint_bounds_[idx] = r;
        }
    }

    Rect paint_bounds(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? Rect{} : paint_bounds_[idx];
    }

    void set_paint_bounds(WidgetHandle h, const Rect& r) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        paint_bounds_[idx] = r;
    }

    bool link(WidgetHandle parent, WidgetHandle child) noexcept {
        const std::uint16_t p = index_of(parent);
        const std::uint16_t c = index_of(child);
        if (p == kInvalidIndex || c == kInvalidIndex) return false;
        if (p == c) return false;
        if (creates_cycle(p, c)) return false;
        detach_from_parent(c);
        parent_[c] = p;
        if (last_child_[p] != kInvalidIndex) {
            const std::uint16_t last = last_child_[p];
            next_sibling_[last] = c;
            prev_sibling_[c] = last;
            last_child_[p] = c;
        } else {
            first_child_[p] = c;
            last_child_[p] = c;
            prev_sibling_[c] = kInvalidIndex;
        }
        next_sibling_[c] = kInvalidIndex;
        child_count_[p] = static_cast<std::uint16_t>(child_count_[p] + 1);
        return true;
    }

    bool unlink(WidgetHandle parent, WidgetHandle child) noexcept {
        const std::uint16_t p = index_of(parent);
        const std::uint16_t c = index_of(child);
        if (p == kInvalidIndex || c == kInvalidIndex) return false;
        if (parent_[c] != p) return false;
        detach_from_parent(c);
        return true;
    }

    WidgetHandle parent(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(parent_[idx]);
    }

    WidgetHandle first_child(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(first_child_[idx]);
    }

    WidgetHandle last_child(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(last_child_[idx]);
    }

    WidgetHandle next_sibling(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(next_sibling_[idx]);
    }

    WidgetHandle prev_sibling(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(prev_sibling_[idx]);
    }

    std::size_t child_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? 0 : child_count_[idx];
    }

    void set_visible(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::Visible, on);
    }

    bool visible(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::Visible);
    }

    void set_enabled(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::Enabled, on);
    }

    bool enabled(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::Enabled);
    }

    void set_focusable(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::Focusable, on);
    }

    bool focusable(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::Focusable);
    }

    void set_hit_testable(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::HitTest, on);
    }

    bool hit_testable(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::HitTest);
    }

    void set_clip_children(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::ClipChildren, on);
    }

    bool clip_children(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::ClipChildren);
    }

    void set_hovered(WidgetHandle h, bool on) noexcept {
        set_state_flag(h, SoaStateFlag::Hovered, on);
    }

    void set_pressed(WidgetHandle h, bool on) noexcept {
        set_state_flag(h, SoaStateFlag::Pressed, on);
    }

    void set_focused(WidgetHandle h, bool on) noexcept {
        set_state_flag(h, SoaStateFlag::Focused, on);
    }

    bool hovered(WidgetHandle h) const noexcept {
        return get_state_flag(h, SoaStateFlag::Hovered);
    }

    bool pressed(WidgetHandle h) const noexcept {
        return get_state_flag(h, SoaStateFlag::Pressed);
    }

    bool focused(WidgetHandle h) const noexcept {
        return get_state_flag(h, SoaStateFlag::Focused);
    }

    void set_variant(WidgetHandle h, std::uint8_t variant) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        variant_[idx] = variant;
    }

    std::uint8_t variant(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? 0 : variant_[idx];
    }

    void set_text(WidgetHandle h, const char* text) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        text_[idx] = text;
    }

    const char* text(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? nullptr : text_[idx];
    }

    void set_value(WidgetHandle h, int value) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        value_[idx] = value;
    }

    int value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? 0 : value_[idx];
    }

    void set_range(WidgetHandle h, int min_value, int max_value) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        min_value_[idx] = min_value;
        max_value_[idx] = max_value;
        if (value_[idx] < min_value) value_[idx] = min_value;
        if (value_[idx] > max_value) value_[idx] = max_value;
    }

    int min_value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? 0 : min_value_[idx];
    }

    int max_value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? 0 : max_value_[idx];
    }

    void set_checked(WidgetHandle h, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        checked_[idx] = static_cast<std::uint8_t>(on ? 1 : 0);
    }

    bool checked(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx != kInvalidIndex) && checked_[idx] != 0;
    }

private:
    std::array<WidgetKind, kMaxNodes> kind_{};
    std::array<std::uint16_t, kMaxNodes> generation_{};
    std::array<std::uint16_t, kMaxNodes> free_next_{};
    std::array<std::uint16_t, kMaxNodes> parent_{};
    std::array<std::uint16_t, kMaxNodes> first_child_{};
    std::array<std::uint16_t, kMaxNodes> last_child_{};
    std::array<std::uint16_t, kMaxNodes> next_sibling_{};
    std::array<std::uint16_t, kMaxNodes> prev_sibling_{};
    std::array<std::uint16_t, kMaxNodes> child_count_{};
    std::array<std::uint8_t, kMaxNodes> flags_{};
    std::array<std::uint8_t, kMaxNodes> state_flags_{};
    std::array<std::uint8_t, kMaxNodes> variant_{};
    std::array<Rect, kMaxNodes> rects_{};
    std::array<Rect, kMaxNodes> paint_bounds_{};
    std::array<const char*, kMaxNodes> text_{};
    std::array<int, kMaxNodes> value_{};
    std::array<int, kMaxNodes> min_value_{};
    std::array<int, kMaxNodes> max_value_{};
    std::array<std::uint8_t, kMaxNodes> checked_{};
    std::uint16_t free_head_{kInvalidIndex};

    std::uint16_t index_of(WidgetHandle h) const noexcept {
        const std::uint16_t idx = h.index;
        if (idx >= kMaxNodes) return kInvalidIndex;
        if (kind_[idx] != h.kind) return kInvalidIndex;
        if (generation_[idx] != h.generation) return kInvalidIndex;
        if (!flag_raw(idx, SoaNodeFlag::Used)) return kInvalidIndex;
        return idx;
    }

    WidgetHandle handle_from_index(std::uint16_t idx) const noexcept {
        if (idx == kInvalidIndex || idx >= kMaxNodes) return {};
        if (!flag_raw(idx, SoaNodeFlag::Used)) return {};
        return WidgetHandle{kind_[idx], idx, generation_[idx]};
    }

    void detach_from_parent(std::uint16_t idx) noexcept {
        const std::uint16_t p = parent_[idx];
        if (p == kInvalidIndex) return;
        const std::uint16_t prev = prev_sibling_[idx];
        const std::uint16_t next = next_sibling_[idx];
        if (prev != kInvalidIndex) {
            next_sibling_[prev] = next;
        } else {
            first_child_[p] = next;
        }
        if (next != kInvalidIndex) {
            prev_sibling_[next] = prev;
        } else {
            last_child_[p] = prev;
        }
        parent_[idx] = kInvalidIndex;
        prev_sibling_[idx] = kInvalidIndex;
        next_sibling_[idx] = kInvalidIndex;
        if (child_count_[p] > 0) {
            child_count_[p] = static_cast<std::uint16_t>(child_count_[p] - 1);
        }
    }

    void detach_children(std::uint16_t idx) noexcept {
        std::uint16_t child = first_child_[idx];
        while (child != kInvalidIndex) {
            parent_[child] = kInvalidIndex;
            const std::uint16_t next = next_sibling_[child];
            prev_sibling_[child] = kInvalidIndex;
            next_sibling_[child] = kInvalidIndex;
            child = next;
        }
        first_child_[idx] = kInvalidIndex;
        last_child_[idx] = kInvalidIndex;
        child_count_[idx] = 0;
    }

    bool creates_cycle(std::uint16_t parent, std::uint16_t child) const noexcept {
        std::uint16_t p = parent;
        while (p != kInvalidIndex) {
            if (p == child) return true;
            p = parent_[p];
        }
        return false;
    }

    bool flag_raw(std::uint16_t idx, SoaNodeFlag flag) const noexcept {
        return (flags_[idx] & static_cast<std::uint8_t>(flag)) != 0;
    }

    bool get_flag(WidgetHandle h, SoaNodeFlag flag) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        return flag_raw(idx, flag);
    }

    void set_flag(WidgetHandle h, SoaNodeFlag flag, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const std::uint8_t mask = static_cast<std::uint8_t>(flag);
        if (on) {
            flags_[idx] |= mask;
        } else {
            flags_[idx] = static_cast<std::uint8_t>(flags_[idx] & ~mask);
        }
    }

    bool get_state_flag(WidgetHandle h, SoaStateFlag flag) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        return (state_flags_[idx] & static_cast<std::uint8_t>(flag)) != 0;
    }

    void set_state_flag(WidgetHandle h, SoaStateFlag flag, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const std::uint8_t mask = static_cast<std::uint8_t>(flag);
        if (on) {
            state_flags_[idx] |= mask;
        } else {
            state_flags_[idx] = static_cast<std::uint8_t>(state_flags_[idx] & ~mask);
        }
    }
};

export
class SoaFactory {
public:
    explicit SoaFactory(SoaKernel& kernel) noexcept : kernel_(kernel) {}

    WidgetHandle create_container() noexcept { return kernel_.create(WidgetKind::Container); }
    WidgetHandle create_label(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::Label);
        kernel_.set_text(h, text);
        kernel_.set_hit_testable(h, false);
        return h;
    }
    WidgetHandle create_button(const char* text) noexcept {
        auto h = kernel_.create(WidgetKind::Button);
        kernel_.set_text(h, text);
        return h;
    }
    WidgetHandle create_switch() noexcept {
        auto h = kernel_.create(WidgetKind::Switch);
        return h;
    }
    WidgetHandle create_slider() noexcept {
        auto h = kernel_.create(WidgetKind::Slider);
        return h;
    }
    WidgetHandle create_progress() noexcept {
        auto h = kernel_.create(WidgetKind::Progress);
        kernel_.set_hit_testable(h, false);
        return h;
    }

    bool link(WidgetHandle parent, WidgetHandle child) noexcept {
        return kernel_.link(parent, child);
    }

    SoaKernel& kernel() noexcept { return kernel_; }
    const SoaKernel& kernel() const noexcept { return kernel_; }

private:
    SoaKernel& kernel_;
};
