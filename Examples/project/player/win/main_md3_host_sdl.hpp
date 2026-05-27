#pragma once

namespace player_win_md3_host_sdl {
    enum class DisplayPixelFormat : unsigned char {
        RGB565,
        RGB888,
        ARGB8888,
    };

    struct DirtyRegion {
        int x;
        int y;
        int w;
        int h;
    };

    struct DisplaySurface {
        void* pixels;
        int width;
        int height;
        unsigned long long stride_bytes;
        DisplayPixelFormat pixel_format;
    };

    enum class InputEventKind : unsigned char {
        Pointer,
        Wheel,
        Button,
        Command,
    };

    enum class PointerAction : unsigned char {
        Down,
        Move,
        Up,
        Cancel,
    };

    struct PointerSample {
        bool down;
        float x;
        float y;
        unsigned char id;
    };

    enum class InputCommand : unsigned char {
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

    struct InputEvent {
        InputEventKind kind;
        unsigned int ms;
        PointerAction pointer_action;
        PointerSample pointer;
        float wheel_y;
        InputCommand command;
        bool button_pressed;
    };

    using InputHandlerFn = void (*)(void* ctx, const InputEvent&) noexcept;

    struct PumpResult {
        bool quit;
        bool resized;
        int width;
        int height;
    };

    class SdlHost {
    public:
        bool init(const char* title,
                  DisplayPixelFormat pixel_format,
                  int width,
                  int height,
                  const char*& error) noexcept;

        void shutdown() noexcept;

        bool valid() const noexcept;
        bool present(const DisplaySurface& surface, DirtyRegion dirty) noexcept;
        PumpResult pump_events(void* ctx, InputHandlerFn handler) noexcept;

        static void delay_ms(unsigned int ms) noexcept;

    private:
        void* window_{};
        void* renderer_{};
        void* texture_{};
        bool sdl_ready_{false};
    };
}
