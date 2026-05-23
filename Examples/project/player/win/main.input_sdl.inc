    struct SdlPlayerInputBatch {
        std::array<player::PlayerInputEvent, 3> events{};
        std::size_t count{0};

        void push(const player::PlayerInputEvent& event) noexcept {
            if (count < events.size()) {
                events[count++] = event;
            }
        }
    };

    std::optional<player::PlayerInputCommand> map_player_input_command(SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_UP: return player::PlayerInputCommand::Up;
        case SDLK_DOWN: return player::PlayerInputCommand::Down;
        case SDLK_LEFT: return player::PlayerInputCommand::Left;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            return player::PlayerInputCommand::Back;
        case SDLK_RETURN: return player::PlayerInputCommand::Enter;
        case SDLK_SPACE: return player::PlayerInputCommand::PlayToggle;
        case SDLK_N: return player::PlayerInputCommand::Next;
        case SDLK_P: return player::PlayerInputCommand::Prev;
        case SDLK_M: return player::PlayerInputCommand::Mode;
        default:
            break;
        }
        return std::nullopt;
    }

    bool player_input_command_has_button(player::PlayerInputCommand command) noexcept {
        switch (command) {
        case player::PlayerInputCommand::Up:
        case player::PlayerInputCommand::Down:
        case player::PlayerInputCommand::Left:
        case player::PlayerInputCommand::Back:
        case player::PlayerInputCommand::Enter:
            return true;
        default:
            break;
        }
        return false;
    }

    player::PlayerInputEvent make_sdl_pointer_event(std::uint32_t ms,
                                                    bool pressed,
                                                    float x,
                                                    float y,
                                                    player::PlayerPointerAction action) noexcept {
        return player::PlayerInputEvent::make_pointer(
            ms,
            action,
            player::PlayerPointerSample{pressed, x, y, 0});
    }

    SdlPlayerInputBatch translate_sdl_player_input(const SDL_Event& evt) noexcept {
        SdlPlayerInputBatch batch{};
        const auto ms = SDL_GetTicks();
        switch (evt.type) {
        case SDL_EVENT_MOUSE_MOTION:
            batch.push(make_sdl_pointer_event(ms,
                                              false,
                                              evt.motion.x,
                                              evt.motion.y,
                                              player::PlayerPointerAction::Move));
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (evt.button.button == SDL_BUTTON_LEFT) {
                batch.push(make_sdl_pointer_event(ms,
                                                  true,
                                                  evt.button.x,
                                                  evt.button.y,
                                                  player::PlayerPointerAction::Down));
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (evt.button.button == SDL_BUTTON_LEFT) {
                batch.push(make_sdl_pointer_event(ms,
                                                  false,
                                                  evt.button.x,
                                                  evt.button.y,
                                                  player::PlayerPointerAction::Up));
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            float mx = 0.0f;
            float my = 0.0f;
            SDL_GetMouseState(&mx, &my);
            batch.push(player::PlayerInputEvent::make_wheel(ms, mx, my, evt.wheel.y));
            break;
        }
        case SDL_EVENT_KEY_DOWN:
            if (auto command = map_player_input_command(evt.key.key)) {
                if (player_input_command_has_button(*command)) {
                    batch.push(player::PlayerInputEvent::make_button(ms, *command, true));
                }
                batch.push(player::PlayerInputEvent::make_command(ms, *command));
            }
            break;
        case SDL_EVENT_KEY_UP:
            if (auto command = map_player_input_command(evt.key.key);
                command && player_input_command_has_button(*command)) {
                batch.push(player::PlayerInputEvent::make_button(ms, *command, false));
            }
            break;
        default:
            break;
        }
        return batch;
    }
