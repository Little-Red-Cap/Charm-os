#include <SDL3/SDL.h>
#include <cstdint>
#include <string_view>

import charm.core.soa_kernel;
import charm.core.soa_gui;
import charm.core.event;
import charm.core.config;
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
        kernel.input_test_set_capture(b);

        kernel.input_clear_events();
        kernel.destroy(b);
        const int cancel_b = count_event(kernel, Event::Type::Cancel, b);
        expect_true(cancel_b >= 1, "regress: captured cancel missing", fails);
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
#endif
}

int main(int argc, char** argv) {
#if defined(VIVID_SOA_TRACE_INPUT)
    bool run_regress = false;
#else
    (void)argc;
    (void)argv;
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        (void)out::error<"SDL_Init failed: {}">(SDL_GetError());
        return 1;
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    for (int i = 1; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--soa-regress") {
            run_regress = true;
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
