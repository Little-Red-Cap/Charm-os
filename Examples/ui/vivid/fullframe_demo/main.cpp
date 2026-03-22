#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>

import charm.core.event;
import charm.core.config;
import charm.gfx.canvas;
import charm.gfx.framebuffer;
import charm.ui.scene;
import out.api;

namespace {
    struct StderrSink {
        out::result<std::size_t> write(out::bytes b) noexcept {
            if (b.size() == 0) return out::ok<std::size_t>(0u);
            const auto n = std::fwrite(b.data(), 1, b.size(), stderr);
            std::fflush(stderr);
            return out::ok(static_cast<std::size_t>(n));
        }
    };

    StderrSink& stderr_sink() noexcept {
        static StderrSink sink{};
        return sink;
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

}

int main() {
    auto& err = stderr_sink();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        (void)out::error<"SDL_Init failed: {}">(err, SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Vivid FullFrame Demo", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
    if (!window) {
        (void)out::error<"SDL_CreateWindow failed: {}">(err, SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        (void)out::error<"SDL_CreateRenderer failed: {}">(err, SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                             screen_width, screen_height);
    if (!texture) {
        (void)out::error<"SDL_CreateTexture failed: {}">(err, SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    DefaultFrameBuffer fb{};
    DefaultCanvas canvas{fb};
    ::ui::scene::Scene scene{canvas};
    auto builder = scene.begin();
    auto root = builder.create_container();
    builder.set_rect(root, {0, 0, screen_width, screen_height});

    auto title = builder.create_label_static("FullFrame Dirty Demo");
    auto btn = builder.create_button_static("Press");
    auto sw = builder.create_switch();
    auto slider = builder.create_slider();
    auto progress = builder.create_progress();

    builder.link(root, title);
    builder.link(root, btn);
    builder.link(root, sw);
    builder.link(root, slider);
    builder.link(root, progress);

    builder.set_rect(title, {24, 16, screen_width - 48, 24});
    builder.set_rect(btn, {24, 60, 160, 40});
    builder.set_rect(sw, {24, 112, 96, 32});
    builder.set_rect(slider, {24, 168, 280, 24});
    builder.set_rect(progress, {24, 208, 280, 18});
    builder.set_range(slider, 0, 100);
    builder.set_range(progress, 0, 100);
    builder.set_root(root);
    scene.end(builder);
    auto access = scene.access();

    int win_w = screen_width;
    int win_h = screen_height;
    bool running = true;
    int mouse_x = 0;
    int mouse_y = 0;
    int progress_value = 0;
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
                    scene.dispatch_event(Event::mouse(Event::Type::MouseMove, mouse_x, mouse_y, 0));
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    if (map_mouse(vp, evt.button.x, evt.button.y, mouse_x, mouse_y)) {
                        scene.dispatch_event(Event::mouse(Event::Type::MouseDown, mouse_x, mouse_y, 1));
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP) {
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    if (map_mouse(vp, evt.button.x, evt.button.y, mouse_x, mouse_y)) {
                        scene.dispatch_event(Event::mouse(Event::Type::MouseUp, mouse_x, mouse_y, 1));
                    }
                }
            } else if (evt.type == SDL_EVENT_MOUSE_WHEEL) {
                if (map_mouse(vp, mouse_x, mouse_y, mouse_x, mouse_y)) {
                    scene.dispatch_event(Event::wheel(mouse_x, mouse_y, static_cast<int>(evt.wheel.y)));
                }
            }
        }

        progress_value = (progress_value + 1) % 101;
        access.set_value(progress, progress_value);
        access.set_value(slider, progress_value);

        scene.render();

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
