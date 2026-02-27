#include <cstdint>
#include <cstdio>

import charm.core.factory;
import charm.core.input_router;
import charm.core.input_adapter;
import charm.core.handle;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.theme_preset;
import charm.widgets.button;
import input.raw_event;

using input::RawInputEvent;
using input::RawInputEventType;

namespace {
    RawInputEvent make_button_event(input::Button b, bool pressed, std::uint32_t ms) noexcept {
        RawInputEvent ev{};
        ev.type = RawInputEventType::Button;
        ev.ms = ms;
        ev.button = b;
        ev.pressed = pressed;
        return ev;
    }
}

int main() {
    ThemeTokens tokens{};
    tokens.surface = {244, 244, 248, 255};
    tokens.surface_variant = {230, 232, 238, 255};
    tokens.on_surface = {32, 32, 38, 255};
    tokens.on_surface_muted = {120, 120, 128, 255};
    tokens.outline = {180, 182, 190, 255};
    tokens.accent = {64, 120, 220, 255};
    tokens.on_accent = {255, 255, 255, 255};
    tokens.focus_ring = {64, 120, 220, 255};
    apply_theme_tokens(tokens);

    StyleRolePatch btn_roles{};
    btn_roles.has_bg_hover = true;
    btn_roles.has_bg_pressed = true;
    btn_roles.has_border_color = true;
    btn_roles.colors.bg_hover = StyleRole::SurfaceVariant;
    btn_roles.colors.bg_pressed = StyleRole::AccentPressed;
    btn_roles.colors.border_color = StyleRole::Accent;
    StyleSheet::instance().add_role_rule(StyleSelector{WidgetKind::Button, 0}, btn_roles);

    UiFactory factory{};
    auto root = factory.create_container();
    auto b1 = factory.create_button("A");
    auto b2 = factory.create_button("B");
    auto b3 = factory.create_button("C");

    factory.link(root, b1);
    factory.link(root, b2);
    factory.link(root, b3);

    int click_count = 0;
    if (auto* btn = factory.get_button(b1)) {
        btn->set_on_click(Callback{+[](void* ctx) { ++(*static_cast<int*>(ctx)); }, &click_count});
    }
    if (auto* btn = factory.get_button(b2)) {
        btn->set_on_click(Callback{+[](void* ctx) { ++(*static_cast<int*>(ctx)); }, &click_count});
    }
    if (auto* btn = factory.get_button(b3)) {
        btn->set_on_click(Callback{+[](void* ctx) { ++(*static_cast<int*>(ctx)); }, &click_count});
    }

    InputRouter router{factory, root};

    auto dispatch_raw = [&](const RawInputEvent& ev, const char* tag) {
        const auto bridge = input::adapter::bridge_from_raw(ev);
        if (bridge.event) {
            router.dispatch_event(*bridge.event);
        }
        const auto focused = router.focused();
        const int focus_index = focused ? static_cast<int>(focused.index) : -1;
        const char* focus_kind = widget_kind_name(focused.kind);
        std::printf("[%s] nav(delta=%d, act=%d, back=%d) focus=%s#%d clicks=%d\n",
                    tag,
                    static_cast<int>(bridge.nav.focus_delta),
                    static_cast<int>(bridge.nav.activated),
                    static_cast<int>(bridge.nav.back),
                    focus_kind,
                    focus_index,
                    click_count);
    };

    dispatch_raw(make_button_event(input::Button::Down, true, 1), "down");
    dispatch_raw(make_button_event(input::Button::Down, true, 2), "down");
    dispatch_raw(make_button_event(input::Button::Up, true, 3), "up");
    dispatch_raw(make_button_event(input::Button::Enter, true, 4), "enter");
    dispatch_raw(make_button_event(input::Button::Back, true, 5), "back");

    return 0;
}
