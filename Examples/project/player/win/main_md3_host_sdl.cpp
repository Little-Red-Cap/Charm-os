#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif

#include "main_md3_host_sdl.hpp"
#include <SDL3/SDL.h>

#if defined(_WIN32)
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#endif

namespace player_win_md3_host_sdl {
    namespace {
        struct PlayerInputBatch {
            InputEvent events[3]{};
            unsigned int count{0};

            void push(const InputEvent& event) noexcept {
                if (count < 3) {
                    events[count++] = event;
                }
            }
        };

        SDL_PixelFormat to_sdl_pixel_format(DisplayPixelFormat format) noexcept {
            switch (format) {
            case DisplayPixelFormat::RGB565:
                return SDL_PIXELFORMAT_RGB565;
            case DisplayPixelFormat::RGB888:
                return SDL_PIXELFORMAT_RGB24;
            case DisplayPixelFormat::ARGB8888:
                return SDL_PIXELFORMAT_ARGB8888;
            }
            return SDL_PIXELFORMAT_RGB24;
        }

        bool try_map_input_command(SDL_Keycode key, InputCommand& out) noexcept {
            switch (key) {
            case SDLK_UP:
                out = InputCommand::Up;
                return true;
            case SDLK_DOWN:
                out = InputCommand::Down;
                return true;
            case SDLK_LEFT:
                out = InputCommand::Left;
                return true;
            case SDLK_ESCAPE:
            case SDLK_BACKSPACE:
                out = InputCommand::Back;
                return true;
            case SDLK_RETURN:
                out = InputCommand::Enter;
                return true;
            case SDLK_SPACE:
                out = InputCommand::PlayToggle;
                return true;
            case SDLK_N:
                out = InputCommand::Next;
                return true;
            case SDLK_P:
                out = InputCommand::Prev;
                return true;
            case SDLK_M:
                out = InputCommand::Mode;
                return true;
            default:
                return false;
            }
        }

        bool command_has_button(InputCommand command) noexcept {
            switch (command) {
            case InputCommand::Up:
            case InputCommand::Down:
            case InputCommand::Left:
            case InputCommand::Back:
            case InputCommand::Enter:
                return true;
            default:
                return false;
            }
        }

        InputEvent make_pointer_event(unsigned int ms,
                                      bool pressed,
                                      float x,
                                      float y,
                                      PointerAction action) noexcept {
            return InputEvent{
                InputEventKind::Pointer,
                ms,
                action,
                PointerSample{pressed, x, y, 0},
                0.0f,
                InputCommand::Enter,
                false};
        }

        PlayerInputBatch translate_player_input(const SDL_Event& evt) noexcept {
            PlayerInputBatch batch{};
            const auto ms = static_cast<unsigned int>(SDL_GetTicks());
            switch (evt.type) {
            case SDL_EVENT_MOUSE_MOTION:
                batch.push(make_pointer_event(ms,
                                              false,
                                              evt.motion.x,
                                              evt.motion.y,
                                              PointerAction::Move));
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    batch.push(make_pointer_event(ms,
                                                  true,
                                                  evt.button.x,
                                                  evt.button.y,
                                                  PointerAction::Down));
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (evt.button.button == SDL_BUTTON_LEFT) {
                    batch.push(make_pointer_event(ms,
                                                  false,
                                                  evt.button.x,
                                                  evt.button.y,
                                                  PointerAction::Up));
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                float mx = 0.0f;
                float my = 0.0f;
                SDL_GetMouseState(&mx, &my);
                batch.push(InputEvent{
                    InputEventKind::Wheel,
                    ms,
                    PointerAction::Move,
                    PointerSample{false, mx, my, 0},
                    evt.wheel.y,
                    InputCommand::Enter,
                    false});
                break;
            }
            case SDL_EVENT_KEY_DOWN: {
                InputCommand command{};
                if (try_map_input_command(evt.key.key, command)) {
                    if (command_has_button(command)) {
                        batch.push(InputEvent{
                            InputEventKind::Button,
                            ms,
                            PointerAction::Move,
                            PointerSample{false, 0.0f, 0.0f, 0},
                            0.0f,
                            command,
                            true});
                    }
                    batch.push(InputEvent{
                        InputEventKind::Command,
                        ms,
                        PointerAction::Move,
                        PointerSample{false, 0.0f, 0.0f, 0},
                        0.0f,
                        command,
                        false});
                }
                break;
            }
            case SDL_EVENT_KEY_UP: {
                InputCommand command{};
                if (try_map_input_command(evt.key.key, command) && command_has_button(command)) {
                    batch.push(InputEvent{
                        InputEventKind::Button,
                        ms,
                        PointerAction::Move,
                        PointerSample{false, 0.0f, 0.0f, 0},
                        0.0f,
                        command,
                        false});
                }
                break;
            }
            default:
                break;
            }
            return batch;
        }
    }

