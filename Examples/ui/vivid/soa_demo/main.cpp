#include <SDL3/SDL.h>
#include <cstdint>
#include <string_view>

import charm.core.soa_kernel;
import charm.core.soa_gui;
import charm.core.event;
import charm.core.config;
import charm.core.theme_preset;
import charm.gfx.canvas;
import out.api;

namespace {
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

#if defined(VIVID_SOA_TRACE_INPUT)
    const char* event_type_name(Event::Type type) noexcept {
        switch (type) {
            case Event::Type::HoverEnter: return "HoverEnter";
            case Event::Type::HoverLeave: return "HoverLeave";
            case Event::Type::MouseDown: return "MouseDown";
            case Event::Type::MouseUp: return "MouseUp";
            case Event::Type::MouseMove: return "MouseMove";
            case Event::Type::MouseWheel: return "MouseWheel";
            case Event::Type::Click: return "Click";
            case Event::Type::DragStart: return "DragStart";
            case Event::Type::DragMove: return "DragMove";
            case Event::Type::DragEnd: return "DragEnd";
            case Event::Type::GestureSwipe: return "GestureSwipe";
            case Event::Type::GesturePinch: return "GesturePinch";
            case Event::Type::FocusIn: return "FocusIn";
            case Event::Type::FocusOut: return "FocusOut";
            case Event::Type::KeyDown: return "KeyDown";
            case Event::Type::KeyUp: return "KeyUp";
            case Event::Type::Cancel: return "Cancel";
        }
        return "Unknown";
    }

    bool same_handle(WidgetHandle a, WidgetHandle b) noexcept {
        return a.kind == b.kind && a.index == b.index && a.generation == b.generation;
    }

    int find_event_index(const SoaKernel& kernel, Event::Type type, WidgetHandle target = {}) noexcept {
        const std::size_t count = kernel.input_event_count();
        for (std::size_t i = 0; i < count; ++i) {
            const auto& item = kernel.input_event(i);
            if (item.event.type != type) continue;
            if (target && !same_handle(item.target, target)) continue;
            return static_cast<int>(i);
        }
        return -1;
    }

    int count_event(const SoaKernel& kernel, Event::Type type, WidgetHandle target = {}) noexcept {
        int total = 0;
        const std::size_t count = kernel.input_event_count();
        for (std::size_t i = 0; i < count; ++i) {
            const auto& item = kernel.input_event(i);
            if (item.event.type != type) continue;
            if (target && !same_handle(item.target, target)) continue;
            ++total;
        }
        return total;
    }

    bool expect_true(bool cond, const char* label, int& fails) noexcept {
        if (cond) return true;
        (void)out::println<"[soa][fail] {}">(label);
        ++fails;
        return false;
    }

    void trace_input_events(SoaKernel& kernel) noexcept {
        const std::size_t count = kernel.input_event_count();
        if (count == 0 && !kernel.input_events_overflowed()) return;
        if (kernel.input_events_overflowed()) {
            (void)out::println<"[soa] input overflow">();
        }
        for (std::size_t i = 0; i < count; ++i) {
            const auto& item = kernel.input_event(i);
            (void)out::println<"[soa] ev: kind={} idx={} gen={} type={} x={} y={} dx={} dy={}">(
                widget_kind_name(item.target.kind),
                static_cast<int>(item.target.index),
                static_cast<int>(item.target.generation),
                event_type_name(item.event.type),
                item.event.x,
                item.event.y,
                item.event.dx,
                item.event.dy
            );
        }
    }

