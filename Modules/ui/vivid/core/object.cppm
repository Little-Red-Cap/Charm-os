module;
#include <array>
#include <cstddef>
#include <cstdint>
export module charm.core.object;

export import charm.core.geometry;
export import charm.core.handle;
export import charm.gfx.canvas;
export import charm.core.event;

export
class ObjectBase {
public:
    struct VTable {
        void (*draw)(ObjectBase&, CanvasBase&) noexcept;
        bool (*on_event)(ObjectBase&, const Event&) noexcept;
        Rect (*layout_rect)(const ObjectBase&) noexcept;
        Rect (*paint_bounds)(const ObjectBase&) noexcept;
        Rect (*children_clip_rect)(const ObjectBase&) noexcept;
        bool (*should_draw_child)(const ObjectBase&, const ObjectBase&) noexcept;
    };

    ObjectBase() noexcept { vtable_ = &default_vtable(); }
    ~ObjectBase() = default;

    Rect get_rect() const noexcept { return rect_; }
    void set_pos(int x, int y) noexcept { rect_.x = x; rect_.y = y; }
    void set_size(int w, int h) noexcept {
        rect_.w = (w < 0) ? 0 : w;
        rect_.h = (h < 0) ? 0 : h;
    }
    void set_rect(Rect r) noexcept { rect_ = rect_normalized(r); }
    Rect layout_rect() const noexcept { return vtable_->layout_rect(*this); }
    Rect paint_bounds() const noexcept { return vtable_->paint_bounds(*this); }

    enum class ClipPolicy : unsigned {
        None,
        Rect,
        LayoutRect,
        Custom
    };

    enum class CachePolicy : unsigned {
        None,
        Subtree
    };

    void set_clip_policy(ClipPolicy policy) noexcept { clip_policy_ = policy; }
    ClipPolicy clip_policy() const noexcept { return clip_policy_; }
    Rect children_clip_rect() const noexcept { return vtable_->children_clip_rect(*this); }
    void set_cache_policy(CachePolicy policy) noexcept { cache_policy_ = policy; }
    CachePolicy cache_policy() const noexcept { return cache_policy_; }
    void mark_cache_dirty() noexcept { cache_dirty_ = true; }
    bool cache_dirty() const noexcept { return cache_dirty_; }
    void clear_cache_dirty() noexcept { cache_dirty_ = false; }

    void set_visible(bool v) noexcept {
        visible_ = v;
        if (!v) {
            set_state(State::Hovered, false);
            set_state(State::Pressed, false);
        }
    }
    bool is_visible() const noexcept { return visible_; }

    enum class State : unsigned {
        None      = 0,
        Hovered   = 1 << 0,
        Pressed   = 1 << 1,
        Focused   = 1 << 2,
        Disabled  = 1 << 3
    };

    enum class LayoutMode : unsigned {
        Anchor,
        Flex,
        Flow,
        Grid,
        Constraint,
        Custom
    };

    struct LayoutSpec {
        LayoutMode kind{LayoutMode::Anchor};
        int flow{0};
        int main_align{0};
        int cross_align{0};
        int gap{0};
        int padding{0};
        int line_gap{0};
        int columns{1};
        int cell_w{0};
        int cell_h{0};
        int grid_gap{0};
        int grid_padding{0};
        int custom_id{0};
        int custom_param0{0};
        int custom_param1{0};
        int custom_param2{0};
        int custom_param3{0};
    };

    void set_state(State s, bool on) noexcept {
        if (on) state_ = static_cast<State>(static_cast<unsigned>(state_) | static_cast<unsigned>(s));
        else    state_ = static_cast<State>(static_cast<unsigned>(state_) & ~static_cast<unsigned>(s));
    }

    bool has_state(State s) const noexcept {
        return (static_cast<unsigned>(state_) & static_cast<unsigned>(s)) != 0;
    }

