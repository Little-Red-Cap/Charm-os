#include <SDL3/SDL.h>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>

import charm.core.gui;
import charm.core.factory;
import charm.core.event;
import charm.core.config;
import charm.gfx.canvas;
import charm.gfx.framebuffer;
import charm.widgets.button;
import charm.widgets.label;
import charm.widgets.progress;
import charm.widgets.slider;
import charm.widgets.switcher;

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

    struct SDLTileBackend {
        DefaultFrameBuffer& fb;

        int width() const noexcept { return screen_width; }
        int height() const noexcept { return screen_height; }
        void begin_frame() noexcept {}
        void end_frame() noexcept {}

        void blit_span(int x, int y, const std::byte* src, std::size_t bytes) noexcept {
            if (!src || bytes == 0) return;
            if (x < 0 || y < 0 || x >= screen_width || y >= screen_height) return;
            const std::size_t stride = DefaultFrameBuffer::stride_bytes;
            const std::size_t bpp = DefaultFrameBuffer::bytes_per_pixel;
            const std::size_t offset = static_cast<std::size_t>(y) * stride
                + static_cast<std::size_t>(x) * bpp;
            std::memcpy(fb.data() + offset, src, bytes);
        }

        void mark_dirty(int, int, int, int) noexcept {}
    };
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Vivid Tile Demo", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
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
    SDLTileBackend backend{fb};

    using TileFrameBuffer = FrameBuffer<screen_pixel_format, 128, 128>;
    TileFrameBuffer tile_fb{};
    FrameBufferView tile_view{
        screen_pixel_format,
        tile_fb.data(),
        TileFrameBuffer::width,
        TileFrameBuffer::height,
        TileFrameBuffer::stride_bytes
    };

    UiFactory factory{};
    auto root = factory.create_container();
    if (auto* root_obj = factory.get(root)) {
        root_obj->set_rect({0, 0, screen_width, screen_height});
    }

    auto title = factory.create_label("Tile/PFB Render Demo");
    auto btn = factory.create_button("Press");
    auto sw = factory.create_switch();
    auto slider = factory.create_slider();
    auto progress = factory.create_progress();

    factory.link(root, title);
    factory.link(root, btn);
    factory.link(root, sw);
    factory.link(root, slider);
    factory.link(root, progress);

    if (auto* obj = factory.get_label(title)) {
        obj->set_rect({24, 16, screen_width - 48, 24});
    }
    if (auto* obj = factory.get(btn)) {
        obj->set_rect({24, 60, 160, 40});
    }
    if (auto* obj = factory.get(sw)) {
        obj->set_rect({24, 112, 96, 32});
    }
    if (auto* obj = factory.get(slider)) {
        obj->set_rect({24, 168, 280, 24});
    }
    if (auto* obj = factory.get(progress)) {
        obj->set_rect({24, 208, 280, 18});
    }

    Gui gui(canvas, factory, root);

    Gui::TileRenderConfig tile_cfg{};
    tile_cfg.tile_width = 128;
    tile_cfg.tile_height = 128;

    int win_w = screen_width;
    int win_h = screen_height;
    bool running = true;
    int mouse_x = 0;
    int mouse_y = 0;
    int progress_value = 0;

    while (running) {
        SDL_Event evt{};
        SDL_GetWindowSize(window, &win_w, &win_h);
        Viewport vp = compute_viewport(win_w, win_h, screen_width, screen_height);

        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            if (evt.type == SDL_EVENT_MOUSE_MOTION) {
                if (map_mouse(vp, evt.motion.x, evt.motion.y, mouse_x, mouse_y)) {
                    gui.dispatch_event(Event::mouse(Event::Type::MouseMove, mouse_x, mouse_y, 0));
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    if (map_mouse(vp, evt.button.x, evt.button.y, mouse_x, mouse_y)) {
                        gui.dispatch_event(Event::mouse(Event::Type::MouseDown, mouse_x, mouse_y, 1));
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    if (map_mouse(vp, evt.button.x, evt.button.y, mouse_x, mouse_y)) {
                        gui.dispatch_event(Event::mouse(Event::Type::MouseUp, mouse_x, mouse_y, 1));
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_WHEEL) {
                if (map_mouse(vp, mouse_x, mouse_y, mouse_x, mouse_y)) {
                    gui.dispatch_event(Event::wheel(mouse_x, mouse_y, static_cast<int>(evt.wheel.y)));
                }
            }
        }

        progress_value = (progress_value + 1) % 101;
        if (auto* obj = factory.get_progress(progress)) {
            obj->set_range(0, 100);
            obj->set_value(progress_value);
        }
        if (auto* obj = factory.get_slider(slider)) {
            obj->set_range(0, 100);
            obj->set_value(progress_value);
        }

        (void)gui.render_tiles(backend, tile_view, tile_cfg);

        SDL_UpdateTexture(texture, nullptr, fb.data(), static_cast<int>(DefaultFrameBuffer::stride_bytes));
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
