module;
#include <cstdint>

export module charm.core.soa_kernel:types;

export import charm.core.handle;

export
enum class SoaNodeFlag : std::uint8_t {
    Used = 1 << 0,
    Visible = 1 << 1,
    Enabled = 1 << 2,
    Focusable = 1 << 3,
    HitTest = 1 << 4,
    ClipChildren = 1 << 5
};

export
enum class SoaStateFlag : std::uint8_t {
    Hovered = 1 << 0,
    Pressed = 1 << 1,
    Focused = 1 << 2
};

export
enum class SoaStateMask : std::uint8_t {
    Enabled = 1 << 0,
    Hovered = 1 << 1,
    Pressed = 1 << 2,
    Focused = 1 << 3
};

export
enum class SemanticRole : std::uint8_t {
    None = 0,
    Button,
    ListItem,
    Text,
    Container,
};

export
struct SemanticFocusSnapshot {
    WidgetHandle handle{};
    const char* id{""};
    const char* role{"none"};
    const char* label{""};
    bool found{false};
    bool focusable{false};
};

export
inline const char* semantic_role_name(SemanticRole role) noexcept {
    switch (role) {
    case SemanticRole::None:
        return "none";
    case SemanticRole::Button:
        return "button";
    case SemanticRole::ListItem:
        return "list_item";
    case SemanticRole::Text:
        return "text";
    case SemanticRole::Container:
        return "container";
    }
    return "unknown";
}

export
enum class ScrollBarOrientation : std::uint8_t {
    Horizontal = 0,
    Vertical = 1
};

export
enum class TableViewHeaderStyle : std::uint8_t {
    Default = 0,
    Accent = 1,
    Muted = 2
};

export
enum class TableViewColDividerStyle : std::uint8_t {
    None = 0,
    HeaderOnly = 1,
    BodyOnly = 2,
    Full = 3
};

export
struct StateCompact {
    std::uint8_t bits{0};
    std::uint8_t variant{0};

    bool enabled() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Enabled)) != 0;
    }

    bool hovered() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Hovered)) != 0;
    }

    bool pressed() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Pressed)) != 0;
    }

    bool focused() const noexcept {
        return (bits & static_cast<std::uint8_t>(SoaStateMask::Focused)) != 0;
    }
};