    void set_enabled(bool on) noexcept {
        set_state(State::Disabled, !on);
        if (!on) {
            set_state(State::Hovered, false);
            set_state(State::Pressed, false);
        }
    }
    bool is_enabled() const noexcept { return !has_state(State::Disabled); }

    void set_focusable(bool on) noexcept { focusable_ = on; }
    bool is_focusable() const noexcept { return focusable_; }

    void set_style_variant(std::uint8_t v) noexcept { style_variant_ = v; }
    std::uint8_t style_variant() const noexcept { return style_variant_; }

    void set_flex_layout(int flow, int main_align, int cross_align, int gap, int padding) noexcept {
        flex_enabled_ = true;
        layout_mode_ = LayoutMode::Flex;
        flex_flow_ = flow;
        flex_main_align_ = main_align;
        flex_cross_align_ = cross_align;
        flex_gap_ = gap;
        flex_padding_ = padding;
        layout_spec_enabled_ = true;
        layout_spec_ = {};
        layout_spec_.kind = LayoutMode::Flex;
        layout_spec_.flow = flow;
        layout_spec_.main_align = main_align;
        layout_spec_.cross_align = cross_align;
        layout_spec_.gap = gap;
        layout_spec_.padding = padding;
    }

    void set_flex_grow(int grow) noexcept { flex_grow_ = (grow > 0) ? grow : 0; }
    int flex_grow() const noexcept { return flex_grow_; }

    void set_flow_layout(int gap, int line_gap, int padding) noexcept {
        layout_mode_ = LayoutMode::Flow;
        flow_gap_ = gap;
        flow_line_gap_ = line_gap;
        flow_padding_ = padding;
        layout_spec_enabled_ = true;
        layout_spec_ = {};
        layout_spec_.kind = LayoutMode::Flow;
        layout_spec_.gap = gap;
        layout_spec_.line_gap = line_gap;
        layout_spec_.padding = padding;
    }

    int flow_gap() const noexcept { return flow_gap_; }
    int flow_line_gap() const noexcept { return flow_line_gap_; }
    int flow_padding() const noexcept { return flow_padding_; }

    void set_grid_layout(int columns, int cell_w, int cell_h, int gap, int padding) noexcept {
        layout_mode_ = LayoutMode::Grid;
        grid_cols_ = (columns > 0) ? columns : 1;
        grid_cell_w_ = cell_w;
        grid_cell_h_ = cell_h;
        grid_gap_ = gap;
        grid_padding_ = padding;
        layout_spec_enabled_ = true;
        layout_spec_ = {};
        layout_spec_.kind = LayoutMode::Grid;
        layout_spec_.columns = grid_cols_;
        layout_spec_.cell_w = cell_w;
        layout_spec_.cell_h = cell_h;
        layout_spec_.grid_gap = gap;
        layout_spec_.grid_padding = padding;
    }

    int grid_columns() const noexcept { return grid_cols_; }
    int grid_cell_width() const noexcept { return grid_cell_w_; }
    int grid_cell_height() const noexcept { return grid_cell_h_; }
    int grid_gap() const noexcept { return grid_gap_; }
    int grid_padding() const noexcept { return grid_padding_; }

    void set_constraint_layout(int padding = 0) noexcept {
        layout_mode_ = LayoutMode::Constraint;
        layout_spec_enabled_ = true;
        layout_spec_ = {};
        layout_spec_.kind = LayoutMode::Constraint;
        layout_spec_.padding = padding;
    }

    void set_custom_layout(int custom_id,
                           int p0 = 0,
                           int p1 = 0,
                           int p2 = 0,
                           int p3 = 0) noexcept {
        layout_mode_ = LayoutMode::Custom;
        layout_spec_enabled_ = true;
        layout_spec_ = {};
        layout_spec_.kind = LayoutMode::Custom;
        layout_spec_.custom_id = custom_id;
        layout_spec_.custom_param0 = p0;
        layout_spec_.custom_param1 = p1;
        layout_spec_.custom_param2 = p2;
        layout_spec_.custom_param3 = p3;
    }

