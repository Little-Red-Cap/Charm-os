module;
#include <cstddef>
export module charm.widgets.radio_group;

import charm.core.object;
import charm.gfx.render;
import charm.widgets.radio;

// Simple mutual-exclusion group. Use resolver to avoid factory dependency.
export
class RadioGroup : public ObjectBase {
public:
    static constexpr std::size_t kMax = 16;

    RadioGroup() {
        set_focusable(false);
        set_size(0, 0);
    }

    bool add(WidgetHandle h) noexcept {
        if (count_ >= kMax) return false;
        radios_[count_++] = h;
        return true;
    }

    template<typename Resolver>
    void set_checked(Resolver&& res, WidgetHandle h) noexcept {
        for (std::size_t i = 0; i < count_; ++i) {
            auto* r = res(radios_[i]);
            if (!r) continue;
            const bool on = (radios_[i] == h);
            if (r->checked() != on) r->set_checked(on);
        }
    }

    template<typename Factory>
    requires requires(Factory& f, WidgetHandle h) { f.get_radio(h); }
    void set_checked(Factory& f, WidgetHandle h) noexcept {
        set_checked([&](WidgetHandle wh) { return f.get_radio(wh); }, h);
    }

    void draw(CanvasBase&) override {}

private:
    WidgetHandle radios_[kMax]{};
    std::size_t count_{0};
};
