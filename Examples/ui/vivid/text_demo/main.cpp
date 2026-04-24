#include <SDL3/SDL.h>
#include <cstdio>

import charm.core.config;
import charm.gfx.canvas;
import charm.gfx.color;
import charm.gfx.text_box;
import charm.font.typography;
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_sc_16;

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
}

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Vivid Text Demo", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
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

    static DefaultFrameBuffer fb{};
    static DefaultCanvas canvas{fb};

    const int pad = 20;
    const rgba text_color{220, 228, 242, 255};
    const rgba title_color{236, 238, 246, 255};
    const Font& ascii_font = font_noto_ascii_16;
    const Font& cjk_font = font_noto_sc_16;
    const char* title = "Text Rendering Probe";
    const char* ascii = "ASCII: The quick brown fox jumps over the lazy dog. 0123456789";
    const char* cjk = "CJK: 你好，文本回退测试。中文字符是否可见？";
    const char* mixed = "Mixed: FELT 路 FLAC | 进度 65% | 状态 Ready\n第二行测试换行。";
    const char* rich = "Rich: [b]bold[/b] [mono]Code 123[/mono]";

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
        }

        canvas.clear(rgba{18, 20, 28, 255});
        int y = pad;
        Rect title_rect{pad, y, screen_width - pad * 2, 28};
        draw_text_box(canvas, title_rect, title, title_color, ascii_font,
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::None);
        y += 40;

        Rect ascii_rect{pad, y, screen_width - pad * 2, 54};
        draw_text_box(canvas, ascii_rect, ascii, text_color, ascii_font,
                      TextAlignH::Left, TextAlignV::Top, TextWrap::Word, TextEllipsis::None);
        y += 70;

        Rect cjk_rect{pad, y, screen_width - pad * 2, 54};
        draw_text_box(canvas, cjk_rect, cjk, text_color, cjk_font,
                      TextAlignH::Left, TextAlignV::Top, TextWrap::Word, TextEllipsis::None);
        y += 70;

        Rect mixed_rect{pad, y, screen_width - pad * 2, 90};
        draw_text_box(canvas, mixed_rect, mixed, text_color, ascii_font,
                      TextAlignH::Left, TextAlignV::Top, TextWrap::Word, TextEllipsis::None);
        y += 110;

        Rect rich_rect{pad, y, screen_width - pad * 2, 60};
        draw_text_box(canvas, rich_rect, rich, text_color, ascii_font,
                      TextAlignH::Left, TextAlignV::Top, TextWrap::Word, TextEllipsis::None);

        SDL_UpdateTexture(texture, nullptr, canvas.data(), screen_width * 3);
        SDL_SetRenderDrawColor(renderer, 8, 10, 14, 255);
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