    void set_layout_spec(const LayoutSpec& spec) noexcept {
        layout_spec_enabled_ = true;
        layout_spec_ = spec;
        layout_mode_ = spec.kind;
    }

    void clear_layout_spec() noexcept {
        layout_spec_enabled_ = false;
        layout_spec_ = {};
        layout_mode_ = LayoutMode::Anchor;
    }
    bool has_layout_spec() const noexcept { return layout_spec_enabled_; }
    const LayoutSpec& layout_spec() const noexcept { return layout_spec_; }

    void set_anchor(int left, int top, int right, int bottom) noexcept {
        anchor_enabled_ = true;
        anchor_left_ = left;
        anchor_top_ = top;
        anchor_right_ = right;
        anchor_bottom_ = bottom;
    }

    void clear_anchor() noexcept { anchor_enabled_ = false; }
    bool has_anchor() const noexcept { return anchor_enabled_; }
    int anchor_left() const noexcept { return anchor_left_; }
    int anchor_top() const noexcept { return anchor_top_; }
    int anchor_right() const noexcept { return anchor_right_; }
    int anchor_bottom() const noexcept { return anchor_bottom_; }

    void set_percent_size(int w_percent, int h_percent) noexcept {
        percent_w_ = w_percent;
        percent_h_ = h_percent;
    }

    void clear_percent_size() noexcept { percent_w_ = -1; percent_h_ = -1; }
    int percent_width() const noexcept { return percent_w_; }
    int percent_height() const noexcept { return percent_h_; }
    bool has_percent_size() const noexcept { return percent_w_ >= 0 || percent_h_ >= 0; }

    void set_min_size(int w, int h) noexcept { min_w_ = w; min_h_ = h; }
    void clear_min_size() noexcept { min_w_ = 0; min_h_ = 0; }
    int min_width() const noexcept { return min_w_; }
    int min_height() const noexcept { return min_h_; }

    void set_max_size(int w, int h) noexcept { max_w_ = w; max_h_ = h; }
    void clear_max_size() noexcept { max_w_ = 0; max_h_ = 0; }
    int max_width() const noexcept { return max_w_; }
    int max_height() const noexcept { return max_h_; }

    void set_align(int h, int v) noexcept { align_h_ = h; align_v_ = v; }
    int align_h() const noexcept { return align_h_; }
    int align_v() const noexcept { return align_v_; }

    bool has_flex_layout() const noexcept { return flex_enabled_; }
    int flex_flow() const noexcept { return flex_flow_; }
    int flex_main_align() const noexcept { return flex_main_align_; }
    int flex_cross_align() const noexcept { return flex_cross_align_; }
    int flex_gap() const noexcept { return flex_gap_; }
    int flex_padding() const noexcept { return flex_padding_; }
    LayoutMode layout_mode() const noexcept { return layout_mode_; }

    void set_parent(WidgetHandle parent) noexcept { parent_ = parent; }
    WidgetHandle parent() const noexcept { return parent_; }

    void draw(CanvasBase& cvs) { vtable_->draw(*this, cvs); }

    bool on_event(const Event& e) { return vtable_->on_event(*this, e); }

    bool should_draw_child(const ObjectBase& ch) const noexcept {
        return vtable_->should_draw_child(*this, ch);
    }
    void set_children_bounds(const Rect& bounds, bool valid) noexcept {
        children_bounds_ = rect_normalized(bounds);
        children_bounds_valid_ = valid;
    }
    bool has_children_bounds() const noexcept { return children_bounds_valid_; }
    Rect children_bounds() const noexcept { return children_bounds_; }

