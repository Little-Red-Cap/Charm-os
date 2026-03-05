module;

#include <cstdint>

export module charm.core.event;

export class Event {
public:
    enum class GesturePhase {
        Begin = 0,
        Update = 1,
        End = 2
    };

    enum class Type {
        HoverEnter,
        HoverLeave,
        MouseDown,
        MouseUp,
        MouseMove,
        MouseWheel,
        Click,
        DragStart,
        DragMove,
        DragEnd,
        GestureSwipe,
        GesturePinch,
        FocusIn,
        FocusOut,
        KeyDown,
        KeyUp,
        Cancel,
    } type;

    std::uint32_t ms = 0;
    int x = 0;
    int y = 0;
    int dx = 0;
    int dy = 0;
    int button = 0;
    int wheel_y = 0;
    GesturePhase gesture_phase = GesturePhase::Begin;
    float scale = 1.0f;
    int ch = 0; // UTF-32 codepoint when available

    enum class Key {
        Unknown,
        Tab,
        Enter,
        Space,
        Backspace,
        Escape,
        Up,
        Down,
        Left,
        Right
    };
    Key key_code = Key::Unknown;

    static Event mouse(Type t, int px, int py, int btn = 0, std::uint32_t ms = 0) noexcept {
        return Event(t, px, py, 0, 0, btn, ms);
    }

    static Event wheel(int px, int py, int dy, std::uint32_t ms = 0) noexcept {
        Event e(Type::MouseWheel, px, py, 0, 0, 0, ms);
        e.wheel_y = dy;
        return e;
    }

    static Event key(Type t, Key keycode, std::uint32_t ms = 0) noexcept {
        Event e(t, 0, 0, 0, 0, 0, ms);
        e.key_code = keycode;
        return e;
    }

    static Event text(int codepoint, std::uint32_t ms = 0) noexcept {
        Event e(Type::KeyDown, 0, 0, 0, 0, 0, ms);
        e.ch = codepoint;
        return e;
    }

    static Event drag(Type t, int px, int py, int ddx, int ddy, int btn = 0, std::uint32_t ms = 0) noexcept {
        return Event(t, px, py, ddx, ddy, btn, ms);
    }

    static Event gesture(Type t, int px, int py, int ddx, int ddy, GesturePhase phase,
                         float scale_value = 1.0f, std::uint32_t ms = 0) noexcept {
        Event e(t, px, py, ddx, ddy, 0, ms);
        e.gesture_phase = phase;
        e.scale = scale_value;
        return e;
    }

    Event(Type t, int px = 0, int py = 0, int ddx = 0, int ddy = 0, int btn = 0,
          std::uint32_t ms_in = 0)
        : type(t), ms(ms_in), x(px), y(py), dx(ddx), dy(ddy), button(btn) {}
};

export
struct Callback {
    using Fn = void(*)(void*);
    Fn    fn{nullptr};
    void* ctx{nullptr};

    constexpr void operator()() const noexcept {
        if (fn) fn(ctx);
    }

    constexpr explicit operator bool() const noexcept { return fn != nullptr; }
};
