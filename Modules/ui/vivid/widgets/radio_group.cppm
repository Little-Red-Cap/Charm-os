module;
#include <cstddef>
#include <span>
#include <type_traits>
export module charm.widgets.radio_group;

import charm.core.object;
import charm.gfx.render_style;
import charm.widgets.radio;

// Simple mutual-exclusion group. Use resolver to avoid factory dependency.
// Logic-only: no style/visual rendering.
export
class RadioGroup : public WidgetBase<RadioGroup, std::dynamic_extent> {
public:
    RadioGroup() {
        set_focusable(false);
        set_size(0, 0);
    }

    bool add(WidgetHandle h) noexcept {
        return add_child(h);
    }

    template<typename Resolver>
    void set_checked(Resolver&& res, WidgetHandle h) noexcept {
        const auto count = child_count();
        for (std::size_t i = 0; i < count; ++i) {
            const auto radio = child_at(i);
            auto* r = res(radio);
            if (!r) continue;
            const bool on = (radio == h);
            if (r->checked() != on) r->set_checked(on);
        }
    }

    template<typename Factory>
    requires requires(Factory& f, WidgetHandle h) { f.get_radio(h); }
    void set_checked(Factory& f, WidgetHandle h) noexcept {
        set_checked([&](WidgetHandle wh) { return f.get_radio(wh); }, h);
    }

    void draw(CanvasBase&) {}
};

static_assert(sizeof(RadioGroup)
              <= sizeof(ObjectBase) + sizeof(std::span<WidgetHandle>)
                  + sizeof(std::size_t) + alignof(std::span<WidgetHandle>),
              "RadioGroup must not regain a fixed radio handle table");
static_assert(!std::is_copy_constructible_v<RadioGroup>);
static_assert(!std::is_move_constructible_v<RadioGroup>);


