#include <SDL3/SDL.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>

import charm.core.gui;
import charm.core.factory;
import charm.core.event;
import charm.core.handle;
import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.theme_preset;
import charm.core.config;
import charm.gfx.canvas;
import charm.widgets.button;
import charm.widgets.checkbox;
import charm.widgets.label;
import charm.widgets.list;
import charm.widgets.progress;
import charm.widgets.radio;
import charm.widgets.slider;
import charm.widgets.switcher;

namespace {
    struct ThemeEntry {
        const char* name;
        ThemeTokens tokens;
    };

    constexpr std::uint8_t kVariantSecondary = 1;

    constexpr std::uint8_t mask_hover() noexcept {
        return static_cast<std::uint8_t>(StyleStateFlag::Hovered);
    }

    constexpr std::uint8_t mask_pressed() noexcept {
        return static_cast<std::uint8_t>(StyleStateFlag::Pressed);
    }

    struct Viewport {
        int x{0};
        int y{0};
        int w{0};
        int h{0};
        float scale{1.0f};
    };

    Viewport compute_viewport(int win_w, int win_h, int canvas_w, int canvas_h) noexcept {
        const float sx = static_cast<float>(win_w) / static_cast<float>(canvas_w);
        const float sy = static_cast<float>(win_h) / static_cast<float>(canvas_h);
        const float scale = (sx < sy) ? sx : sy;
        const int w = static_cast<int>(static_cast<float>(canvas_w) * scale);
        const int h = static_cast<int>(static_cast<float>(canvas_h) * scale);
        const int x = (win_w - w) / 2;
        const int y = (win_h - h) / 2;
        return Viewport{x, y, w, h, scale};
    }

    bool map_mouse(const Viewport& vp, int wx, int wy, int& out_x, int& out_y) noexcept {
        if (wx < vp.x || wy < vp.y || wx >= vp.x + vp.w || wy >= vp.y + vp.h) return false;
        out_x = static_cast<int>((wx - vp.x) / vp.scale);
        out_y = static_cast<int>((wy - vp.y) / vp.scale);
        return true;
    }

    bool style_sheet_selftest() {
        auto& sheet = StyleSheet::instance();
        sheet.clear();

        const Style& base = Theme::instance().get<Button>();
        Style out{};

        auto mk = [](std::uint8_t r, std::uint8_t g, std::uint8_t b) noexcept {
            return rgba{r, g, b, 255};
        };
        auto add_bg_rule = [&](StyleSelector sel, rgba c) {
            StylePatch patch{};
            patch.has_bg_color = true;
            patch.bg_color = c;
            sheet.add_rule(sel, patch);
        };
        auto eq = [](const rgba& a, const rgba& b) noexcept {
            return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
        };
        auto check = [&](WidgetKind kind, const StyleState& state, rgba expected, const char* label) {
            if (!sheet.apply(kind, state, out, base)) {
                std::fprintf(stderr, "StyleSheet selftest no match: %s\n", label);
                return false;
            }
            if (!eq(out.colors.bg_color, expected)) {
                std::fprintf(stderr, "StyleSheet selftest mismatch: %s\n", label);
                return false;
            }
            return true;
        };

        const std::uint8_t hover = mask_hover();
        const std::uint8_t pressed = mask_pressed();
        add_bg_rule(StyleSelector{WidgetKind::None, hover}, mk(1, 2, 3));
        add_bg_rule(StyleSelector{WidgetKind::Button, hover}, mk(4, 5, 6));
        add_bg_rule(StyleSelector{WidgetKind::Button, static_cast<std::uint8_t>(hover | pressed)}, mk(7, 8, 9));
        add_bg_rule(StyleSelector{WidgetKind::Button, hover, kVariantSecondary}, mk(10, 11, 12));

        bool ok = true;
        ok = check(WidgetKind::Button,
                   make_style_state(true, true, true, false, 0),
                   mk(7, 8, 9),
                   "button hover+pressed") && ok;
        ok = check(WidgetKind::Button,
                   make_style_state(true, true, false, false, 0),
                   mk(4, 5, 6),
                   "button hover") && ok;
        ok = check(WidgetKind::Button,
                   make_style_state(true, true, false, false, kVariantSecondary),
                   mk(10, 11, 12),
                   "button variant hover") && ok;
        ok = check(WidgetKind::Checkbox,
                   make_style_state(true, true, false, false, 0),
                   mk(1, 2, 3),
                   "generic hover") && ok;

        sheet.clear();
        return ok;
    }
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Vivid Theme Demo", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                             screen_width, screen_height);
    if (!texture) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    DefaultFrameBuffer fb{};
    DefaultCanvas canvas{fb};