    bool take_dirty_hint(Rect& out) noexcept {
        if (!dirty_hint_valid_) return false;
        out = dirty_hint_;
        dirty_hint_valid_ = false;
        return true;
    }

protected:
    Rect rect_{};
    Rect children_bounds_{};
    bool children_bounds_valid_{false};
    bool visible_{true};
    State state_{State::None};
    bool focusable_{false};
    std::uint8_t style_variant_{0};
    bool flex_enabled_{false};
    int flex_flow_{0};
    int flex_main_align_{0};
    int flex_cross_align_{0};
    int flex_gap_{0};
    int flex_padding_{0};
    int flex_grow_{0};
    LayoutMode layout_mode_{LayoutMode::Anchor};
    int flow_gap_{0};
    int flow_line_gap_{0};
    int flow_padding_{0};
    int grid_cols_{1};
    int grid_cell_w_{0};
    int grid_cell_h_{0};
    int grid_gap_{0};
    int grid_padding_{0};
    bool anchor_enabled_{false};
    int anchor_left_{0};
    int anchor_top_{0};
    int anchor_right_{0};
    int anchor_bottom_{0};
    int percent_w_{-1};
    int percent_h_{-1};
    int min_w_{0};
    int min_h_{0};
    int max_w_{0};
    int max_h_{0};
    int align_h_{0};
    int align_v_{0};
    bool layout_spec_enabled_{false};
    LayoutSpec layout_spec_{};
    ClipPolicy clip_policy_{ClipPolicy::None};
    CachePolicy cache_policy_{CachePolicy::None};
    bool cache_dirty_{false};
    WidgetHandle parent_{};
    Rect dirty_hint_{};
    bool dirty_hint_valid_{false};
    const VTable* vtable_{nullptr};

    void mark_dirty_hint(const Rect& r) noexcept {
        const Rect nr = rect_normalized(r);
        if (!rect_valid(nr)) return;
        cache_dirty_ = true;
        if (!dirty_hint_valid_) {
            dirty_hint_ = nr;
            dirty_hint_valid_ = true;
            return;
        }
        const int left = (nr.x < dirty_hint_.x) ? nr.x : dirty_hint_.x;
        const int top = (nr.y < dirty_hint_.y) ? nr.y : dirty_hint_.y;
        const int right = ((nr.x + nr.w) > (dirty_hint_.x + dirty_hint_.w)) ? (nr.x + nr.w) : (dirty_hint_.x + dirty_hint_.w);
        const int bottom = ((nr.y + nr.h) > (dirty_hint_.y + dirty_hint_.h)) ? (nr.y + nr.h) : (dirty_hint_.y + dirty_hint_.h);
        dirty_hint_.x = left;
        dirty_hint_.y = top;
        dirty_hint_.w = right - left;
        dirty_hint_.h = bottom - top;
    }

    template<typename Derived>
    void init_vtable() noexcept {
        vtable_ = &vtable_for<Derived>();
    }

private:
    static Rect default_layout_rect(const ObjectBase& self) noexcept { return self.rect_; }
    static Rect default_paint_bounds(const ObjectBase& self) noexcept { return self.rect_; }
    static Rect default_children_clip_rect(const ObjectBase& self) noexcept { return self.rect_; }
    static bool default_on_event(ObjectBase&, const Event&) noexcept { return false; }
    static bool default_should_draw_child(const ObjectBase&, const ObjectBase&) noexcept { return true; }
    static void default_draw(ObjectBase&, CanvasBase&) noexcept {}

    static const VTable& default_vtable() noexcept {
        static const VTable table{
            &default_draw,
            &default_on_event,
            &default_layout_rect,
            &default_paint_bounds,
            &default_children_clip_rect,
            &default_should_draw_child
        };
        return table;
    }

template<typename Derived>
static constexpr bool overrides_layout_rect() noexcept {
    return &Derived::layout_rect != &ObjectBase::layout_rect;
}

template<typename Derived>
static constexpr bool overrides_paint_bounds() noexcept {
    return &Derived::paint_bounds != &ObjectBase::paint_bounds;
}

template<typename Derived>
static constexpr bool overrides_children_clip_rect() noexcept {
    return &Derived::children_clip_rect != &ObjectBase::children_clip_rect;
}