    bool SdlHost::init(const char* title,
                       DisplayPixelFormat pixel_format,
                       int width,
                       int height,
                       const char*& error) noexcept {
        shutdown();
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
            error = SDL_GetError();
            return false;
        }
        sdl_ready_ = true;

        auto* window = SDL_CreateWindow(title ? title : "Charm Player",
                                        width,
                                        height,
                                        SDL_WINDOW_RESIZABLE);
        if (!window) {
            error = SDL_GetError();
            shutdown();
            return false;
        }
        window_ = window;

        auto* renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer) {
            error = SDL_GetError();
            shutdown();
            return false;
        }
        renderer_ = renderer;

        auto* texture = SDL_CreateTexture(renderer,
                                          to_sdl_pixel_format(pixel_format),
                                          SDL_TEXTUREACCESS_STREAMING,
                                          width,
                                          height);
        if (!texture) {
            error = SDL_GetError();
            shutdown();
            return false;
        }
        texture_ = texture;
        SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

        error = nullptr;
        return true;
    }

    void SdlHost::shutdown() noexcept {
        if (texture_) {
            SDL_DestroyTexture(static_cast<SDL_Texture*>(texture_));
            texture_ = nullptr;
        }
        if (renderer_) {
            SDL_DestroyRenderer(static_cast<SDL_Renderer*>(renderer_));
            renderer_ = nullptr;
        }
        if (window_) {
            SDL_DestroyWindow(static_cast<SDL_Window*>(window_));
            window_ = nullptr;
        }
        if (sdl_ready_) {
            SDL_Quit();
            sdl_ready_ = false;
        }
    }

    bool SdlHost::valid() const noexcept {
        return window_ != nullptr && renderer_ != nullptr && texture_ != nullptr;
    }

    bool SdlHost::present(const DisplaySurface& surface, DirtyRegion dirty) noexcept {
        (void)dirty;
        if (!renderer_ || !texture_ || !surface.pixels || surface.width <= 0 || surface.height <= 0
            || surface.stride_bytes == 0) {
            return false;
        }

        auto* renderer = static_cast<SDL_Renderer*>(renderer_);
        auto* texture = static_cast<SDL_Texture*>(texture_);
        SDL_UpdateTexture(texture, nullptr, surface.pixels, static_cast<int>(surface.stride_bytes));
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        return true;
    }

    PumpResult SdlHost::pump_events(void* ctx, InputHandlerFn handler) noexcept {
        PumpResult result{false, false, 0, 0};
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                result.quit = true;
                continue;
            }
            if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
                result.resized = true;
                result.width = static_cast<int>(evt.window.data1);
                result.height = static_cast<int>(evt.window.data2);
            }

            const auto batch = translate_player_input(evt);
            if (!handler) {
                continue;
            }
            for (unsigned int i = 0; i < batch.count; ++i) {
                handler(ctx, batch.events[i]);
            }
        }
        return result;
    }

    void SdlHost::delay_ms(unsigned int ms) noexcept {
        SDL_Delay(ms);
    }
}
