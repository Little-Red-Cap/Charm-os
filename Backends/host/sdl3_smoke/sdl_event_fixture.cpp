#include <SDL3/SDL.h>

#include <limits>

namespace {
    SDL_Window* g_foreign_window = nullptr;

    bool push_event(SDL_Event event) noexcept {
        return SDL_PushEvent(&event);
    }
}

bool queue_sdl_input_sequence() noexcept {
    SDL_Event pointer_down{};
    pointer_down.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    pointer_down.button.button = SDL_BUTTON_LEFT;
    pointer_down.button.x = 32.0F;
    pointer_down.button.y = 24.0F;

    SDL_Event pointer_move{};
    pointer_move.type = SDL_EVENT_MOUSE_MOTION;
    pointer_move.motion.x = 40.0F;
    pointer_move.motion.y = 32.0F;

    SDL_Event key_down{};
    key_down.type = SDL_EVENT_KEY_DOWN;
    key_down.key.key = SDLK_RETURN;

    SDL_Event wheel{};
    wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.y = 2.0F;
    wheel.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;

    SDL_Event key_up{};
    key_up.type = SDL_EVENT_KEY_UP;
    key_up.key.key = SDLK_RETURN;

    SDL_Event pointer_up{};
    pointer_up.type = SDL_EVENT_MOUSE_BUTTON_UP;
    pointer_up.button.button = SDL_BUTTON_LEFT;
    pointer_up.button.x = 40.0F;
    pointer_up.button.y = 32.0F;

    SDL_Event quit{};
    quit.type = SDL_EVENT_QUIT;

    SDL_Event invalid_wheel{};
    invalid_wheel.type = SDL_EVENT_MOUSE_WHEEL;
    invalid_wheel.wheel.y = std::numeric_limits<float>::quiet_NaN();

    return push_event(pointer_down)
        && push_event(pointer_move)
        && push_event(key_down)
        && push_event(wheel)
        && push_event(key_up)
        && push_event(pointer_up)
        && push_event(invalid_wheel)
        && push_event(quit);
}

bool create_sdl_foreign_window() noexcept {
    if (g_foreign_window) {
        return false;
    }
    g_foreign_window = SDL_CreateWindow(
        "Charm Host SDL3 foreign window",
        16,
        16,
        SDL_WINDOW_HIDDEN);
    return g_foreign_window != nullptr;
}

void destroy_sdl_foreign_window() noexcept {
    if (g_foreign_window) {
        SDL_DestroyWindow(g_foreign_window);
        g_foreign_window = nullptr;
    }
}

bool queue_sdl_window_close_event() noexcept {
    int window_count = 0;
    SDL_Window** windows = SDL_GetWindows(&window_count);
    if (!windows || window_count != 1) {
        SDL_free(windows);
        return false;
    }
    const SDL_WindowID window_id = SDL_GetWindowID(windows[0]);
    SDL_free(windows);
    if (window_id == 0U) {
        return false;
    }

    SDL_Event close{};
    close.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
    close.window.windowID = window_id;
    return push_event(close);
}