    template<typename Derived>
    static constexpr bool overrides_should_draw_child() noexcept {
        return &Derived::should_draw_child != &ObjectBase::should_draw_child;
    }

    template<typename Derived>
    static constexpr bool overrides_on_event() noexcept {
        return &Derived::on_event != &ObjectBase::on_event;
    }

template<typename Derived>
static Rect layout_rect_thunk(const ObjectBase& self) noexcept {
    if constexpr (overrides_layout_rect<Derived>()) {
        return static_cast<const Derived&>(self).layout_rect();
    }
    return default_layout_rect(self);
}

template<typename Derived>
static Rect paint_bounds_thunk(const ObjectBase& self) noexcept {
    if constexpr (overrides_paint_bounds<Derived>()) {
        return static_cast<const Derived&>(self).paint_bounds();
    }
    return default_paint_bounds(self);
}

template<typename Derived>
static Rect children_clip_rect_thunk(const ObjectBase& self) noexcept {
    if constexpr (overrides_children_clip_rect<Derived>()) {
        return static_cast<const Derived&>(self).children_clip_rect();
    }
    return default_children_clip_rect(self);
}

    template<typename Derived>
    static bool should_draw_child_thunk(const ObjectBase& self, const ObjectBase& child) noexcept {
        if constexpr (overrides_should_draw_child<Derived>()) {
            return static_cast<const Derived&>(self).should_draw_child(child);
        }
        return default_should_draw_child(self, child);
    }

    template<typename Derived>
    static bool on_event_thunk(ObjectBase& self, const Event& e) noexcept {
        if constexpr (overrides_on_event<Derived>()) {
            return static_cast<Derived&>(self).on_event(e);
        }
        return default_on_event(self, e);
    }

    template<typename Derived>
    static void draw_thunk(ObjectBase& self, CanvasBase& cvs) noexcept {
        static_cast<Derived&>(self).draw(cvs);
    }

    template<typename Derived>
    static const VTable& vtable_for() noexcept {
        static const VTable table{
            &draw_thunk<Derived>,
            &on_event_thunk<Derived>,
            &layout_rect_thunk<Derived>,
            &paint_bounds_thunk<Derived>,
            &children_clip_rect_thunk<Derived>,
            &should_draw_child_thunk<Derived>
        };
        return table;
    }
};

static_assert(sizeof(ObjectBase) <= 288,
              "ObjectBase must not regain resident child or interaction storage");

namespace vivid_object_detail {
    template<std::size_t Capacity>
    struct ChildStorage {
        std::array<WidgetHandle, Capacity> children{};
        std::size_t count{0};
    };

    template<>
    struct ChildStorage<0> {};
}

