    std::optional<input::Button> map_nav_button(SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_UP: return input::Button::Up;
        case SDLK_DOWN: return input::Button::Down;
        case SDLK_RETURN: return input::Button::Enter;
        case SDLK_ESCAPE: return input::Button::Back;
        case SDLK_BACKSPACE: return input::Button::Back;
        default:
            break;
        }
        return std::nullopt;
    }

    std::optional<player::UiKey> map_ui_key(SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_UP: return player::UiKey::Up;
        case SDLK_DOWN: return player::UiKey::Down;
        case SDLK_RETURN: return player::UiKey::Enter;
        case SDLK_SPACE: return player::UiKey::PlayToggle;
        case SDLK_N: return player::UiKey::Next;
        case SDLK_P: return player::UiKey::Prev;
        case SDLK_M: return player::UiKey::Mode;
        default:
            break;
        }
        return std::nullopt;
    }

    input::RawInputEvent make_pointer_event(bool pressed,
                                            float x,
                                            float y,
                                            input::PointerAction action) noexcept {
        input::RawInputEvent raw{};
        raw.type = input::RawInputEventType::Pointer;
        raw.ms = SDL_GetTicks();
        raw.pointer = input::PointerRaw{pressed,
                                        static_cast<std::int16_t>(x),
                                        static_cast<std::int16_t>(y),
                                        0};
        raw.pointer_action = action;
        return raw;
    }

    input::RawInputEvent make_button_event(input::Button button, bool pressed) noexcept {
        input::RawInputEvent raw{};
        raw.type = input::RawInputEventType::Button;
        raw.ms = SDL_GetTicks();
        raw.button = button;
        raw.pressed = pressed;
        return raw;
    }

    bool dispatch_sdl_event(::ui::scene::Scene& scene, player::App& app, PlayerUiContext& ctx, const SDL_Event& evt) {
        switch (evt.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            input::RawInputEvent raw =
                make_pointer_event(false, evt.motion.x, evt.motion.y, input::PointerAction::Move);
            app.dispatch_raw_input(scene, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (evt.button.button != SDL_BUTTON_LEFT) return true;
            input::RawInputEvent raw =
                make_pointer_event(true, evt.button.x, evt.button.y, input::PointerAction::Down);
            app.dispatch_raw_input(scene, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (evt.button.button != SDL_BUTTON_LEFT) return true;
            input::RawInputEvent raw =
                make_pointer_event(false, evt.button.x, evt.button.y, input::PointerAction::Up);
            app.dispatch_raw_input(scene, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            {
                float mx = 0.0f;
                float my = 0.0f;
                SDL_GetMouseState(&mx, &my);
                scene.dispatch_event(Event::wheel(static_cast<int>(mx),
                                                  static_cast<int>(my),
                                                  evt.wheel.y));
            }
            ctx.process_input_events();
            return true;
        case SDL_EVENT_KEY_DOWN:
            if (auto k = map_ui_key(evt.key.key)) {
                ctx.handle_key_action(*k);
            }
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw = make_button_event(*b, true);
                app.dispatch_raw_input(scene, ctx, raw);
            }
            return true;
        case SDL_EVENT_KEY_UP:
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw = make_button_event(*b, false);
                app.dispatch_raw_input(scene, ctx, raw);
            }
            return true;
        default:
            return false;
        }
    }
