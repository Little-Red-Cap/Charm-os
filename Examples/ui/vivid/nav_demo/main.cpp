#include <cstdint>
#include <cstdio>

import charm.core.soa_kernel;
import charm.core.soa_router;
import charm.core.handle;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.theme_preset;

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
    btn_roles.bg_hover = StyleRole::SurfaceVariant;
    btn_roles.bg_pressed = StyleRole::AccentPressed;
    btn_roles.border_color = StyleRole::Accent;
    StyleSheet::instance().add_role_rule(StyleSelector{WidgetKind::Button, 0}, btn_roles);

    SoaKernel kernel{};
    SoaFactory factory{kernel};
    auto root = factory.create_container();
    auto b1 = factory.create_button("A");
    auto b2 = factory.create_button("B");
    auto b3 = factory.create_button("C");

    factory.link(root, b1);
    factory.link(root, b2);
    factory.link(root, b3);

    kernel.set_rect(root, {0, 0, 240, 160});
    kernel.set_rect(b1, {10, 10, 80, 40});
    kernel.set_rect(b2, {10, 60, 80, 40});
    kernel.set_rect(b3, {10, 110, 80, 40});

    int click_count = 0;

    SoaLayoutPass layout{kernel};
    SoaRouter router{kernel, layout, root};

    auto dispatch_click = [&](int x, int y, const char* tag) {
        router.dispatch_event(Event::mouse(Event::Type::MouseMove, x, y, 0));
        router.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1));
        router.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1));
        int frame_clicks = 0;
        for (std::size_t i = 0; i < kernel.input_event_count(); ++i) {
            const auto& ev = kernel.input_event(i);
            if (ev.event.type == Event::Type::Click) {
                frame_clicks += 1;
            }
        }
        click_count += frame_clicks;
        const auto focused = router.focused();
        const int focus_index = focused ? static_cast<int>(focused.index) : -1;
        const char* focus_kind = widget_kind_name(focused.kind);
        std::printf("[%s] focus=%s#%d clicks=%d\n", tag, focus_kind, focus_index, click_count);
    };

    dispatch_click(20, 20, "click-1");
    dispatch_click(20, 70, "click-2");
    dispatch_click(20, 120, "click-3");

    return 0;
}