export
template<typename Derived, std::size_t ChildCapacity = 0>
class WidgetBase : public ObjectBase,
                   private vivid_object_detail::ChildStorage<ChildCapacity> {
public:
    static constexpr std::size_t child_capacity = ChildCapacity;

    WidgetBase() {
        init_vtable<Derived>();
    }

    bool add_child(WidgetHandle child) noexcept
        requires (ChildCapacity > 0) {
        auto& count = child_size();
        if (count >= ChildCapacity) return false;
        child_handles()[count++] = child;
        return true;
    }

    void clear_children() noexcept
        requires (ChildCapacity > 0) {
        auto& count = child_size();
        for (std::size_t i = 0; i < count; ++i) {
            child_handles()[i] = {};
        }
        count = 0;
    }

    bool remove_child(WidgetHandle child) noexcept
        requires (ChildCapacity > 0) {
        auto& count = child_size();
        const auto index = child_index(child);
        if (index >= count) return false;
        for (std::size_t i = index + 1; i < count; ++i) {
            child_handles()[i - 1] = child_handles()[i];
        }
        child_handles()[count - 1] = {};
        --count;
        return true;
    }

    bool insert_child_before(WidgetHandle child, WidgetHandle before) noexcept
        requires (ChildCapacity > 0) {
        const auto index = child_index(before);
        return insert_child_at(child, index);
    }

    bool insert_child_after(WidgetHandle child, WidgetHandle after) noexcept
        requires (ChildCapacity > 0) {
        const auto index = child_index(after);
        const auto count = child_size();
        return insert_child_at(child, index < count ? index + 1 : index);
    }

    bool move_child_to_front(WidgetHandle child) noexcept
        requires (ChildCapacity > 0) {
        const auto count = child_size();
        if (count <= 1) return true;
        const auto index = child_index(child);
        if (index >= count || index + 1 == count) return true;
        const auto value = child_handles()[index];
        for (std::size_t i = index + 1; i < count; ++i) {
            child_handles()[i - 1] = child_handles()[i];
        }
        child_handles()[count - 1] = value;
        return true;
    }

    bool move_child_to_back(WidgetHandle child) noexcept
        requires (ChildCapacity > 0) {
        const auto count = child_size();
        if (count <= 1) return true;
        const auto index = child_index(child);
        if (index >= count || index == 0) return true;
        const auto value = child_handles()[index];
        for (std::size_t i = index; i > 0; --i) {
            child_handles()[i] = child_handles()[i - 1];
        }
        child_handles()[0] = value;
        return true;
    }

    [[nodiscard]] std::size_t child_count() const noexcept
        requires (ChildCapacity > 0) {
        return child_size();
    }

    [[nodiscard]] WidgetHandle child_at(std::size_t index) const noexcept
        requires (ChildCapacity > 0) {
        return index < child_size() ? child_handles()[index] : WidgetHandle{};
    }

    [[nodiscard]] bool has_child(WidgetHandle child) const noexcept
        requires (ChildCapacity > 0) {
        return child_index(child) < child_size();
    }

    [[nodiscard]] std::size_t child_index(WidgetHandle child) const noexcept
        requires (ChildCapacity > 0) {
        const auto count = child_size();
        for (std::size_t i = 0; i < count; ++i) {
            if (child_handles()[i] == child) return i;
        }
        return count;
    }

private:
    bool insert_child_at(WidgetHandle child, std::size_t index) noexcept
        requires (ChildCapacity > 0) {
        auto& count = child_size();
        if (count >= ChildCapacity) return false;
        if (index >= count) return add_child(child);
        for (std::size_t i = count; i > index; --i) {
            child_handles()[i] = child_handles()[i - 1];
        }
        child_handles()[index] = child;
        ++count;
        return true;
    }

    using ChildStorage = vivid_object_detail::ChildStorage<ChildCapacity>;

    [[nodiscard]] ChildStorage& child_storage() noexcept {
        return static_cast<ChildStorage&>(*this);
    }

    [[nodiscard]] const ChildStorage& child_storage() const noexcept {
        return static_cast<const ChildStorage&>(*this);
    }

    [[nodiscard]] auto& child_handles() noexcept
        requires (ChildCapacity > 0) {
        return child_storage().children;
    }

    [[nodiscard]] const auto& child_handles() const noexcept
        requires (ChildCapacity > 0) {
        return child_storage().children;
    }

    [[nodiscard]] std::size_t& child_size() noexcept
        requires (ChildCapacity > 0) {
        return child_storage().count;
    }

    [[nodiscard]] const std::size_t& child_size() const noexcept
        requires (ChildCapacity > 0) {
        return child_storage().count;
    }
};

namespace {
    struct WidgetBaseLeafLayoutProbe final : WidgetBase<WidgetBaseLeafLayoutProbe> {};
    static_assert(sizeof(WidgetBaseLeafLayoutProbe) == sizeof(ObjectBase),
                  "zero-capacity WidgetBase must not add resident child storage");
}

export
constexpr ObjectBase::State operator|(ObjectBase::State a, ObjectBase::State b) noexcept {
    return static_cast<ObjectBase::State>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