    UiFactory factory{};
    auto root = factory.create_container();
    if (auto* root_obj = factory.get(root)) {
        root_obj->set_rect({0, 0, screen_width, screen_height});
    }

    auto title = factory.create_label("Theme: Light");
    auto subtitle = factory.create_label("Keys: 1 Light  2 Dark  3 High Contrast");

    auto btn_primary = factory.create_button("Primary");
    auto btn_secondary = factory.create_button("Secondary");
    auto sw = factory.create_switch();
    auto cb = factory.create_checkbox("Enable feature");
    auto radio = factory.create_radio("Radio option");
    auto slider = factory.create_slider();
    auto progress = factory.create_progress();
    auto list_item = factory.create_list_item("List item");

    factory.link(root, title);
    factory.link(root, subtitle);
    factory.link(root, btn_primary);
    factory.link(root, btn_secondary);
    factory.link(root, sw);
    factory.link(root, cb);
    factory.link(root, radio);
    factory.link(root, slider);
    factory.link(root, progress);
    factory.link(root, list_item);

    const int col_x = 24;
    const int col_w = 280;
    int y = 20;
    const int row_h = 36;
    const int gap = 12;

    if (auto* lbl = factory.get_label(title)) {
        lbl->set_rect({col_x, y, screen_width - col_x * 2, 24});
    }
    y += 28;
    if (auto* lbl = factory.get_label(subtitle)) {
        lbl->set_rect({col_x, y, screen_width - col_x * 2, 20});
    }
    y += 36;

    if (auto* btn = factory.get_button(btn_primary)) {
        btn->set_rect({col_x, y, col_w, row_h});
    }
    y += row_h + gap;
    if (auto* btn = factory.get_button(btn_secondary)) {
        btn->set_rect({col_x, y, col_w, row_h});
        btn->set_style_variant(kVariantSecondary);
    }
    y += row_h + gap;
    if (auto* s = factory.get_switch(sw)) {
        s->set_rect({col_x, y, 64, 28});
        s->set_on(true);
    }
    if (auto* c = factory.get_checkbox(cb)) {
        c->set_rect({col_x + 90, y, col_w, 28});
        c->set_checked(true);
    }
    y += 40;
    if (auto* r = factory.get_radio(radio)) {
        r->set_rect({col_x, y, col_w, 28});
        r->set_checked(true);
    }
    y += 40;
    if (auto* s = factory.get_slider(slider)) {
        s->set_rect({col_x, y, col_w, 24});
        s->set_range(0, 100);
        s->set_value(60);
    }
    y += 40;
    if (auto* p = factory.get_progress(progress)) {
        p->set_rect({col_x, y, col_w, 16});
        p->set_value(30);
    }
    y += 32;
    if (auto* li = factory.get_list_item(list_item)) {
        li->set_rect({col_x, y, col_w, 28});
    }

