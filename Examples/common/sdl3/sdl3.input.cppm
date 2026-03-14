module;

#include <SDL3/SDL.h>

export module example.sdl3.input;

import example.pc.board_io;

export namespace example::sdl3 {
    struct InputMap {
        SDL_Scancode key0{SDL_SCANCODE_SPACE};
        SDL_Scancode wkup2{SDL_SCANCODE_TAB};
        SDL_Scancode enc_key{SDL_SCANCODE_RETURN};
        SDL_Scancode enc_key_alt{SDL_SCANCODE_KP_ENTER};
        SDL_Keycode enc_step_up{SDLK_UP};
        SDL_Keycode enc_step_down{SDLK_DOWN};
        SDL_Keycode enc_step_page_up{SDLK_PAGEUP};
        SDL_Keycode enc_step_page_down{SDLK_PAGEDOWN};
        SDL_Keycode quit_key{SDLK_ESCAPE};
    };

    inline bool pump_input(const InputMap& map = {}) noexcept {
        bool quit = false;
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                quit = true;
                continue;
            }
            if (evt.type == SDL_EVENT_KEY_DOWN) {
                if (evt.key.key == map.quit_key) {
                    quit = true;
                    continue;
                }
                if (evt.key.key == map.enc_step_up) {
                    example::pc::add_encoder_steps(-1);
                } else if (evt.key.key == map.enc_step_down) {
                    example::pc::add_encoder_steps(1);
                } else if (evt.key.key == map.enc_step_page_up) {
                    example::pc::add_encoder_steps(-4);
                } else if (evt.key.key == map.enc_step_page_down) {
                    example::pc::add_encoder_steps(4);
                }
            } else if (evt.type == SDL_EVENT_MOUSE_WHEEL) {
                if (evt.wheel.y != 0.0f) {
                    example::pc::add_encoder_steps(-static_cast<int>(evt.wheel.y));
                }
            }
        }

        SDL_PumpEvents();
        const bool* keys = SDL_GetKeyboardState(nullptr);
        example::pc::set_key(example::pc::KeyId::key0, keys && keys[map.key0]);
        example::pc::set_key(example::pc::KeyId::wkup2, keys && keys[map.wkup2]);
        const bool enc_key = keys && (keys[map.enc_key] || keys[map.enc_key_alt]);
        example::pc::set_key(example::pc::KeyId::enc_key, enc_key);
        return !quit;
    }
}
