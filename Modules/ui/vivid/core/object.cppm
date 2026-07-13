module;
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
export module charm.core.object;

export import charm.core.geometry;
export import charm.core.handle;

export
class ObjectBase {
public:
    ObjectBase() = default;
    ~ObjectBase() = default;

    Rect get_rect() const noexcept { return rect_; }
    void set_pos(int x, int y) noexcept { rect_.x = x; rect_.y = y; }
    void set_size(int w, int h) noexcept {
        rect_.w = (w < 0) ? 0 : w;
        rect_.h = (h < 0) ? 0 : h;
    }
    void set_rect(Rect r) noexcept { rect_ = rect_normalized(r); }

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

protected:
    Rect rect_{};
    bool visible_{true};
    State state_{State::None};
    bool focusable_{false};
    std::uint8_t style_variant_{0};
};

static_assert(sizeof(ObjectBase) <= 32,
              "ObjectBase must remain a non-owning geometry and state base");
static_assert(!std::is_polymorphic_v<ObjectBase>,
              "ObjectBase must not regain implicit or manual dynamic dispatch");
static_assert(std::is_trivially_copyable_v<ObjectBase>,
              "ObjectBase must remain a zero-overhead state value");

namespace vivid_object_detail {
    template<std::size_t Capacity>
    struct ChildStorage {
        std::array<WidgetHandle, Capacity> children{};
        std::size_t count{0};
    };

    template<>
    struct ChildStorage<0> {};

    template<>
    struct ChildStorage<std::dynamic_extent> {
        ChildStorage() = default;
        ChildStorage(const ChildStorage&) = delete;
        ChildStorage& operator=(const ChildStorage&) = delete;
        ChildStorage(ChildStorage&&) = delete;
        ChildStorage& operator=(ChildStorage&&) = delete;
        ~ChildStorage() {
            for (std::size_t i = 0; i < count; ++i) children[i] = {};
        }

        std::span<WidgetHandle> children{};
        std::size_t count{0};
    };
}

export
template<typename Derived, std::size_t ChildCapacity = 0>
class WidgetBase : public ObjectBase,
                   private vivid_object_detail::ChildStorage<ChildCapacity> {
public:
    WidgetBase() = default;

    void attach_child_storage(std::span<WidgetHandle> storage) noexcept
        requires (ChildCapacity == std::dynamic_extent) {
        clear_children();
        for (auto& child : storage) child = {};
        child_storage().children = storage;
    }

    void detach_child_storage() noexcept
        requires (ChildCapacity == std::dynamic_extent) {
        clear_children();
        child_storage().children = {};
    }

    [[nodiscard]] std::size_t child_storage_capacity() const noexcept
        requires (ChildCapacity > 0) {
        if constexpr (ChildCapacity == std::dynamic_extent) {
            return child_handles().size();
        } else {
            return ChildCapacity;
        }
    }

    bool add_child(WidgetHandle child) noexcept
        requires (ChildCapacity > 0) {
        auto& count = child_size();
        if (count >= child_storage_capacity()) return false;
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
        if (count >= child_storage_capacity()) return false;
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

    struct WidgetBaseExternalStorageProbe final
        : WidgetBase<WidgetBaseExternalStorageProbe, std::dynamic_extent> {};
    static_assert(sizeof(WidgetBaseExternalStorageProbe)
                  <= sizeof(ObjectBase) + sizeof(std::span<WidgetHandle>)
                      + sizeof(std::size_t) + alignof(std::span<WidgetHandle>),
                  "external child storage must remain a non-owning span and count");
    static_assert(!std::is_copy_constructible_v<WidgetBaseExternalStorageProbe>);
    static_assert(!std::is_move_constructible_v<WidgetBaseExternalStorageProbe>);
}

export
constexpr ObjectBase::State operator|(ObjectBase::State a, ObjectBase::State b) noexcept {
    return static_cast<ObjectBase::State>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