    ThemeEntry themes[] = {
        {"Light", ThemeTokens{
            .surface = {244, 244, 248, 255},
            .surface_variant = {230, 232, 238, 255},
            .on_surface = {32, 32, 38, 255},
            .on_surface_muted = {120, 120, 128, 255},
            .outline = {180, 182, 190, 255},
            .accent = {64, 120, 220, 255},
            .on_accent = {255, 255, 255, 255},
            .danger = {200, 60, 60, 255},
            .on_danger = {255, 255, 255, 255},
            .focus_ring = {64, 120, 220, 255},
        }},
        {"Dark", ThemeTokens{
            .surface = {26, 28, 34, 255},
            .surface_variant = {40, 44, 52, 255},
            .on_surface = {220, 226, 232, 255},
            .on_surface_muted = {140, 146, 156, 255},
            .outline = {64, 70, 80, 255},
            .accent = {90, 180, 255, 255},
            .on_accent = {16, 20, 28, 255},
            .danger = {240, 96, 96, 255},
            .on_danger = {16, 20, 28, 255},
            .focus_ring = {90, 180, 255, 255},
        }},
        {"High Contrast", ThemeTokens{
            .surface = {16, 16, 16, 255},
            .surface_variant = {32, 32, 32, 255},
            .on_surface = {250, 250, 250, 255},
            .on_surface_muted = {200, 200, 200, 255},
            .outline = {255, 255, 255, 255},
            .accent = {255, 196, 0, 255},
            .on_accent = {32, 32, 32, 255},
            .danger = {255, 72, 72, 255},
            .on_danger = {0, 0, 0, 255},
            .focus_ring = {255, 196, 0, 255},
        }},
    };

    if (!style_sheet_selftest()) {
        std::fprintf(stderr, "StyleSheet selftest failed; rule priority may be incorrect.\n");
    }