    bool run_input_regression(SoaGui& gui, SoaKernel& kernel, SoaFactory& factory, WidgetHandle root) noexcept {
        int fails = 0;
        kernel.input_clear_events();

        auto test_root = factory.create_container();
        auto sc = factory.create_scroll_container();
        factory.link(root, test_root);
        factory.link(test_root, sc);
        kernel.set_rect(test_root, {40, 40, 220, 180});
        kernel.set_rect(sc, {10, 10, 160, 100});

        const int x = 60;
        const int y = 60;
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, x, y, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, x, y, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, x + 20, y + 20, 0));

        expect_true(kernel.input_dragging(), "regress: dragging not started", fails);

        kernel.input_clear_events();
        kernel.destroy(test_root);

        const int idx_drag_end = find_event_index(kernel, Event::Type::DragEnd);
        const int idx_cancel = find_event_index(kernel, Event::Type::Cancel);
        const int idx_hover = find_event_index(kernel, Event::Type::HoverLeave);
        const int idx_focus = find_event_index(kernel, Event::Type::FocusOut);
        expect_true(idx_drag_end >= 0, "regress: destroy missing DragEnd", fails);
        expect_true(idx_cancel >= 0, "regress: destroy missing Cancel", fails);
        expect_true(idx_hover >= 0, "regress: destroy missing HoverLeave", fails);
        expect_true(idx_focus >= 0, "regress: destroy missing FocusOut", fails);
        if (idx_drag_end >= 0 && idx_cancel >= 0) {
            expect_true(idx_drag_end < idx_cancel, "regress: DragEnd order", fails);
        }
        if (idx_cancel >= 0 && idx_hover >= 0) {
            expect_true(idx_cancel < idx_hover, "regress: Cancel order", fails);
        }
        if (idx_hover >= 0 && idx_focus >= 0) {
            expect_true(idx_hover < idx_focus, "regress: HoverLeave order", fails);
        }

        expect_true(!kernel.input_pressed(), "regress: pressed not cleared", fails);
        expect_true(!kernel.input_captured(), "regress: captured not cleared", fails);
        expect_true(!kernel.input_hovered(), "regress: hovered not cleared", fails);
        expect_true(!kernel.input_focused(), "regress: focused not cleared", fails);
        expect_true(!kernel.input_dragging(), "regress: dragging not cleared", fails);

        kernel.destroy(sc);
        kernel.destroy(test_root);

        auto test_root2 = factory.create_container();
        auto a = factory.create_button("A");
        auto b = factory.create_checkbox("B");
        factory.link(root, test_root2);
        factory.link(test_root2, a);
        factory.link(test_root2, b);
        kernel.set_rect(test_root2, {300, 40, 220, 120});
        kernel.set_rect(a, {10, 10, 120, 32});
        kernel.set_rect(b, {10, 50, 120, 32});

        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 320, 60, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, 320, 60, 1));
        kernel.input_test_request_capture(b);

        kernel.input_clear_events();
        kernel.destroy(b);
        const int cancel_b = count_event(kernel, Event::Type::Cancel, b);
        expect_true(cancel_b > 0, "regress: Cancel(B) missing", fails);
        expect_true(!kernel.input_captured(), "regress: captured not cleared", fails);

        kernel.input_clear_events();
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, 320, 60, 1));

        kernel.destroy(a);
        kernel.destroy(test_root2);

        auto test_root3 = factory.create_container();
        auto c = factory.create_checkbox("C");
        factory.link(root, test_root3);
        factory.link(test_root3, c);
        kernel.set_rect(test_root3, {560, 40, 200, 120});
        kernel.set_rect(c, {10, 10, 120, 32});

        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 580, 60, 0));
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, 580, 60, 1));
        kernel.input_test_force_overflow();
        expect_true(kernel.input_events_overflowed(), "regress: overflow flag missing", fails);
        expect_true(!kernel.input_pressed(), "regress: overflow pressed not cleared", fails);
        expect_true(!kernel.input_captured(), "regress: overflow captured not cleared", fails);
        expect_true(!kernel.input_hovered(), "regress: overflow hovered not cleared", fails);
        expect_true(!kernel.input_focused(), "regress: overflow focused not cleared", fails);
        expect_true(!kernel.input_dragging(), "regress: overflow dragging not cleared", fails);

        kernel.destroy(c);
        kernel.destroy(test_root3);

        if (fails == 0) {
            (void)out::println<"[soa] input regression OK">();
        }
        return fails == 0;
    }

    bool run_layout_regression(SoaGui& gui, SoaKernel& kernel, SoaFactory& factory, WidgetHandle root) noexcept {
        int fails = 0;

        auto layout_root = factory.create_container();
        auto layout_box = factory.create_checkbox("Layout");
        factory.link(root, layout_root);
        factory.link(layout_root, layout_box);
        kernel.set_rect(layout_root, {40, 260, 220, 120});
        kernel.set_rect(layout_box, {10, 10, 180, 32});

        gui.render();

        kernel.set_layout_state_influence(false);
        gui.render();
        kernel.layout_trace_reset();

        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 60, 280, 0));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "layout: hover invalidated with influence off", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: hover pass with influence off", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: hover missing paint invalidation", fails);
        kernel.layout_trace_reset();

        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, 60, 280, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 90, 300, 0));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "layout: drag invalidated with influence off", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: drag pass with influence off", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: drag missing paint invalidation", fails);
        kernel.layout_trace_reset();

        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, 90, 300, 1));
        gui.dispatch_event(Event::wheel(60, 280, 1));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "layout: wheel invalidated with influence off", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: wheel pass with influence off", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: wheel missing paint invalidation", fails);

        kernel.set_layout_state_influence(true);
        gui.render();
        kernel.layout_trace_reset();

        kernel.set_focused(layout_box, true);
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "layout: focus invalidated but mask forbids", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: focus pass but mask forbids", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: focus missing paint invalidation", fails);

        kernel.layout_trace_reset();
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 60, 280, 0));
        gui.render();
        expect_true(kernel.layout_invalidated_count() == 0, "layout: hover invalidated but mask forbids", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: hover pass but mask forbids", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: hover missing paint invalidation", fails);

        auto scroll_root = factory.create_container();
        auto scroll = factory.create_scroll_container();
        auto scroll_a = factory.create_button("Scroll A");
        auto scroll_b = factory.create_button("Scroll B");
        auto scroll_c = factory.create_button("Scroll C");
        factory.link(root, scroll_root);
        factory.link(scroll_root, scroll);
        factory.link(scroll, scroll_a);
        factory.link(scroll, scroll_b);
        factory.link(scroll, scroll_c);
        kernel.set_rect(scroll_root, {280, 260, 220, 120});
        kernel.set_rect(scroll, {10, 10, 180, 80});
        kernel.set_rect(scroll_a, {0, 0, 160, 30});
        kernel.set_rect(scroll_b, {0, 35, 160, 30});
        kernel.set_rect(scroll_c, {0, 70, 160, 30});
        kernel.set_scroll_step(scroll, 12);
        gui.render();

        kernel.layout_trace_reset();
        const int scroll_before = kernel.scroll_y(scroll);
        gui.dispatch_event(Event::wheel(300, 280, 1));
        gui.render();
        const int scroll_after = kernel.scroll_y(scroll);
        expect_true(scroll_after != scroll_before, "layout: scroll container did not scroll", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "layout: scroll container invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: scroll container pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: scroll container missing paint invalidation", fails);

        kernel.layout_trace_reset();
        const int drag_before = kernel.scroll_y(scroll);
        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, 300, 280, 1));
        gui.dispatch_event(Event::mouse(Event::Type::MouseMove, 300, 320, 0));
        gui.render();
        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, 300, 320, 1));
        const int drag_after = kernel.scroll_y(scroll);
        expect_true(drag_after != drag_before, "layout: scroll drag did not scroll", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "layout: scroll drag invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: scroll drag pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: scroll drag missing paint invalidation", fails);

        auto list_root = factory.create_container();
        auto list = factory.create_list();
        auto list_item_a = factory.create_list_item("Item A");
        auto list_item_b = factory.create_list_item("Item B");
        auto list_item_c = factory.create_list_item("Item C");
        auto list_item_d = factory.create_list_item("Item D");
        auto list_item_e = factory.create_list_item("Item E");
        auto list_item_f = factory.create_list_item("Item F");
        factory.link(root, list_root);
        factory.link(list_root, list);
        factory.link(list, list_item_a);
        factory.link(list, list_item_b);
        factory.link(list, list_item_c);
        factory.link(list, list_item_d);
        factory.link(list, list_item_e);
        factory.link(list, list_item_f);
        kernel.set_rect(list_root, {40, 420, 220, 120});
        kernel.set_rect(list, {10, 10, 180, 80});
        kernel.set_list_row_height(list, 24);
        kernel.set_scroll_step(list, 12);
        gui.render();

        kernel.layout_trace_reset();
        const int list_before = kernel.scroll_y(list);
        gui.dispatch_event(Event::wheel(60, 440, 1));
        gui.render();
        const int list_after = kernel.scroll_y(list);
        expect_true(list_after != list_before, "layout: list did not scroll", fails);
        expect_true(kernel.layout_invalidated_count() == 0, "layout: list invalidated layout", fails);
        expect_true(kernel.layout_pass_count() == 0, "layout: list pass", fails);
        expect_true(kernel.paint_invalidated_count() > 0, "layout: list missing paint invalidation", fails);

        kernel.layout_trace_reset();
        kernel.set_text(layout_box, "Layout Updated");
        gui.render();
        expect_true(kernel.layout_invalidated_count() > 0, "layout: text change did not invalidate", fails);
        expect_true(kernel.layout_pass_count() > 0, "layout: text change did not run pass", fails);

        kernel.destroy(list_item_f);
        kernel.destroy(list_item_e);
        kernel.destroy(list_item_d);
        kernel.destroy(list_item_c);
        kernel.destroy(list_item_b);
        kernel.destroy(list_item_a);
        kernel.destroy(list);
        kernel.destroy(list_root);
        kernel.destroy(scroll_c);
        kernel.destroy(scroll_b);
        kernel.destroy(scroll_a);
        kernel.destroy(scroll);
        kernel.destroy(scroll_root);
        kernel.destroy(layout_box);
        kernel.destroy(layout_root);

        if (fails == 0) {
            (void)out::println<"[soa] layout regression OK">();
        }
        return fails == 0;
    }

    bool run_style_regression(SoaGui& gui) noexcept {
        int fails = 0;
        auto& sheet = StyleSheet::instance();
        sheet.style_trace_reset();
        const std::uint32_t role_before = sheet.role_palette_compile_count();
        const std::uint32_t table_before = sheet.style_table_compile_count();

        gui.render();
        expect_true(sheet.role_palette_compile_count() == role_before,
                    "style: role palette compiled without token change", fails);
        expect_true(sheet.style_table_compile_count() == table_before,
                    "style: style table compiled without token change", fails);

        ThemeTokens tokens = Theme::instance().get_tokens();
        apply_theme_tokens(tokens);
        const std::uint32_t role_after = sheet.role_palette_compile_count();
        const std::uint32_t table_after = sheet.style_table_compile_count();
        expect_true(role_after == role_before + 1u, "style: role palette not rebuilt", fails);
        expect_true(table_after == table_before + 1u, "style: style table not rebuilt", fails);

        if (fails == 0) {
            (void)out::println<"[soa] style regression OK">();
        }
        return fails == 0;
    }
