module;

#include <cstddef>
#include <cstdint>
#include <optional>

export module player.input;

export namespace player {
    enum class PlayerInputEventKind : std::uint8_t {
        Pointer,
        Wheel,
        Button,
        Command,
    };

    enum class PlayerPointerAction : std::uint8_t {
        Down,
        Move,
        Up,
        Cancel,
    };

    struct PlayerPointerSample {
        bool down{false};
        float x{0.0f};
        float y{0.0f};
        std::uint8_t id{0};
    };

    enum class PlayerInputCommand : std::uint8_t {
        Up,
        Down,
        Left,
        Enter,
        Back,
        PlayToggle,
        Next,
        Prev,
        Mode,
    };

    struct PlayerInputEvent {
        PlayerInputEventKind kind{PlayerInputEventKind::Pointer};
        std::uint32_t ms{0};
        PlayerPointerAction pointer_action{PlayerPointerAction::Move};
        PlayerPointerSample pointer{};
        float wheel_y{0.0f};
        PlayerInputCommand command{PlayerInputCommand::Enter};
        bool button_pressed{false};

        [[nodiscard]] static constexpr PlayerInputEvent make_pointer(std::uint32_t ms,
                                                                      PlayerPointerAction action,
                                                                      PlayerPointerSample sample) noexcept {
            return PlayerInputEvent{
                .kind = PlayerInputEventKind::Pointer,
                .ms = ms,
                .pointer_action = action,
                .pointer = sample,
            };
        }

        [[nodiscard]] static constexpr PlayerInputEvent make_wheel(std::uint32_t ms,
                                                                    float x,
                                                                    float y,
                                                                    float delta_y) noexcept {
            return PlayerInputEvent{
                .kind = PlayerInputEventKind::Wheel,
                .ms = ms,
                .pointer = PlayerPointerSample{false, x, y, 0},
                .wheel_y = delta_y,
            };
        }

        [[nodiscard]] static constexpr PlayerInputEvent make_command(std::uint32_t ms,
                                                                      PlayerInputCommand value) noexcept {
            return PlayerInputEvent{
                .kind = PlayerInputEventKind::Command,
                .ms = ms,
                .command = value,
            };
        }

        [[nodiscard]] static constexpr PlayerInputEvent make_button(std::uint32_t ms,
                                                                     PlayerInputCommand value,
                                                                     bool pressed) noexcept {
            return PlayerInputEvent{
                .kind = PlayerInputEventKind::Button,
                .ms = ms,
                .command = value,
                .button_pressed = pressed,
            };
        }
    };

    struct PlayerTouchSample {
        bool down{false};
        float x{0.0f};
        float y{0.0f};
        std::uint8_t id{0};
        std::uint32_t ms{0};
    };

    struct PlayerTouchSampleSource {
        using ReadFn = std::optional<PlayerTouchSample> (*)(void* ctx) noexcept;

        void* ctx{nullptr};
        ReadFn read_fn{nullptr};

        [[nodiscard]] std::optional<PlayerTouchSample> read() const noexcept {
            return read_fn ? read_fn(ctx) : std::nullopt;
        }
    };

    struct PlayerTouchAdapterState {
        bool initialized{false};
        bool last_down{false};
        float last_x{0.0f};
        float last_y{0.0f};
        std::uint8_t last_id{0};
    };

    struct PlayerInputEventBatch {
        static constexpr std::size_t capacity = 4;

        PlayerInputEvent events[capacity]{};
        std::size_t count{0};

        void push(const PlayerInputEvent& event) noexcept {
            if (count < capacity) {
                events[count++] = event;
            }
        }
    };

    inline std::optional<PlayerInputEvent> make_player_input_from_touch_sample(
        PlayerTouchAdapterState& state,
        const PlayerTouchSample& sample) noexcept {
        PlayerPointerAction action = PlayerPointerAction::Move;
        if (!state.initialized) {
            if (!sample.down) {
                state.last_down = false;
                state.last_x = sample.x;
                state.last_y = sample.y;
                state.last_id = sample.id;
                state.initialized = true;
                return std::nullopt;
            }
            action = PlayerPointerAction::Down;
        } else if (sample.down && !state.last_down) {
            action = PlayerPointerAction::Down;
        } else if (!sample.down && state.last_down) {
            action = PlayerPointerAction::Up;
        } else if (sample.down) {
            action = PlayerPointerAction::Move;
        } else {
            state.last_down = false;
            state.last_x = sample.x;
            state.last_y = sample.y;
            state.last_id = sample.id;
            state.initialized = true;
            return std::nullopt;
        }

        state.last_down = sample.down;
        state.last_x = sample.x;
        state.last_y = sample.y;
        state.last_id = sample.id;
        state.initialized = true;

        return PlayerInputEvent::make_pointer(sample.ms,
                                              action,
                                              PlayerPointerSample{sample.down, sample.x, sample.y, sample.id});
    }

    inline PlayerInputEventBatch read_player_touch_events(PlayerTouchSampleSource source,
                                                          PlayerTouchAdapterState& state) noexcept {
        PlayerInputEventBatch out{};
        while (out.count < PlayerInputEventBatch::capacity) {
            auto sample = source.read();
            if (!sample) {
                break;
            }
            auto event = make_player_input_from_touch_sample(state, *sample);
            if (event) {
                out.push(*event);
            }
        }
        return out;
    }
}