    auto apply_demo_theme = [&](int index) {
        const int count = static_cast<int>(sizeof(themes) / sizeof(themes[0]));
        if (index < 0) index = 0;
        if (index >= count) index = count - 1;
        apply_theme_tokens(themes[index].tokens);
        auto& sheet = StyleSheet::instance();
        sheet.clear();

        StyleRolePatch btn_roles{};
        btn_roles.has_bg_color = true;
        btn_roles.has_bg_hover = true;
        btn_roles.has_bg_pressed = true;
        btn_roles.has_border_color = true;
        btn_roles.bg_color = StyleRole::SurfaceVariant;
        btn_roles.bg_hover = StyleRole::SurfaceHover;
        btn_roles.bg_pressed = StyleRole::AccentPressed;
        btn_roles.border_color = StyleRole::Accent;
        sheet.add_role_rule(StyleSelector{WidgetKind::Button, 0}, btn_roles);

        StyleRolePatch btn_secondary_roles{};
        btn_secondary_roles.has_bg_color = true;
        btn_secondary_roles.has_bg_hover = true;
        btn_secondary_roles.has_bg_pressed = true;
        btn_secondary_roles.has_border_color = true;
        btn_secondary_roles.has_font_color = true;
        btn_secondary_roles.bg_color = StyleRole::Surface;
        btn_secondary_roles.bg_hover = StyleRole::SurfaceHover;
        btn_secondary_roles.bg_pressed = StyleRole::SurfacePressed;
        btn_secondary_roles.border_color = StyleRole::Outline;
        btn_secondary_roles.font_color = StyleRole::OnSurface;
        sheet.add_role_rule(StyleSelector{WidgetKind::Button, 0, kVariantSecondary}, btn_secondary_roles);

        StyleRolePatch list_hover{};
        list_hover.has_bg_color = true;
        list_hover.bg_color = StyleRole::SurfaceVariant;
        sheet.add_role_rule(StyleSelector{WidgetKind::ListItem, mask_hover()}, list_hover);

        StyleRolePatch list_pressed{};
        list_pressed.has_bg_color = true;
        list_pressed.has_font_color = true;
        list_pressed.bg_color = StyleRole::AccentPressed;
        list_pressed.font_color = StyleRole::OnAccent;
        sheet.add_role_rule(StyleSelector{WidgetKind::ListItem, mask_pressed()}, list_pressed);

        if (auto* lbl = factory.get_label(title)) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Theme: %s", themes[index].name);
            lbl->set_text(buf);
        }
    };

    int theme_index = 0;
    apply_demo_theme(theme_index);

    Gui gui{canvas, factory, root};

    auto t0 = std::chrono::steady_clock::now();
    bool running = true;
    while (running) {
        SDL_Event evt{};
        int win_w = 0;
        int win_h = 0;
        SDL_GetWindowSize(window, &win_w, &win_h);
        const auto vp = compute_viewport(win_w, win_h, screen_width, screen_height);

        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            if (evt.type == SDL_EVENT_KEY_DOWN) {
                if (evt.key.key == SDLK_1) { theme_index = 0; apply_demo_theme(theme_index); }
                if (evt.key.key == SDLK_2) { theme_index = 1; apply_demo_theme(theme_index); }
                if (evt.key.key == SDLK_3) { theme_index = 2; apply_demo_theme(theme_index); }

                Event::Key key = Event::Key::Unknown;
                if (evt.key.key == SDLK_UP) key = Event::Key::Up;
                else if (evt.key.key == SDLK_DOWN) key = Event::Key::Down;
                else if (evt.key.key == SDLK_LEFT) key = Event::Key::Left;
                else if (evt.key.key == SDLK_RIGHT) key = Event::Key::Right;
                else if (evt.key.key == SDLK_RETURN) key = Event::Key::Enter;
                else if (evt.key.key == SDLK_SPACE) key = Event::Key::Space;
                if (key != Event::Key::Unknown) {
                    gui.dispatch_event(Event::key(Event::Type::KeyDown, key));
                }
            } else if (evt.type == SDL_EVENT_KEY_UP) {
                Event::Key key = Event::Key::Unknown;
                if (evt.key.key == SDLK_UP) key = Event::Key::Up;
                else if (evt.key.key == SDLK_DOWN) key = Event::Key::Down;
                else if (evt.key.key == SDLK_LEFT) key = Event::Key::Left;
                else if (evt.key.key == SDLK_RIGHT) key = Event::Key::Right;
                else if (evt.key.key == SDLK_RETURN) key = Event::Key::Enter;
                else if (evt.key.key == SDLK_SPACE) key = Event::Key::Space;
                if (key != Event::Key::Unknown) {
                    gui.dispatch_event(Event::key(Event::Type::KeyUp, key));
                }
            } else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
                int x = 0;
                int y = 0;
                if (map_mouse(vp, evt.motion.x, evt.motion.y, x, y)) {
                    gui.dispatch_event(Event::mouse(Event::Type::MouseMove, x, y, 0));
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    int x = 0;
                    int y = 0;
                    if (map_mouse(vp, evt.button.x, evt.button.y, x, y)) {
                        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1));
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    int x = 0;
                    int y = 0;
                    if (map_mouse(vp, evt.button.x, evt.button.y, x, y)) {
                        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, x, y, 1));
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_WHEEL) {
                int x = 0;
                int y = 0;
                if (map_mouse(vp, evt.wheel.mouse_x, evt.wheel.mouse_y, x, y)) {
                    gui.dispatch_event(Event::wheel(x, y, static_cast<int>(evt.wheel.y)));
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const float t = std::chrono::duration<float>(now - t0).count();
        if (auto* p = factory.get_progress(progress)) {
            const int value = static_cast<int>((std::sin(t) * 0.5f + 0.5f) * 100.0f);
            p->set_value(value);
        }

        gui.render();

        SDL_UpdateTexture(texture, nullptr, canvas.data(), screen_width * 3);
        SDL_SetRenderDrawColor(renderer, 12, 12, 12, 255);
        SDL_RenderClear(renderer);
        SDL_FRect dst{
            static_cast<float>(vp.x),
            static_cast<float>(vp.y),
            static_cast<float>(vp.w),
            static_cast<float>(vp.h)
        };
        SDL_RenderTexture(renderer, texture, nullptr, &dst);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