#endif
}

int main(int argc, char** argv) {
#if defined(VIVID_SOA_TRACE_INPUT)
    bool run_regress = false;
    bool run_regress_layout = false;
#else
    (void)argc;
    (void)argv;
#endif
    apply_theme_tokens(ThemeTokens{});
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        (void)out::error<"SDL_Init failed: {}">(SDL_GetError());
        return 1;
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--soa-regress") {
            run_regress = true;
            run_regress_layout = true;
        } else if (std::string_view(argv[i]) == "--soa-regress-layout") {
            run_regress_layout = true;
        }
    }
#endif

    SDL_Window* window = SDL_CreateWindow("Vivid SoA Demo", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
    if (!window) {
        (void)out::error<"SDL_CreateWindow failed: {}">(SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        (void)out::error<"SDL_CreateRenderer failed: {}">(SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                             screen_width, screen_height);
    if (!texture) {
        (void)out::error<"SDL_CreateTexture failed: {}">(SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    DefaultFrameBuffer fb{};
    DefaultCanvas canvas{fb};

    SoaKernel kernel{};
    SoaFactory factory{kernel};

    auto root = factory.create_container();
    kernel.set_rect(root, {0, 0, screen_width, screen_height});
    kernel.set_clip_children(root, true);

    auto title = factory.create_label("SoA Kernel Demo");
    auto btn = factory.create_button("Press");
    auto sw = factory.create_switch();
    auto checkbox = factory.create_checkbox("Checkbox");
    auto radio = factory.create_radio("Radio");
    auto slider = factory.create_slider();
    auto progress = factory.create_progress();
    auto list = factory.create_list();
    auto scroll = factory.create_scroll_container();

    factory.link(root, title);
    factory.link(root, btn);
    factory.link(root, sw);
    factory.link(root, checkbox);
    factory.link(root, radio);
    factory.link(root, slider);
    factory.link(root, progress);
    factory.link(root, list);
    factory.link(root, scroll);

    kernel.set_rect(title, {24, 16, screen_width - 48, 24});
    kernel.set_rect(btn, {24, 60, 160, 40});
    kernel.set_rect(sw, {24, 112, 96, 32});
    kernel.set_rect(checkbox, {24, 160, 200, 32});
    kernel.set_rect(radio, {24, 200, 200, 32});
    kernel.set_rect(slider, {24, 250, 280, 24});
    kernel.set_rect(progress, {24, 290, 280, 18});
    kernel.set_rect(list, {24, 340, 200, 200});
    kernel.set_rect(scroll, {250, 340, 200, 200});

    kernel.set_range(slider, 0, 100);
    kernel.set_range(progress, 0, 100);
    kernel.set_list_row_height(list, 28);

    const char* list_items[] = {
        "Item 1", "Item 2", "Item 3", "Item 4", "Item 5",
        "Item 6", "Item 7", "Item 8", "Item 9", "Item 10"
    };
    for (const char* item_text : list_items) {
        auto item = factory.create_list_item(item_text);
        factory.link(list, item);
    }

    const char* scroll_rows[] = {
        "Row 1", "Row 2", "Row 3", "Row 4", "Row 5", "Row 6",
        "Row 7", "Row 8", "Row 9", "Row 10", "Row 11", "Row 12"
    };
    int row_y = 0;
    for (const char* row_text : scroll_rows) {
        auto row = factory.create_label(row_text);
        factory.link(scroll, row);
        kernel.set_rect(row, {8, row_y, 160, 20});
        row_y += 22;
    }

    SoaGui gui(canvas, kernel, root);

#if defined(VIVID_SOA_TRACE_INPUT)
    if (run_regress) {
        if (!run_input_regression(gui, kernel, factory, root)) {
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        if (!run_style_regression(gui)) {
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }
    if (run_regress_layout) {
        if (!run_layout_regression(gui, kernel, factory, root)) {
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }
#endif

    int win_w = screen_width;
    int win_h = screen_height;
    int mouse_x = 0;
    int mouse_y = 0;
    bool running = true;
    int value = 0;

    while (running) {
        SDL_Event evt{};
        SDL_GetWindowSize(window, &win_w, &win_h);
        const Viewport vp = compute_viewport(win_w, win_h, screen_width, screen_height);

        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            if (evt.type == SDL_EVENT_MOUSE_MOTION) {
                if (map_mouse(vp, evt.motion.x, evt.motion.y, mouse_x, mouse_y)) {
                    gui.dispatch_event(Event::mouse(Event::Type::MouseMove, mouse_x, mouse_y, 0));
#if defined(VIVID_SOA_TRACE_INPUT)
                    trace_input_events(kernel);
#endif
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    if (map_mouse(vp, evt.button.x, evt.button.y, mouse_x, mouse_y)) {
                        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, mouse_x, mouse_y, 1));
#if defined(VIVID_SOA_TRACE_INPUT)
                        trace_input_events(kernel);
#endif
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    if (map_mouse(vp, evt.button.x, evt.button.y, mouse_x, mouse_y)) {
                        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, mouse_x, mouse_y, 1));
#if defined(VIVID_SOA_TRACE_INPUT)
                        trace_input_events(kernel);
#endif
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_WHEEL) {
                gui.dispatch_event(Event::wheel(mouse_x, mouse_y, static_cast<int>(evt.wheel.y)));
#if defined(VIVID_SOA_TRACE_INPUT)
                trace_input_events(kernel);
#endif
            }
        }

        value = (value + 1) % 101;
        kernel.set_value(progress, value);
        if (!kernel.pressed(slider)) {
            kernel.set_value(slider, value);
        }

        gui.render();

        SDL_UpdateTexture(texture, nullptr, canvas.data(), static_cast<int>(DefaultFrameBuffer::stride_bytes));
        SDL_SetRenderDrawColor(renderer, 10, 10, 10, 255);
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
