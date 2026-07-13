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

    Rect children_clip_rect() const noexcept { return vtable_->children_clip_rect(*this); }

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

    void set_parent(WidgetHandle parent) noexcept { parent_ = parent; }
    WidgetHandle parent() const noexcept { return parent_; }

    void draw(CanvasBase& cvs) { vtable_->draw(*this, cvs); }

    bool on_event(const Event& e) { return vtable_->on_event(*this, e); }

    bool should_draw_child(const ObjectBase& ch) const noexcept {
        return vtable_->should_draw_child(*this, ch);
    }
protected:
    Rect rect_{};
    bool visible_{true};
    State state_{State::None};
    bool focusable_{false};
    std::uint8_t style_variant_{0};
    WidgetHandle parent_{};
    const VTable* vtable_{nullptr};

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

static_assert(sizeof(ObjectBase) <= 48,
              "ObjectBase must not regain legacy layout, invalidation, or optional runtime storage");

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
    struct WidgetBaseLeafStorageProbe final : WidgetBase<WidgetBaseLeafStorageProbe> {};
    static_assert(sizeof(WidgetBaseLeafStorageProbe) == sizeof(ObjectBase),
                  "zero-capacity WidgetBase must not add resident child storage");
}

export
constexpr ObjectBase::State operator|(ObjectBase::State a, ObjectBase::State b) noexcept {
    return static_cast<ObjectBase::State>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

